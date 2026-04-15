#pragma once

#include "ORM/async/task.hpp"
#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

namespace orm {

    class ThreadPool
    {
        std::vector<std::thread> workers_;
        std::queue<std::function<void()>> tasks_;
        std::mutex mutex_;
        std::condition_variable cv_;
        std::atomic<bool> stopped_{false};

    public:
        explicit ThreadPool(size_t thread_count = std::thread::hardware_concurrency())
        {
            for (size_t i = 0; i < thread_count; ++i)
            {
                workers_.emplace_back([this] { worker_loop(); });
            }
        }

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        ~ThreadPool()
        {
            shutdown();
        }

        void shutdown() noexcept
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (stopped_) return;
                stopped_ = true;
            }
            cv_.notify_all();
            for (auto& w : workers_)
            {
                if (w.joinable()) w.join();
            }
        }

        void post(std::function<void()> task)
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                tasks_.push(std::move(task));
            }
            cv_.notify_one();
        }

        [[nodiscard]] size_t thread_count() const noexcept
        {
            return workers_.size();
        }

        [[nodiscard]] bool is_stopped() const noexcept
        {
            return stopped_.load(std::memory_order_acquire);
        }

    private:
        void worker_loop()
        {
            while (true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cv_.wait(lock, [this] {
                        return stopped_ || !tasks_.empty();
                    });
                    if (stopped_ && tasks_.empty()) return;
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                if (task) task();
            }
        }
    };

    // ── run_on_pool ─────────────────────────────────────────────────────────
    //
    // Offloads a callable to a thread pool and suspends the calling coroutine.
    // When the callable completes, the coroutine is resumed on the pool thread.
    //
    // Usage (non-void):
    //   auto result = co_await orm::run_on_pool(pool, [&] {
    //       return db.execute("SELECT ...");
    //   });
    //
    // Usage (void):
    //   co_await orm::run_on_pool(pool, [&] { db.ping(); });
    //

    namespace detail {

        template <typename F>
        struct PoolAwaiter
        {
            using R = std::invoke_result_t<F>;

            ThreadPool& pool;
            F func;
            std::exception_ptr exception;
            std::coroutine_handle<> continuation;

            // Storage for non-void results; empty for void
            struct VoidTag {};
            using Storage = std::conditional_t<std::is_void_v<R>,
                                               VoidTag,
                                               std::optional<R>>;
            Storage result{};

            bool await_ready() const noexcept { return false; }

            void await_suspend(std::coroutine_handle<> h) noexcept
            {
                continuation = h;
                pool.post([this] {
                    try
                    {
                        if constexpr (std::is_void_v<R>)
                        {
                            func();
                        }
                        else
                        {
                            result.emplace(func());
                        }
                    }
                    catch (...)
                    {
                        exception = std::current_exception();
                    }
                    continuation.resume();
                });
            }

            auto await_resume() -> R
            {
                if (exception) std::rethrow_exception(exception);
                if constexpr (!std::is_void_v<R>)
                {
                    return std::move(*result);
                }
            }
        };

    } // namespace detail

    template <typename F>
    auto run_on_pool(ThreadPool& pool, F&& func) -> Task<std::invoke_result_t<F>>
    {
        using R = std::invoke_result_t<F>;

        if constexpr (std::is_void_v<R>)
        {
            co_await detail::PoolAwaiter<std::decay_t<F>>{
                pool, std::forward<F>(func), {}, {}};
        }
        else
        {
            co_return co_await detail::PoolAwaiter<std::decay_t<F>>{
                pool, std::forward<F>(func), {}, {}, {}};
        }
    }

} // namespace orm
