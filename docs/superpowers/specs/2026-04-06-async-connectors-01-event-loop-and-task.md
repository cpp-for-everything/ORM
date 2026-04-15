# Async Connectors — Event Loop, Task<T>, and Coroutine Primitives

**Date:** 2026-04-06
**Status:** Design notes — pre-implementation
**Author:** Design session with Alex Tsvetanov

---

## 1. Overview

This document specifies the three foundational primitives required by all async connectors:

1. **`EventLoop`** — platform-specific I/O multiplexer (io_uring / IOCP / kqueue)
2. **`Task<T>`** — minimal C++20 coroutine return type
3. **Awaitable bridges** — `IOAwaitable`, `FutureAwaitable`, `CassandraFutureAwaitable`

These primitives live in a new `lib/include/ORM/async/` directory and are shared by all async connectors.

---

## 2. Event Loop Abstraction

### 2.1 Responsibilities

The event loop is the single point where the program waits for I/O readiness. It:

- Accepts registrations: "wake me (resume this coroutine handle) when fd X is ready for read/write."
- Waits for one or more fds to become ready (blocking the event loop thread, not the coroutine).
- Resumes the coroutine handles associated with ready fds.
- Runs on one or more dedicated threads (typically one per hardware thread, or a configurable thread pool).

### 2.2 Interface

```cpp
// lib/include/ORM/async/event_loop.hpp
#pragma once
#include <coroutine>
#include <cstdint>
#include <functional>

namespace orm::async {

    enum class IOInterest : std::uint8_t
    {
        Read  = 0x01,
        Write = 0x02,
        Both  = 0x03
    };

    // Platform-agnostic event loop interface.
    // Concrete implementation is selected at compile time via #ifdef.
    class EventLoop
    {
    public:
        // Returns the event loop for the current thread.
        // Each thread that drives async work owns one EventLoop instance.
        [[nodiscard]] static EventLoop& current();

        // Register: when `fd` is ready for `interest`, resume `handle`.
        // The handle is resumed exactly once; caller must re-register for further events.
        void watch(int fd, IOInterest interest, std::coroutine_handle<> handle);

        // Cancel any pending watch on `fd`. Safe to call if no watch is active.
        void unwatch(int fd);

        // Drive the loop: submit pending registrations, wait for completions, resume handles.
        // Blocks until at least one event fires or stop() is called.
        void run();

        // Signal the loop to exit run() at the next opportunity.
        void stop();

        // Returns true if the loop is currently running.
        [[nodiscard]] bool is_running() const noexcept;

    private:
        struct Impl;       // platform-specific PIMPL
        Impl* impl_{};     // or std::unique_ptr<Impl> in production

        // Platform-specific data members sketched below
        // for reference — the actual PIMPL hides them.
#ifdef __linux__
        // struct io_uring ring_;
        // Per-fd map: fd → coroutine_handle<>
#elif defined(_WIN32)
        // HANDLE iocp_;
        // Per-handle overlapped structures
#elif defined(__APPLE__)
        // int kqueue_fd_;
        // Per-fd map: fd → coroutine_handle<>
#endif
    };

} // namespace orm::async
```

### 2.3 Platform Backends

#### Linux — io_uring

```
io_uring_setup()       → create the ring
io_uring_prep_poll_add → register fd interest
io_uring_submit()      → submit SQEs
io_uring_wait_cqe()    → block until completion
io_uring_cqe_seen()    → mark CQE consumed
user_data in SQE       → stores coroutine_handle<>.address()
```

- io_uring is available on Linux 5.1+ and is the highest-performance I/O multiplexer.
- For older kernels, fall back to `epoll_ctl` / `epoll_wait` with the same interface.
- The `watch()` call prepares a `IORING_OP_POLL_ADD` SQE with the fd and interest mask.
- The `user_data` field of the SQE stores `handle.address()` as a `uint64_t`.
- When `run()` processes a CQE, it reconstructs the handle via `std::coroutine_handle<>::from_address(cqe->user_data)` and calls `.resume()`.

#### Windows — IOCP

```
CreateIoCompletionPort()       → create the IOCP handle
PostQueuedCompletionStatus()   → associate a socket + key
GetQueuedCompletionStatus()    → dequeue a completion
lpCompletionKey                → stores coroutine_handle<>.address()
```

- All Windows async socket operations (WSARecv, WSASend, ConnectEx) post completions to the IOCP.
- For database libraries that expose a raw `SOCKET`, we can associate it with our IOCP.
- The completion key stores the coroutine handle address.

#### macOS — kqueue

```
kqueue()               → create the kqueue fd
kevent() with EV_ADD   → register fd interest (EVFILT_READ / EVFILT_WRITE)
kevent() with NULL changelist → wait for events
udata field            → stores coroutine_handle<>.address()
```

- kqueue is the canonical macOS/BSD I/O multiplexer.
- `watch()` calls `kevent()` with `EV_ADD | EV_ONESHOT` (one-shot, matches our "resume once" semantics).
- `udata` stores the coroutine handle.

### 2.4 Thread Model

```
┌─────────────────────────────────┐
│  Application Coroutines         │
│  (thousands of Tasks)           │
├─────────────────────────────────┤
│  EventLoop::run()               │  ← one per thread
│  (waits for I/O, resumes coros) │
├─────────────────────────────────┤
│  OS kernel                      │
│  (io_uring / IOCP / kqueue)     │
└─────────────────────────────────┘
```

- **Single-threaded mode:** One thread calls `EventLoop::current().run()`. All coroutines run on that thread. Sufficient for many applications.
- **Multi-threaded mode:** N threads each run their own `EventLoop`. Work is distributed across threads. Coroutines are pinned to the thread that created them (no cross-thread migration — simplifies lifetime management).

---

## 3. IOAwaitable — fd-Based Suspension

This is the core awaitable that MySQL, PostgreSQL, and Redis async connectors use. It suspends the current coroutine until a file descriptor is ready for a specified I/O operation.

```cpp
// lib/include/ORM/async/io_awaitable.hpp
#pragma once
#include "ORM/async/event_loop.hpp"

namespace orm::async {

    struct IOAwaitable
    {
        int          fd;
        IOInterest   interest;
        EventLoop*   loop;

        // I/O is never immediately ready — always suspend.
        [[nodiscard]] bool await_ready() const noexcept { return false; }

        // Register the fd with the event loop; the loop will resume us.
        void await_suspend(std::coroutine_handle<> h) noexcept
        {
            loop->watch(fd, interest, h);
        }

        // Nothing to return — the fd is now ready, caller proceeds with the next I/O step.
        void await_resume() const noexcept {}
    };

    // Convenience: wait for read-ready
    [[nodiscard]] inline IOAwaitable wait_readable(int fd)
    {
        return IOAwaitable{fd, IOInterest::Read, &EventLoop::current()};
    }

    // Convenience: wait for write-ready
    [[nodiscard]] inline IOAwaitable wait_writable(int fd)
    {
        return IOAwaitable{fd, IOInterest::Write, &EventLoop::current()};
    }

} // namespace orm::async
```

### 3.1 Usage Pattern — The `_start`/`_cont` Loop (MySQL)

Every MySQL non-blocking operation follows this universal pattern:

```cpp
// Generic pattern for any mysql_*_start / mysql_*_cont pair
template <typename StartFn, typename ContFn, typename... Args>
Task<void> mysql_async_op(int fd, StartFn start_fn, ContFn cont_fn, Args&&... args)
{
    int status = start_fn(std::forward<Args>(args)...);
    while (status)
    {
        co_await IOAwaitable{
            fd,
            (status & MYSQL_WAIT_WRITE) ? IOInterest::Write : IOInterest::Read,
            &EventLoop::current()
        };
        status = cont_fn(/* re-pass state */, status);
    }
}
```

### 3.2 Usage Pattern — The PQ State Machine (PostgreSQL)

```cpp
// Send query, flush, wait for result
co_await wait_writable(PQsocket(conn));  // ensure send buffer can accept
PQsendQueryParams(conn, sql, ...);       // non-blocking enqueue

while (PQflush(conn) == 1)              // drain send buffer
    co_await wait_writable(PQsocket(conn));

while (PQisBusy(conn))                  // wait for response
{
    co_await wait_readable(PQsocket(conn));
    PQconsumeInput(conn);
}

PGresult* res = PQgetResult(conn);       // ready — no blocking
```

---

## 4. Task<T> — Coroutine Return Type

### 4.1 Requirements

- `co_return value` stores the value in the promise.
- `co_await task` suspends the awaiting coroutine until the inner task completes, then returns its value.
- Exception propagation: an unhandled exception in the task is rethrown in the awaiter.
- Move-only: tasks own their coroutine frame.
- `[[nodiscard]]`: discarding a task is always a bug.

### 4.2 Implementation

```cpp
// lib/include/ORM/async/task.hpp
#pragma once
#include <coroutine>
#include <exception>
#include <utility>
#include <variant>

namespace orm::async {

    template <typename T>
    class [[nodiscard]] Task
    {
    public:
        struct promise_type
        {
            // Storage for the result: either T or an exception
            std::variant<std::monostate, T, std::exception_ptr> result_;

            // The coroutine that is co_awaiting this task
            std::coroutine_handle<> continuation_{};

            [[nodiscard]] Task get_return_object()
            {
                return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            // Lazy start: suspend immediately, run only when co_awaited.
            // This is critical for structured concurrency and avoids premature execution.
            std::suspend_always initial_suspend() noexcept { return {}; }

            // On final suspend, resume the continuation (the parent coroutine).
            struct FinalAwaiter
            {
                [[nodiscard]] bool await_ready() const noexcept { return false; }
                std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<promise_type> h) noexcept
                {
                    auto cont = h.promise().continuation_;
                    return cont ? cont : std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            [[nodiscard]] FinalAwaiter final_suspend() noexcept { return {}; }

            void return_value(T value)
            {
                result_.template emplace<1>(std::move(value));
            }

            void unhandled_exception()
            {
                result_.template emplace<2>(std::current_exception());
            }
        };

        // ── Awaiter interface ────────────────────────────────────────────
        [[nodiscard]] bool await_ready() const noexcept { return handle_.done(); }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> cont)
        {
            handle_.promise().continuation_ = cont;
            return handle_;   // symmetric transfer: resume the inner task
        }

        T await_resume()
        {
            auto& result = handle_.promise().result_;
            if (result.index() == 2)
                std::rethrow_exception(std::get<2>(result));
            return std::move(std::get<1>(result));
        }

        // ── Lifecycle ────────────────────────────────────────────────────
        explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}
        Task(Task&& other) noexcept : handle_(other.handle_) { other.handle_ = {}; }
        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;

        ~Task()
        {
            if (handle_)
                handle_.destroy();
        }

    private:
        std::coroutine_handle<promise_type> handle_;
    };

    // ── Task<void> specialization ────────────────────────────────────────
    template <>
    class [[nodiscard]] Task<void>
    {
    public:
        struct promise_type
        {
            std::exception_ptr exception_;
            std::coroutine_handle<> continuation_{};

            [[nodiscard]] Task get_return_object()
            {
                return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            std::suspend_always initial_suspend() noexcept { return {}; }

            struct FinalAwaiter
            {
                [[nodiscard]] bool await_ready() const noexcept { return false; }
                std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<promise_type> h) noexcept
                {
                    auto cont = h.promise().continuation_;
                    return cont ? cont : std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            [[nodiscard]] FinalAwaiter final_suspend() noexcept { return {}; }

            void return_void() {}

            void unhandled_exception()
            {
                exception_ = std::current_exception();
            }
        };

        [[nodiscard]] bool await_ready() const noexcept { return handle_.done(); }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> cont)
        {
            handle_.promise().continuation_ = cont;
            return handle_;
        }

        void await_resume()
        {
            if (handle_.promise().exception_)
                std::rethrow_exception(handle_.promise().exception_);
        }

        explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}
        Task(Task&& other) noexcept : handle_(other.handle_) { other.handle_ = {}; }
        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;
        ~Task() { if (handle_) handle_.destroy(); }

    private:
        std::coroutine_handle<promise_type> handle_;
    };

} // namespace orm::async
```

### 4.3 Design Decisions

| Decision | Rationale |
|---|---|
| **Lazy start (`suspend_always initial_suspend`)** | Ensures the task doesn't start executing until it's `co_await`ed. This is essential for structured concurrency — the caller controls when execution begins, preventing dangling references. |
| **Symmetric transfer in `FinalAwaiter`** | Instead of calling `continuation_.resume()` (which grows the stack), we return the continuation handle and let the compiler perform a symmetric transfer (tail call). This prevents stack overflow with deeply nested `co_await` chains. |
| **`std::variant` for result storage** | Avoids a separate `bool has_value_` flag. `monostate` = not yet set, index 1 = value, index 2 = exception. |
| **Move-only** | A coroutine frame is a unique resource. Copying would create two owners of the same frame. |
| **`[[nodiscard]]`** | Silently discarding a `Task<T>` means the coroutine frame is destroyed immediately, the async work never runs, and the result is lost. This is always a bug. |

### 4.4 Alternative: Use an Existing Library

Instead of writing `Task<T>` from scratch, we could use:

- **`cppcoro::task<T>`** — Lewis Baker's library. Well-tested, same design. But it's not header-only and requires linking.
- **`asio::awaitable<T>`** — Boost.Asio's coroutine type. Pulls in the entire Asio dependency.
- **`folly::coro::Task<T>`** — Facebook's coroutine library. Heavy dependency.
- **`stdexec::task<T>`** — P2300 sender/receiver proposal. Not yet standardized.

**Recommendation:** Write our own minimal `Task<T>` (as above) for zero external dependencies. The implementation is ~150 lines. If we later adopt Asio or stdexec for the event loop, we can type-erase or adapt.

---

## 5. FutureAwaitable — Thread Pool Bridge

For connectors without native async APIs (MongoDB, Neo4j, SQLite), we need to run blocking work on a thread pool and suspend the coroutine until it completes.

### 5.1 Thread Pool

```cpp
// lib/include/ORM/async/thread_pool.hpp
#pragma once
#include <functional>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>

namespace orm::async {

    class ThreadPool
    {
    public:
        // Returns a global shared thread pool (sized to hardware_concurrency).
        [[nodiscard]] static ThreadPool& global();

        explicit ThreadPool(std::size_t num_threads);
        ~ThreadPool();

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        // Post a callable to the pool. Returns a future for the result.
        template <typename F>
        [[nodiscard]] auto post(F&& f) -> std::future<std::invoke_result_t<F>>
        {
            using R = std::invoke_result_t<F>;
            auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));
            auto fut = task->get_future();
            {
                std::lock_guard<std::mutex> lk{mtx_};
                queue_.push([task]() { (*task)(); });
            }
            cv_.notify_one();
            return fut;
        }

    private:
        std::vector<std::jthread>          workers_;
        std::queue<std::function<void()>>  queue_;
        std::mutex                         mtx_;
        std::condition_variable            cv_;
        bool                               stop_{false};
    };

} // namespace orm::async
```

### 5.2 FutureAwaitable

Bridges a `std::future<T>` to a coroutine suspension point. The coroutine suspends; a background thread polls or is notified when the future is ready; then the coroutine is resumed on the event loop thread.

```cpp
// lib/include/ORM/async/future_awaitable.hpp
#pragma once
#include "ORM/async/event_loop.hpp"
#include "ORM/async/thread_pool.hpp"
#include <future>
#include <utility>

namespace orm::async {

    // Awaitable that suspends the coroutine, runs `f` on the thread pool,
    // and resumes the coroutine with the result.
    template <typename F>
    class PoolAwaitable
    {
    public:
        using result_type = std::invoke_result_t<F>;

        explicit PoolAwaitable(F&& f) : f_(std::forward<F>(f)) {}

        [[nodiscard]] bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h)
        {
            // Capture the event loop of the suspending thread
            auto* loop = &EventLoop::current();

            ThreadPool::global().post([this, h, loop]() {
                try
                {
                    if constexpr (std::is_void_v<result_type>)
                    {
                        f_();
                    }
                    else
                    {
                        result_.template emplace<1>(f_());
                    }
                }
                catch (...)
                {
                    result_.template emplace<2>(std::current_exception());
                }
                // Resume the coroutine on the event loop thread.
                // In production, this would post a "resume" event to `loop`.
                // For simplicity, we resume directly here (if the pool thread
                // is allowed to resume coroutines — depends on the thread model).
                h.resume();
            });
        }

        result_type await_resume()
        {
            if constexpr (!std::is_void_v<result_type>)
            {
                if (result_.index() == 2)
                    std::rethrow_exception(std::get<2>(result_));
                return std::move(std::get<1>(result_));
            }
            else
            {
                if (result_.index() == 2)
                    std::rethrow_exception(std::get<2>(result_));
            }
        }

    private:
        F f_;
        std::variant<std::monostate, result_type, std::exception_ptr> result_;
    };

    // Convenience factory
    template <typename F>
    [[nodiscard]] auto run_on_pool(F&& f)
    {
        return PoolAwaitable<std::decay_t<F>>{std::forward<F>(f)};
    }

} // namespace orm::async
```

### 5.3 Usage in MongoDB Async Connector

```cpp
// In connector_trait<AsyncMongoDBLive>::execute():
static auto execute(AsyncMongoDBLive& db, select_query<...> q, Params&&... params)
    -> Task<result<Row, Response>>
{
    // The entire synchronous MongoDB call is offloaded to the thread pool.
    // The coroutine suspends and the event loop thread is free to service
    // other coroutines while this one waits.
    co_return co_await run_on_pool([&] {
        // This lambda runs on a pool thread — blocking is OK here.
        return sync_exec_select(db, q, std::forward<Params>(params)...);
    });
}
```

---

## 6. CassandraFutureAwaitable — Callback Bridge

The Cassandra C driver already performs all I/O internally on its own threads. It returns a `CassFuture*` which supports a callback. We bridge this directly to a coroutine.

```cpp
// lib/include/ORM/async/cassandra_awaitable.hpp
#pragma once
#include <cassandra.h>
#include <coroutine>
#include <stdexcept>

namespace orm::async {

    struct CassandraFutureAwaitable
    {
        CassFuture* future_;

        [[nodiscard]] bool await_ready() const noexcept
        {
            // If the future is already resolved, don't suspend.
            return cass_future_ready(future_) == cass_true;
        }

        void await_suspend(std::coroutine_handle<> h) noexcept
        {
            // Set a callback that resumes the coroutine when the future completes.
            cass_future_set_callback(future_,
                [](CassFuture* /*f*/, void* data) {
                    auto handle = std::coroutine_handle<>::from_address(data);
                    handle.resume();
                },
                h.address());
        }

        void await_resume() const
        {
            if (cass_future_error_code(future_) != CASS_OK)
            {
                const char* msg = nullptr;
                std::size_t msg_len = 0;
                cass_future_error_message(future_, &msg, &msg_len);
                cass_future_free(future_);
                throw std::runtime_error(std::string("Cassandra error: ") +
                    std::string(msg, msg_len));
            }
        }
    };

} // namespace orm::async
```

### 6.1 Usage in Cassandra Async Connector

```cpp
// Replace every cass_future_wait(future) with:
CassFuture* future = cass_session_execute(db.session_, stmt);
co_await CassandraFutureAwaitable{future};
// future is now resolved — proceed to read the result
const CassResult* cass_res = cass_future_get_result(future);
cass_future_free(future);
```

This is the **lowest-effort** async conversion of any connector — the driver is already async internally.

---

## 7. Redis Async Adapter

hiredis provides `hiredis/async.h` with a callback-based async API and event loop adapter hooks. We need to connect these hooks to our `EventLoop`.

### 7.1 hiredis Event Loop Adapter

```cpp
// lib/include/ORM/async/hiredis_adapter.hpp
#pragma once
#include "ORM/async/event_loop.hpp"
#include <hiredis/hiredis.h>
#include <hiredis/async.h>

namespace orm::async {

    // Adapter functions that hiredis calls to register/unregister fd watches.
    // We bridge these to our EventLoop.

    struct HiredisAdapter
    {
        static void add_read(void* privdata)
        {
            auto* ac = static_cast<redisAsyncContext*>(privdata);
            EventLoop::current().watch(ac->c.fd, IOInterest::Read,
                /* stored coroutine handle — see full implementation */);
        }

        static void del_read(void* privdata) { /* unwatch read */ }
        static void add_write(void* privdata) { /* watch write */ }
        static void del_write(void* privdata) { /* unwatch write */ }
        static void cleanup(void* privdata) { /* cleanup */ }

        static void attach(redisAsyncContext* ac)
        {
            // Register our adapter functions with hiredis
            ac->ev.addRead  = add_read;
            ac->ev.delRead  = del_read;
            ac->ev.addWrite = add_write;
            ac->ev.delWrite = del_write;
            ac->ev.cleanup  = cleanup;
            ac->ev.data     = ac;
        }
    };

    // Awaitable for a single Redis async command
    struct RedisCommandAwaitable
    {
        redisAsyncContext* ctx_;
        const char*        cmd_;
        // ... args

        std::coroutine_handle<> handle_{};
        redisReply*             reply_{nullptr};

        [[nodiscard]] bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h)
        {
            handle_ = h;
            redisAsyncCommand(ctx_,
                [](redisAsyncContext* /*ac*/, void* reply, void* privdata) {
                    auto* self = static_cast<RedisCommandAwaitable*>(privdata);
                    self->reply_ = static_cast<redisReply*>(reply);
                    self->handle_.resume();
                },
                this, cmd_);
        }

        redisReply* await_resume() { return reply_; }
    };

} // namespace orm::async
```

---

## 8. is_async_connector Concept

The ORM core needs to distinguish sync and async connectors at compile time so that `db<DB>::operator<<` knows whether to `co_await` the result.

```cpp
// lib/include/ORM/connector/async_trait.hpp
#pragma once
#include "ORM/async/task.hpp"
#include <type_traits>

namespace orm {

    namespace detail {
        // Detect if connector_trait<DB>::execute returns Task<...>
        template <typename T>
        struct is_task : std::false_type {};

        template <typename T>
        struct is_task<async::Task<T>> : std::true_type {};
    }

    // A connector is async if its execute() returns Task<result<...>>
    template <typename DB>
    concept is_async_connector = requires {
        typename connector_trait<DB>::is_async;  // explicit opt-in tag
    };

} // namespace orm
```

### 8.1 Async db<DB> Extension

```cpp
// Sketch — the async overload of operator<< in db.hpp
template <typename DB>
    requires is_connector<DB>
class db
{
public:
    // Sync path (existing)
    template <typename Query>
        requires (!is_async_connector<DB>)
    auto operator<<(Query q)
    {
        return connector_trait<DB>::execute(*conn_, std::move(q));
    }

    // Async path (new)
    template <typename Query>
        requires is_async_connector<DB>
    auto operator<<(Query q) -> async::Task</* deduced */>
    {
        co_return co_await connector_trait<DB>::execute(*conn_, std::move(q));
    }

    // ... similarly for execute(Query, Params...)
};
```

This means **existing sync code compiles unchanged** — the sync `operator<<` is selected when `DB` is not an async connector.

---

## 9. Lifetime and Safety Rules

### 9.1 Coroutine Parameter Capture

**Critical rule:** Coroutines outlive their caller's stack frame. Any parameter passed by reference to a coroutine function will dangle after the first `co_await` if the caller has returned.

**ORM convention:**
- Query objects are passed **by value** (they're lightweight compile-time types).
- Runtime parameters (`Params&&...`) must be captured **by value** in the coroutine frame. The parameter pack expansion in the `execute()` signature should use `std::decay_t<Params>...` as value types stored in the frame.
- The `db` reference (`DB& conn`) is safe because the `db<DB>` object holds a pointer to the connection, and the connection outlives any individual query.

### 9.2 Connection Handle Ownership

- Async connector tag types (`AsyncMySQLDB`, `AsyncPgDB`, etc.) follow the same RAII pattern as their sync counterparts.
- The async `connect()` function is itself a coroutine returning `Task<AsyncMySQLDB>`.
- The destructor must **not** be a coroutine (destructors cannot be coroutines). For async close operations (e.g., Cassandra's `cass_session_close`), the destructor issues a synchronous close. An `async_close()` method is provided for graceful shutdown within a coroutine context.

### 9.3 Thread Safety

- A single `EventLoop` instance is **not** thread-safe. It runs on one thread.
- Coroutines pinned to an event loop thread must not be resumed from another thread without synchronization.
- The `FutureAwaitable` (thread pool bridge) handles cross-thread resumption by posting a "resume" notification to the event loop rather than calling `handle.resume()` directly from the pool thread.

---

## 10. File Layout

```
lib/include/ORM/async/
    event_loop.hpp              ← EventLoop interface
    io_awaitable.hpp            ← IOAwaitable (fd-based suspension)
    task.hpp                    ← Task<T> coroutine type
    thread_pool.hpp             ← ThreadPool for blocking offload
    future_awaitable.hpp        ← PoolAwaitable / run_on_pool()
    cassandra_awaitable.hpp     ← CassandraFutureAwaitable
    hiredis_adapter.hpp         ← hiredis event loop adapter

lib/src/ORM/async/
    event_loop_iouring.cpp      ← Linux io_uring backend
    event_loop_epoll.cpp        ← Linux epoll fallback
    event_loop_iocp.cpp         ← Windows IOCP backend
    event_loop_kqueue.cpp       ← macOS kqueue backend
    thread_pool.cpp             ← ThreadPool implementation
```

---

## 11. Alternatives Considered

| Alternative | Pros | Cons | Decision |
|---|---|---|---|
| **Boost.Asio as event loop** | Battle-tested, cross-platform, built-in coroutine support | Heavy dependency; pulls in Boost; Asio's type system is complex | Rejected — we want zero external deps for the core |
| **libuv** | Cross-platform, widely used (Node.js) | C library, callback-based, no native coroutine support, adds a dependency | Rejected |
| **Raw epoll everywhere** | Simple, well-understood | No io_uring benefits, no Windows/macOS support | Rejected — we use io_uring where available, epoll as fallback |
| **Sender/Receiver (P2300)** | Future-proof, composable | Not yet standardized, no compiler ships it | Rejected for now — can migrate later |
| **One thread per connection** | Simplest possible "async" | Defeats the purpose; doesn't scale | Rejected |

---

## 12. Open Questions

1. **Should `Task<T>` use eager or lazy start?** Current design uses lazy (`suspend_always initial_suspend`). Eager start simplifies some usage patterns but complicates structured concurrency. **Current decision: lazy.**

2. **Should we support cancellation?** A `CancellationToken` can be threaded through `Task<T>` to allow cooperative cancellation. Not needed for MVP but should be considered for the thread pool bridge. **Current decision: defer to post-MVP.**

3. **Should the event loop support timers?** Needed for connection timeouts and retry logic. io_uring supports `IORING_OP_TIMEOUT` natively. **Current decision: yes, add timer support in v2 of the event loop.**

4. **Cross-thread resumption model?** When the thread pool completes work, how do we resume the coroutine on the correct event loop thread? Options: (a) post a pipe/eventfd write that the event loop watches, (b) use an atomic queue that the event loop drains. **Current decision: eventfd (Linux) / PostQueuedCompletionStatus (Windows) / EVFILT_USER (macOS).**
