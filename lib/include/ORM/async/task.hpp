#pragma once

#include <coroutine>
#include <exception>
#include <utility>
#include <optional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <type_traits>
#include <atomic>
#include <concepts>

namespace orm {

    template <typename T = void>
    class Task;

    namespace detail {

        struct TaskPromiseBase
        {
            std::coroutine_handle<> continuation_ = std::noop_coroutine();
            std::exception_ptr exception_;
            bool detached_ = false;
            std::function<void()> on_complete_;

            struct FinalAwaiter
            {
                static bool await_ready() noexcept { return false; }

                // NOTE: must NOT be inlined into the coroutine's final-suspend ramp.
                // This awaiter calls h.destroy() to free the (detached) coroutine
                // frame; if inlined, Apple clang / libc++ keeps live references to the
                // just-freed frame across the symmetric-transfer tail call → crash.
                // Forcing a real call makes destroy() operate on its own stack frame.
                template <typename Promise>
                __attribute__((noinline)) static std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<Promise> h) noexcept
                {
                    auto& promise = h.promise();

                    // Cache all fields BEFORE on_complete_. Calling on_complete_
                    // wakes sync_wait() which may destroy the coroutine frame on
                    // another thread. Any access to the promise after that is UB.
                    bool is_detached = promise.detached_;
                    auto continuation = promise.continuation_;
                    auto on_done = std::move(promise.on_complete_);

                    if (on_done)
                    {
                        on_done();
                    }
                    if (is_detached)
                    {
                        h.destroy();
                        return std::noop_coroutine();
                    }
                    if (continuation)
                    {
                        return continuation;
                    }
                    return std::noop_coroutine();
                }

                static void await_resume() noexcept {}
            };

            static std::suspend_always initial_suspend() noexcept { return {}; }
            static FinalAwaiter final_suspend() noexcept { return {}; }

            void detach() noexcept { detached_ = true; }

            void unhandled_exception() noexcept
            {
                exception_ = std::current_exception();
            }
        };

        template <typename T>
        struct TaskPromise : TaskPromiseBase
        {
            std::optional<T> value_;

            Task<T> get_return_object() noexcept;

            template <typename U>
                requires std::convertible_to<U, T>
            void return_value(U&& value) noexcept(
                std::is_nothrow_constructible_v<T, U>)
            {
                value_.emplace(std::forward<U>(value));
            }

            T& result() &
            {
                if (exception_) std::rethrow_exception(exception_);
                return *value_;
            }

            T&& result() &&
            {
                if (exception_) std::rethrow_exception(exception_);
                return std::move(*value_);
            }
        };

        template <>
        struct TaskPromise<void> : TaskPromiseBase
        {
            Task<void> get_return_object() noexcept;

            static void return_void() noexcept {}

            void result()
            {
                if (exception_) std::rethrow_exception(exception_);
            }
        };

    } // namespace detail

    template <typename T>
    class [[nodiscard]] Task
    {
    public:
        using promise_type = detail::TaskPromise<T>;
        using handle_type = std::coroutine_handle<promise_type>;

    private:
        handle_type handle_;

    public:
        Task() noexcept : handle_(nullptr) {}

        explicit Task(handle_type h) noexcept : handle_(h) {}

        Task(Task&& other) noexcept : handle_(other.handle_)
        {
            other.handle_ = nullptr;
        }

        Task& operator=(Task&& other) noexcept
        {
            if (this != &other)
            {
                if (handle_) handle_.destroy();
                handle_ = other.handle_;
                other.handle_ = nullptr;
            }
            return *this;
        }

        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;

        ~Task()
        {
            if (handle_) handle_.destroy();
        }

        [[nodiscard]] bool valid() const noexcept { return handle_ != nullptr; }
        explicit operator bool() const noexcept { return valid(); }
        [[nodiscard]] bool done() const noexcept { return handle_ && handle_.done(); }

        struct Awaiter
        {
            handle_type handle_;

            bool await_ready() const noexcept
            {
                return !handle_ || handle_.done();
            }

            std::coroutine_handle<> await_suspend(
                std::coroutine_handle<> continuation) noexcept
            {
                handle_.promise().continuation_ = continuation;
                return handle_;
            }

            T await_resume()
            {
                if constexpr (std::is_void_v<T>)
                {
                    handle_.promise().result();
                }
                else
                {
                    return std::move(handle_.promise()).result();
                }
            }
        };

        Awaiter operator co_await() && noexcept { return Awaiter{handle_}; }

        void start()
        {
            if (handle_ && !handle_.done())
            {
                handle_.resume();
            }
        }

        T sync_wait()
        {
            if (done())
            {
                if constexpr (std::is_void_v<T>)
                {
                    handle_.promise().result();
                    return;
                }
                else
                {
                    return std::move(handle_.promise()).result();
                }
            }

            std::mutex mtx;
            std::condition_variable cv;
            bool finished = false;

            handle_.promise().on_complete_ = [&]
            {
                // Notify WHILE holding the lock. If notify_one() ran after the
                // unlock, the woken waiter could return from sync_wait and destroy
                // `cv`/`mtx` (stack locals) before notify_one() touched `cv` — a use-
                // after-free of the condition_variable (crashes on libc++/macOS where
                // the waiter wins the race; latent on glibc). Holding the lock blocks
                // the waiter from re-acquiring `mtx` to leave cv.wait until we unlock.
                std::lock_guard<std::mutex> lock(mtx);
                finished = true;
                cv.notify_one();
            };

            handle_.resume();

            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [&] { return finished; });
            }

            handle_.promise().on_complete_ = nullptr;

            if constexpr (std::is_void_v<T>)
            {
                handle_.promise().result();
            }
            else
            {
                return std::move(handle_.promise()).result();
            }
        }

        handle_type release() noexcept
        {
            auto h = handle_;
            handle_ = nullptr;
            return h;
        }

        void detach()
        {
            if (handle_)
            {
                handle_.promise().detach();
                handle_ = nullptr;
            }
        }

        void start_detached()
        {
            if (handle_ && !handle_.done())
            {
                handle_.promise().detach();
                handle_.resume();
                handle_ = nullptr;
            }
        }
    };

    namespace detail {

        template <typename T>
        Task<T> TaskPromise<T>::get_return_object() noexcept
        {
            return Task<T>{
                std::coroutine_handle<TaskPromise<T>>::from_promise(*this)};
        }

        inline Task<void> TaskPromise<void>::get_return_object() noexcept
        {
            return Task<void>{
                std::coroutine_handle<TaskPromise<void>>::from_promise(*this)};
        }

    } // namespace detail

    struct YieldAwaiter
    {
        static bool await_ready() noexcept { return false; }

        static std::coroutine_handle<> await_suspend(
            std::coroutine_handle<> h) noexcept
        {
            std::this_thread::yield();
            return h;
        }

        static void await_resume() noexcept {}
    };

    inline YieldAwaiter yield() noexcept { return {}; }

} // namespace orm
