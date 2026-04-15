#include "ORM/async/io_context.hpp"

#if defined(__APPLE__)

#include <sys/event.h>
#include <unistd.h>

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <stdexcept>
#include <functional>
#include <cstring>

namespace orm {

    class KqueueContext : public IoContext
    {
        int kq_ = -1;
        std::vector<std::thread> workers_;
        std::atomic<bool> stopped_{false};
        size_t thread_count_;

        std::mutex callback_mutex_;
        std::queue<std::function<void()>> callbacks_;

        std::mutex poll_mutex_;

    public:
        explicit KqueueContext(size_t thread_count)
            : thread_count_(thread_count)
        {
            kq_ = kqueue();
            if (kq_ < 0)
            {
                throw std::runtime_error("kqueue() failed");
            }
        }

        ~KqueueContext() override
        {
            stop();

            for (auto& worker : workers_)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }

            if (kq_ >= 0)
            {
                close(kq_);
            }
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

        void stop() override { stopped_ = true; }

        [[nodiscard]] bool stopped() const noexcept override
        {
            return stopped_;
        }

        void post(std::function<void()> callback) override
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            callbacks_.push(std::move(callback));
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
        // Submits directly to kqueue fd (thread-safe) so registrations
        // from any thread are immediately visible to the event loop.
        void register_poll(int fd, PollOperation* op) override
        {
            struct kevent ev{};
            auto filter = (op->interest == PollInterest::Read)
                              ? EVFILT_READ
                              : EVFILT_WRITE;
            EV_SET(&ev, fd, filter, EV_ADD | EV_ONESHOT, 0, 0, op);

            std::lock_guard<std::mutex> lock(poll_mutex_);
            kevent(kq_, &ev, 1, nullptr, 0, nullptr);
        }

    private:
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
            struct kevent events[64];
            struct timespec ts = {0, 50000000}; // 50ms

            int n = kevent(kq_, nullptr, 0, events, 64, &ts);

            if (n < 0)
            {
                return;
            }

            for (int i = 0; i < n; ++i)
            {
                auto& ev = events[i];
                auto* op = static_cast<PollOperation*>(ev.udata);

                if (!op) continue;

                // Pure reactor: just resume the coroutine.
                // The caller's database library will do its own I/O.
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
        return std::make_unique<KqueueContext>(thread_count);
    }

} // namespace orm

#endif // __APPLE__
