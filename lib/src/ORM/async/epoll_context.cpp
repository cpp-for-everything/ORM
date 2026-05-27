#include "ORM/async/io_context.hpp"

#if defined(__linux__)

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

namespace orm {

    // Linux reactor backed by epoll(7) in level-triggered + EPOLLONESHOT mode.
    //
    // The semantics intentionally mirror the macOS kqueue backend:
    //   - register_poll(fd, op) installs a one-shot interest for the requested
    //     direction; epoll auto-disarms the interest once the event fires, so
    //     the same fd can be re-registered later with a different op.
    //   - watch_readable/watch_writable suspend the calling coroutine; the
    //     reactor resumes it when the kernel signals readiness.
    //   - post() and schedule() are independent of the epoll fd; an eventfd
    //     is used to wake the reactor when callbacks arrive cross-thread.
    class EpollContext : public IoContext
    {
        int epfd_ = -1;
        int wakeup_fd_ = -1; // eventfd for post()/schedule() cross-thread wakeup
        std::vector<std::thread> workers_;
        std::atomic<bool> stopped_{false};
        size_t thread_count_;

        std::mutex callback_mutex_;
        std::queue<std::function<void()>> callbacks_;

    public:
        explicit EpollContext(size_t thread_count)
            : thread_count_(thread_count)
        {
            epfd_ = epoll_create1(EPOLL_CLOEXEC);
            if (epfd_ < 0)
            {
                throw std::runtime_error(
                    std::string("epoll_create1() failed: ") + std::strerror(errno));
            }

            wakeup_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
            if (wakeup_fd_ < 0)
            {
                int saved = errno;
                close(epfd_);
                epfd_ = -1;
                throw std::runtime_error(
                    std::string("eventfd() failed: ") + std::strerror(saved));
            }

            epoll_event ev{};
            ev.events = EPOLLIN;
            ev.data.ptr = nullptr; // sentinel: nullptr op → just drain
            if (epoll_ctl(epfd_, EPOLL_CTL_ADD, wakeup_fd_, &ev) < 0)
            {
                int saved = errno;
                close(wakeup_fd_);
                close(epfd_);
                throw std::runtime_error(
                    std::string("epoll_ctl(wakeup) failed: ") +
                    std::strerror(saved));
            }
        }

        ~EpollContext() override
        {
            stop();

            // Wake any blocked epoll_wait so workers can observe stopped_.
            wake();

            for (auto& worker : workers_)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }

            if (wakeup_fd_ >= 0) close(wakeup_fd_);
            if (epfd_ >= 0) close(epfd_);
        }

        void run() override
        {
            stopped_ = false;

            for (size_t i = 0; i < thread_count_; ++i)
            {
                workers_.emplace_back([this] { worker_thread(); });
            }

            for (auto& worker : workers_)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }
            workers_.clear();
        }

        void run_one() override
        {
            process_events();
            process_callbacks();
        }

        void stop() override
        {
            stopped_ = true;
            wake();
        }

        [[nodiscard]] bool stopped() const noexcept override
        {
            return stopped_;
        }

        void post(std::function<void()> callback) override
        {
            {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                callbacks_.push(std::move(callback));
            }
            wake();
        }

        void schedule(
            std::chrono::milliseconds delay,
            std::function<void()> callback) override
        {
            std::thread(
                [this, delay, cb = std::move(callback)]() mutable
                {
                    std::this_thread::sleep_for(delay);
                    post(std::move(cb));
                })
                .detach();
        }

        // Reactor: register fd for readiness notification only.
        //
        // Uses EPOLLONESHOT — the kernel auto-disarms the interest after the
        // event fires, mirroring kqueue's EV_ONESHOT and io_uring's one-shot
        // IORING_OP_POLL_ADD.
        //
        // fd-reuse correctness: we try EPOLL_CTL_MOD first and fall back to
        // EPOLL_CTL_ADD on ENOENT. This handles both the first-registration
        // case (no prior entry — ENOENT → ADD) and the fd-reuse case (a new
        // MYSQL handle, libpq connection, or Redis context that happens to
        // get the same fd value the kernel just reclaimed from a closed
        // descriptor — the kernel removed the old epoll entry on close so
        // MOD fails with ENOENT and ADD then succeeds). Tracking fds in a
        // separate set was insufficient: it could not see kernel-side
        // close-time auto-removals.
        void register_poll(int fd, PollOperation* op) override
        {
            epoll_event ev{};
            ev.events =
                ((op->interest == PollInterest::Read) ? EPOLLIN : EPOLLOUT) |
                EPOLLONESHOT;
            ev.data.ptr = op;

            if (::epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev) == 0) return;
            if (errno == ENOENT)
            {
                // Either first time we've seen this fd, or the kernel
                // already auto-removed the entry on close. ADD it fresh.
                if (::epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) == 0) return;
                // If a racing thread added it between our MOD and ADD,
                // retry MOD once more.
                if (errno == EEXIST)
                {
                    ::epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev);
                }
            }
        }

    private:
        void wake() noexcept
        {
            if (wakeup_fd_ < 0) return;
            uint64_t one = 1;
            // Best-effort; an EAGAIN here just means the counter is non-zero
            // and a previous wake is still pending — fine for our purpose.
            ssize_t written = ::write(wakeup_fd_, &one, sizeof(one));
            (void)written;
        }

        void worker_thread()
        {
            while (!stopped_)
            {
                process_events();
                process_callbacks();
            }
        }

        void process_events()
        {
            epoll_event events[64];

            int n = epoll_wait(epfd_, events, 64, 50); // 50 ms cadence

            if (n < 0)
            {
                if (errno == EINTR) return;
                return;
            }

            for (int i = 0; i < n; ++i)
            {
                auto& ev = events[i];
                auto* op = static_cast<PollOperation*>(ev.data.ptr);

                if (!op)
                {
                    // Wakeup eventfd; drain it.
                    uint64_t buf = 0;
                    ssize_t r = ::read(wakeup_fd_, &buf, sizeof(buf));
                    (void)r;
                    continue;
                }

                // Pure reactor: just resume the coroutine.
                if (op->continuation)
                {
                    op->continuation.resume();
                }
            }
        }

        void process_callbacks()
        {
            std::function<void()> callback;
            {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                if (callbacks_.empty()) return;
                callback = std::move(callbacks_.front());
                callbacks_.pop();
            }
            if (callback)
            {
                callback();
            }
        }
    };

    // Factory
    auto IoContext::create(size_t thread_count) -> std::unique_ptr<IoContext>
    {
        return std::make_unique<EpollContext>(thread_count);
    }

} // namespace orm

#endif // __linux__
