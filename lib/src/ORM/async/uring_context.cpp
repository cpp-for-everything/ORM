#include "ORM/async/io_context.hpp"

#if defined(__linux__) && defined(ORM_HAS_LIBURING)

#include <liburing.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <poll.h>

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

    // Linux reactor backed by io_uring(7) — single-issuer submission, one-shot
    // poll completions for fd readiness, a multishot poll on an eventfd for
    // cross-thread wake-ups.
    //
    // The semantics intentionally mirror the kqueue / epoll backends:
    //   - register_poll(fd, op) installs IORING_OP_POLL_ADD; once the kernel
    //     delivers the readiness CQE, the entry is gone (one-shot).
    //   - watch_readable/watch_writable suspend the calling coroutine; the
    //     reactor thread resumes it when the CQE arrives.
    //   - post()/schedule() append work to a callback queue and write() to
    //     the eventfd, which wakes the reactor out of io_uring_wait_cqe.
    //
    // io_uring submission rings are NOT thread-safe; register_poll may be
    // called from any thread, so all io_uring_get_sqe / io_uring_submit calls
    // are serialised under sq_mutex_. The reactor itself is single-threaded
    // (with optional callback-drain helpers) to avoid CQE races; this matches
    // the iocp_context.cpp pattern on Windows and keeps the per-platform
    // contract uniform.
    class UringContext : public IoContext
    {
        // Sentinel non-null pointer used as user_data for the wakeup poll
        // entry so we can distinguish it from regular PollOperation* values
        // without risking a real-allocation address collision.
        static char wakeup_token_;

        struct io_uring ring_{};
        int wakeup_fd_ = -1;

        std::vector<std::thread> workers_;
        std::atomic<bool> stopped_{false};
        size_t thread_count_;

        std::mutex sq_mutex_;

        std::mutex callback_mutex_;
        std::queue<std::function<void()>> callbacks_;

    public:
        explicit UringContext(size_t thread_count)
            : thread_count_(thread_count)
        {
            if (int rc = ::io_uring_queue_init(/*entries=*/256, &ring_, 0); rc < 0)
            {
                throw std::runtime_error(
                    std::string("io_uring_queue_init failed: ") +
                    std::strerror(-rc));
            }

            wakeup_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
            if (wakeup_fd_ < 0)
            {
                int saved = errno;
                ::io_uring_queue_exit(&ring_);
                throw std::runtime_error(
                    std::string("eventfd() failed: ") + std::strerror(saved));
            }

            arm_wakeup_poll();
        }

        ~UringContext() override
        {
            stop();
            wake();

            for (auto& worker : workers_)
            {
                if (worker.joinable()) worker.join();
            }

            if (wakeup_fd_ >= 0) ::close(wakeup_fd_);
            ::io_uring_queue_exit(&ring_);
        }

        void run() override
        {
            stopped_ = false;

            // One reactor worker drains CQEs; additional workers (if
            // requested) only drain the callback queue, matching the
            // iocp_context.cpp pattern.
            workers_.emplace_back([this] { worker_thread(); });
            for (size_t i = 1; i < thread_count_; ++i)
            {
                workers_.emplace_back([this] { callback_drain_thread(); });
            }

            for (auto& worker : workers_)
            {
                if (worker.joinable()) worker.join();
            }
            workers_.clear();
        }

        void run_one() override
        {
            poll_once(50);
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

        // Reactor: install a one-shot poll for the requested direction on fd.
        // The kernel delivers a single CQE; the entry is gone after firing,
        // mirroring kqueue EV_ONESHOT and epoll EPOLLONESHOT.
        void register_poll(int fd, PollOperation* op) override
        {
            std::lock_guard<std::mutex> lock(sq_mutex_);
            struct io_uring_sqe* sqe = ::io_uring_get_sqe(&ring_);
            if (!sqe)
            {
                // SQ full — flush and retry once.
                ::io_uring_submit(&ring_);
                sqe = ::io_uring_get_sqe(&ring_);
                if (!sqe) return; // best-effort; caller will time out
            }
            unsigned mask =
                (op->interest == PollInterest::Read) ? POLLIN : POLLOUT;
            ::io_uring_prep_poll_add(sqe, fd, mask);
            ::io_uring_sqe_set_data(sqe, op);
            ::io_uring_submit(&ring_);
        }

    private:
        void wake() noexcept
        {
            if (wakeup_fd_ < 0) return;
            uint64_t one = 1;
            ssize_t r = ::write(wakeup_fd_, &one, sizeof(one));
            (void)r; // best-effort
        }

        // Arm the wakeup eventfd as a one-shot poll. We re-arm each time it
        // fires; this is slightly cheaper than multishot in liburing and
        // avoids the kernel-version dependency of IORING_POLL_ADD_MULTI.
        void arm_wakeup_poll()
        {
            std::lock_guard<std::mutex> lock(sq_mutex_);
            struct io_uring_sqe* sqe = ::io_uring_get_sqe(&ring_);
            if (!sqe) return;
            ::io_uring_prep_poll_add(sqe, wakeup_fd_, POLLIN);
            ::io_uring_sqe_set_data(sqe, &wakeup_token_);
            ::io_uring_submit(&ring_);
        }

        void worker_thread()
        {
            while (!stopped_)
            {
                poll_once(50);
                process_callbacks();
            }
        }

        void callback_drain_thread()
        {
            while (!stopped_)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                process_callbacks();
            }
        }

        void poll_once(int timeout_ms)
        {
            struct io_uring_cqe* cqe = nullptr;
            struct __kernel_timespec ts;
            ts.tv_sec = timeout_ms / 1000;
            ts.tv_nsec = (timeout_ms % 1000) * 1'000'000LL;

            int rc = ::io_uring_wait_cqe_timeout(&ring_, &cqe, &ts);
            if (rc == -ETIME || rc == -EINTR)
            {
                return;
            }
            if (rc < 0 || !cqe)
            {
                return;
            }

            void* ud = ::io_uring_cqe_get_data(cqe);
            ::io_uring_cqe_seen(&ring_, cqe);

            if (ud == &wakeup_token_)
            {
                // Drain the eventfd counter and re-arm the wakeup poll.
                uint64_t buf = 0;
                ssize_t r = ::read(wakeup_fd_, &buf, sizeof(buf));
                (void)r;
                arm_wakeup_poll();
                return;
            }

            auto* op = static_cast<PollOperation*>(ud);
            if (op && op->continuation)
            {
                op->continuation.resume();
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

    char UringContext::wakeup_token_ = 0;

    // Factory
    auto IoContext::create(size_t thread_count) -> std::unique_ptr<IoContext>
    {
        return std::make_unique<UringContext>(thread_count);
    }

} // namespace orm

#endif // __linux__ && ORM_HAS_LIBURING
