# Async Database Connectors — Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transform all ORM database connectors from blocking synchronous I/O to coroutine-based asynchronous I/O using an event loop mirrored from the coroute/webframe project, maximising CPU utilisation and minimising idle wait time on database sockets.

**Architecture:** Mirror the production-quality `IoContext` (io_uring / IOCP / kqueue), `Task<T>`, and `CancellationToken` from the webframe project into the ORM's own `lib/include/ORM/async/` namespace. Extend `IoContext` with a reactor-mode `watch_fd()` so database libraries can drive their own socket I/O after readiness notification. Adapt the connector trait system with an `is_async_connector` concept. Convert each connector's `execute()` to a coroutine returning `Task<result<...>>`. Add async-aware connection pool and transaction guard. Keep all existing sync connectors working unchanged.

**Tech Stack:** C++23, C++20 coroutines, io_uring (Linux), IOCP (Windows), kqueue (macOS), Google Test, Docker, CMake 3.20+

---

## File Map

### New files to create

#### Core async infrastructure (`lib/include/ORM/async/`)

| File | Responsibility |
|------|----------------|
| `task.hpp` | `orm::Task<T>` — mirrored and adapted from `coroute::Task<T>` |
| `cancellation.hpp` | `orm::CancellationToken`, `CancellationSource`, `CancellationGuard` |
| `io_context.hpp` | Abstract `orm::IoContext` base — `run()`, `stop()`, `post()`, `watch_fd()` |
| `io_context_factory.cpp` | `IoContext::create()` factory — platform detection |
| `kqueue_context.cpp` | kqueue backend (macOS) — proactor + reactor |
| `uring_context.cpp` | io_uring backend (Linux) — proactor + reactor |
| `iocp_context.cpp` | IOCP backend (Windows) — proactor + reactor |
| `thread_pool.hpp` | `orm::ThreadPool` — for offloading blocking libs (MongoDB, Neo4j, SQLite) |
| `pool_awaitable.hpp` | `orm::run_on_pool(ThreadPool&, F)` — coroutine bridge |

#### Connector infrastructure changes (`lib/include/ORM/connector/`)

| File | Change |
|------|--------|
| `capabilities.hpp` | Add `cap::supports_async` tag |
| `trait.hpp` | Add `is_async_connector<DB>` concept |
| `db.hpp` | Add `operator<<` overload returning `Task<result<...>>` when `is_async_connector<DB>` |
| `async_db.hpp` (new) | `orm::async_db<DB>` — async-only handle with `co_await db << query` |

#### Async connection management (`lib/include/ORM/db/connectors/ThreadSafety/`)

| File | Change |
|------|--------|
| `thread_safety.hpp` | Add `async_connection_pool<DB, N>` with `co_await acquire()` |
| `async_transaction.hpp` (new) | `orm::async_transaction_guard<DB>` with `co_await begin/commit/rollback` |

#### Per-connector async implementations

| Connector | New file | Strategy |
|-----------|----------|----------|
| MySQL | `mysql_async.hpp` | Reactor: `_start`/`_cont` + `watch_fd()` |
| PostgreSQL | `postgresql_async.hpp` | Reactor: `PQsendQuery` + `watch_fd()` on `PQsocket()` |
| Redis | `redis_async.hpp` | Reactor: hiredis async callbacks + `watch_fd()` |
| Cassandra | `cassandra_async.hpp` | Callback bridge: `CassFuture` → coroutine via `post()` |
| MongoDB | `mongodb_async.hpp` | Thread pool: `run_on_pool()` wrapping sync calls |
| Neo4j | `neo4j_async.hpp` | Thread pool: `run_on_pool()` wrapping sync calls |
| SQLite | `sqlite_async.hpp` | Dedicated worker thread via `run_on_pool()` |

#### Tests

| File | What it tests |
|------|---------------|
| `tests/unit/test_task.cpp` | `Task<T>` coroutine mechanics, sync_wait, cancellation |
| `tests/unit/test_io_context.cpp` | IoContext post(), watch_fd(), run/stop lifecycle |
| `tests/unit/test_thread_pool.cpp` | ThreadPool + run_on_pool() |
| `tests/unit/test_async_pool.cpp` | async_connection_pool acquire/release, transaction isolation |
| `tests/unit/test_async_mysql_connector.cpp` | MySQL async connector_trait (mock socket) |
| `tests/unit/test_async_postgresql_connector.cpp` | PostgreSQL async connector_trait (mock socket) |
| `tests/unit/test_async_cassandra_connector.cpp` | Cassandra async connector_trait (mock future) |
| `tests/unit/test_async_redis_connector.cpp` | Redis async connector_trait (mock) |
| `tests/unit/test_async_mongodb_connector.cpp` | MongoDB async connector_trait (mock) |
| `tests/unit/test_async_sqlite_connector.cpp` | SQLite async connector_trait (mock) |
| `tests/integration/test_async_mysql_live.cpp` | MySQL async live integration |
| `tests/integration/test_async_postgresql_live.cpp` | PostgreSQL async live integration |
| `tests/integration/test_async_redis_live.cpp` | Redis async live integration |
| `tests/integration/test_async_cassandra_live.cpp` | Cassandra async live integration |
| `tests/integration/test_async_mongodb_live.cpp` | MongoDB async live integration |
| `tests/integration/test_async_sqlite_live.cpp` | SQLite async live integration |
| `tests/integration/test_async_transaction_isolation.cpp` | Multi-coroutine transaction isolation |

#### Benchmarks

| File | What it measures |
|------|------------------|
| `benchmarks/CMakeLists.txt` | Build configuration for benchmarks |
| `benchmarks/bench_sync_vs_async.cpp` | Head-to-head: sync vs async throughput per connector |
| `benchmarks/bench_concurrent_queries.cpp` | N concurrent queries scaling |
| `benchmarks/bench_connection_pool.cpp` | Pool contention under load |
| `docker/docker-compose.benchmark.yml` | Benchmark-specific docker services |
| `benchmarks/run_benchmarks.sh` | Orchestration script |

#### CMake changes

| File | Change |
|------|--------|
| `CMakeLists.txt` (root) | Add `ORM_ENABLE_ASYNC` option, platform detection |
| `lib/CMakeLists.txt` | Add `add_subdirectory(src/ORM/async)` |
| `lib/src/ORM/async/CMakeLists.txt` (new) | Build `orm::async` library (compiled, not INTERFACE) |
| `tests/unit/CMakeLists.txt` | Add async test sources |
| `tests/integration/CMakeLists.txt` | Add async integration test sources |

---

## Chunk 1: Core Async Primitives

### Task 1: `orm::Task<T>` coroutine type

**Files:**
- Create: `lib/include/ORM/async/task.hpp`
- Test: `tests/unit/test_task.cpp`

- [ ] **Step 1: Write failing tests for Task<T>**

```cpp
// tests/unit/test_task.cpp
#include <gtest/gtest.h>
#include "ORM/async/task.hpp"

TEST(TaskTest, VoidTaskCompletes)
{
    bool executed = false;
    auto task = [&]() -> orm::Task<void> {
        executed = true;
        co_return;
    }();
    task.sync_wait();
    EXPECT_TRUE(executed);
}

TEST(TaskTest, IntTaskReturnsValue)
{
    auto task = []() -> orm::Task<int> {
        co_return 42;
    }();
    EXPECT_EQ(task.sync_wait(), 42);
}

TEST(TaskTest, NestedAwait)
{
    auto inner = []() -> orm::Task<int> { co_return 7; };
    auto outer = [&]() -> orm::Task<int> {
        int v = co_await inner();
        co_return v * 6;
    }();
    EXPECT_EQ(outer.sync_wait(), 42);
}

TEST(TaskTest, ExceptionPropagates)
{
    auto task = []() -> orm::Task<int> {
        throw std::runtime_error("boom");
        co_return 0;
    }();
    EXPECT_THROW(task.sync_wait(), std::runtime_error);
}

TEST(TaskTest, MoveOnlyNoDiscard)
{
    static_assert(!std::is_copy_constructible_v<orm::Task<int>>);
    static_assert(std::is_move_constructible_v<orm::Task<int>>);
}

TEST(TaskTest, StartDetached)
{
    std::atomic<bool> done{false};
    auto task = [&]() -> orm::Task<void> {
        done.store(true, std::memory_order_release);
        co_return;
    }();
    task.start_detached();
    // Detached tasks run immediately with lazy start + resume
    EXPECT_TRUE(done.load(std::memory_order_acquire));
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build && ctest -R test_task -V`
Expected: FAIL — `ORM/async/task.hpp` does not exist

- [ ] **Step 3: Implement `orm::Task<T>`**

Mirror from `coroute::Task<T>` (external/webframe/include/coroute/coro/task.hpp) with these adaptations:
- Namespace: `orm` instead of `coroute`
- Remove dependency on `coroute/util/expected.hpp` and `coroute/core/error.hpp` — Task<T> is self-contained
- Remove `CancellationToken` from promise for now (added in Task 2)
- Keep: lazy start, symmetric transfer, `sync_wait()`, `start_detached()`, `detach()`, `YieldAwaiter`
- Add `[[nodiscard]]` per code style rules
- Use 4-space indent, `lower_case` methods, `CamelCase` types, trailing underscore for members

```cpp
// lib/include/ORM/async/task.hpp
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

                template <typename Promise>
                static std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<Promise> h) noexcept
                {
                    auto& promise = h.promise();
                    if (promise.on_complete_)
                    {
                        promise.on_complete_();
                    }
                    if (promise.detached_)
                    {
                        h.destroy();
                        return std::noop_coroutine();
                    }
                    if (promise.continuation_)
                    {
                        return promise.continuation_;
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
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    finished = true;
                }
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
```

- [ ] **Step 4: Add CMake build for async library**

Create `lib/src/ORM/async/CMakeLists.txt`:
```cmake
# Detect platform and set I/O backend
if(WIN32)
    set(ORM_IO_BACKEND "iocp" CACHE STRING "I/O backend")
    add_compile_definitions(ORM_PLATFORM_WINDOWS)
elseif(UNIX AND NOT APPLE)
    set(ORM_IO_BACKEND "io_uring" CACHE STRING "I/O backend")
    add_compile_definitions(ORM_PLATFORM_LINUX)
elseif(APPLE)
    set(ORM_IO_BACKEND "kqueue" CACHE STRING "I/O backend")
    add_compile_definitions(ORM_PLATFORM_MACOS)
endif()

add_library(orm_async INTERFACE)
add_library(orm::async ALIAS orm_async)

target_include_directories(orm_async INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(orm_async INTERFACE orm::orm)
target_compile_features(orm_async INTERFACE cxx_std_23)

# Platform-specific link deps
if(ORM_IO_BACKEND STREQUAL "io_uring")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(URING REQUIRED liburing)
    target_link_libraries(orm_async INTERFACE ${URING_LIBRARIES})
    target_include_directories(orm_async INTERFACE ${URING_INCLUDE_DIRS})
elseif(ORM_IO_BACKEND STREQUAL "iocp")
    target_link_libraries(orm_async INTERFACE ws2_32 mswsock)
endif()
```

Add to `lib/CMakeLists.txt`:
```cmake
# ── Async infrastructure ─────────────────────────────────────────────
add_subdirectory(src/ORM/async)
```

Add test to `tests/unit/CMakeLists.txt`:
```cmake
# test_task.cpp added to test_unit sources
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake -S . -B build && cmake --build build && ctest --test-dir build -R TaskTest -V`
Expected: All TaskTest cases PASS

- [ ] **Step 6: Commit**

```bash
git add lib/include/ORM/async/task.hpp lib/src/ORM/async/CMakeLists.txt \
        lib/CMakeLists.txt tests/unit/test_task.cpp tests/unit/CMakeLists.txt
git commit -m "feat(async): add orm::Task<T> coroutine type"
```

---

### Task 2: Cancellation primitives

**Files:**
- Create: `lib/include/ORM/async/cancellation.hpp`
- Modify: `lib/include/ORM/async/task.hpp` (add cancellation token to promise)
- Test: `tests/unit/test_task.cpp` (append cancellation tests)

- [ ] **Step 1: Write failing cancellation tests**

```cpp
TEST(CancellationTest, TokenStartsNotCancelled)
{
    orm::CancellationSource source;
    auto token = source.token();
    EXPECT_FALSE(token.is_cancelled());
}

TEST(CancellationTest, CancelPropagates)
{
    orm::CancellationSource source;
    auto token = source.token();
    source.cancel();
    EXPECT_TRUE(token.is_cancelled());
}

TEST(CancellationTest, CallbackInvokedOnCancel)
{
    orm::CancellationSource source;
    auto token = source.token();
    bool called = false;
    token.on_cancel([&] { called = true; });
    source.cancel();
    EXPECT_TRUE(called);
}

TEST(CancellationTest, GuardCancelsOnDestruction)
{
    orm::CancellationSource source;
    auto token = source.token();
    {
        orm::CancellationGuard guard(source);
    }
    EXPECT_TRUE(token.is_cancelled());
}
```

- [ ] **Step 2: Run tests, verify failure**

- [ ] **Step 3: Implement `orm::CancellationToken`**

Mirror from `coroute::CancellationToken` with namespace change to `orm`. Same structure: `CancellationState` (shared), `CancellationToken` (read-only view), `CancellationSource` (write control), `CancellationGuard` (RAII).

- [ ] **Step 4: Integrate cancellation into Task promise**

Add `CancellationToken cancel_token_` to `TaskPromiseBase`, add `set_cancellation_token()`, `is_cancelled()`, add `CheckCancellationAwaiter`.

- [ ] **Step 5: Run all tests, verify pass**

- [ ] **Step 6: Commit**

```bash
git add lib/include/ORM/async/cancellation.hpp lib/include/ORM/async/task.hpp \
        tests/unit/test_task.cpp
git commit -m "feat(async): add CancellationToken and integrate with Task<T>"
```

---

### Task 3: `orm::IoContext` with reactor extension

**Files:**
- Create: `lib/include/ORM/async/io_context.hpp`
- Create: `lib/src/ORM/async/kqueue_context.cpp` (macOS)
- Create: `lib/src/ORM/async/uring_context.cpp` (Linux)
- Create: `lib/src/ORM/async/iocp_context.cpp` (Windows)
- Create: `lib/src/ORM/async/io_context_factory.cpp`
- Modify: `lib/src/ORM/async/CMakeLists.txt` (add compiled sources)
- Test: `tests/unit/test_io_context.cpp`

- [ ] **Step 1: Write failing IoContext tests**

```cpp
// tests/unit/test_io_context.cpp
#include <gtest/gtest.h>
#include "ORM/async/io_context.hpp"
#include "ORM/async/task.hpp"
#include <atomic>
#include <thread>

TEST(IoContextTest, CreateSucceeds)
{
    auto ctx = orm::IoContext::create(1);
    ASSERT_NE(ctx, nullptr);
}

TEST(IoContextTest, PostCallbackExecutes)
{
    auto ctx = orm::IoContext::create(1);
    std::atomic<bool> called{false};
    ctx->post([&] { called = true; ctx->stop(); });

    std::thread t([&] { ctx->run(); });
    t.join();

    EXPECT_TRUE(called.load());
}

TEST(IoContextTest, RunOneProcessesSingleEvent)
{
    auto ctx = orm::IoContext::create(1);
    std::atomic<int> count{0};
    ctx->post([&] { count++; });
    ctx->post([&] { count++; });
    ctx->run_one();
    EXPECT_EQ(count.load(), 1);
}

TEST(IoContextTest, StopBreaksRunLoop)
{
    auto ctx = orm::IoContext::create(1);
    ctx->post([&] { ctx->stop(); });
    ctx->run(); // must return, not hang
    EXPECT_TRUE(ctx->stopped());
}

// watch_fd test uses a pipe for portable fd readiness
TEST(IoContextTest, WatchFdReadable)
{
    auto ctx = orm::IoContext::create(1);
    int pipefd[2];
    ASSERT_EQ(pipe(pipefd), 0);

    std::atomic<bool> ready{false};

    auto coro = [&]() -> orm::Task<void> {
        co_await ctx->watch_readable(pipefd[0]);
        ready.store(true, std::memory_order_release);
        ctx->stop();
        co_return;
    }();
    coro.start_detached();

    // Write to pipe to make read end readable
    ctx->post([&] {
        char buf = 'x';
        write(pipefd[1], &buf, 1);
    });

    ctx->run();

    EXPECT_TRUE(ready.load());
    close(pipefd[0]);
    close(pipefd[1]);
}
```

- [ ] **Step 2: Run tests, verify failure**

- [ ] **Step 3: Implement `orm::IoContext` abstract interface**

```cpp
// lib/include/ORM/async/io_context.hpp
// Abstract base with:
//   virtual void run() = 0;
//   virtual void run_one() = 0;
//   virtual void stop() = 0;
//   virtual bool stopped() const noexcept = 0;
//   virtual void post(std::function<void()>) = 0;
//   virtual void schedule(std::chrono::milliseconds, std::function<void()>) = 0;
//
// REACTOR EXTENSION (new vs webframe):
//   WatchReadableAwaitable watch_readable(int fd);
//   WatchWritableAwaitable watch_writable(int fd);
//
// Factory:
//   static std::unique_ptr<IoContext> create(size_t thread_count = 1);
```

Key difference from webframe: `watch_readable(fd)` and `watch_writable(fd)` return awaitables that suspend the coroutine until the fd is ready, **without performing any I/O** — pure reactor. The caller (database library) does its own read/write after resumption.

- [ ] **Step 4: Implement kqueue backend**

Mirror from webframe's `kqueue_context.cpp` with these changes:
- Add `PollReady` to `KqueueOpType`
- In `process_events()`: when `op->type == PollReady`, skip `recv()`/`send()` — just resume the coroutine
- Add `register_poll_op(int fd, PollInterest interest, PollOperation* op)` method
- Expose `watch_readable()` / `watch_writable()` returning awaitables

- [ ] **Step 5: Implement io_uring backend**

Mirror from webframe's `uring_context.cpp` with:
- Add `PollReady` to `UringOpType`
- Use `io_uring_prep_poll_add(sqe, fd, POLLIN)` / `io_uring_prep_poll_add(sqe, fd, POLLOUT)` for reactor ops
- Resume coroutine on CQE completion

- [ ] **Step 6: Implement IOCP backend**

Mirror from webframe's `iocp_context.cpp` with:
- Zero-byte `WSARecv` for readiness detection
- Resume coroutine on completion

- [ ] **Step 7: Update CMakeLists for compiled sources**

Change `orm_async` from INTERFACE to STATIC library, add platform-specific source files.

- [ ] **Step 8: Run all tests, verify pass**

- [ ] **Step 9: Commit**

```bash
git commit -m "feat(async): add orm::IoContext with reactor watch_fd extension"
```

---

### Task 4: Thread pool and `run_on_pool()`

**Files:**
- Create: `lib/include/ORM/async/thread_pool.hpp`
- Create: `lib/include/ORM/async/pool_awaitable.hpp`
- Test: `tests/unit/test_thread_pool.cpp`

- [ ] **Step 1: Write failing tests**

```cpp
TEST(ThreadPoolTest, RunsCallback)
{
    orm::ThreadPool pool(2);
    std::atomic<bool> ran{false};
    auto task = orm::run_on_pool(pool, [&] {
        ran = true;
        return 42;
    });
    EXPECT_EQ(task.sync_wait(), 42);
    EXPECT_TRUE(ran.load());
}

TEST(ThreadPoolTest, MultipleTasksConcurrent)
{
    orm::ThreadPool pool(4);
    std::atomic<int> count{0};
    auto make_task = [&]() -> orm::Task<void> {
        co_await orm::run_on_pool(pool, [&] { count++; });
        co_return;
    };

    std::vector<orm::Task<void>> tasks;
    for (int i = 0; i < 100; ++i)
        tasks.push_back(make_task());

    for (auto& t : tasks)
        t.sync_wait();

    EXPECT_EQ(count.load(), 100);
}
```

- [ ] **Step 2: Run tests, verify failure**

- [ ] **Step 3: Implement ThreadPool**

Simple thread pool: fixed worker count, `std::queue<std::function<void()>>`, mutex + condition_variable. `post(F)` enqueues work.

- [ ] **Step 4: Implement `run_on_pool()` awaitable**

Returns a `Task<R>` that suspends the calling coroutine, posts the work to the pool, and resumes with the result.

- [ ] **Step 5: Run all tests, verify pass**

- [ ] **Step 6: Commit**

```bash
git commit -m "feat(async): add ThreadPool and run_on_pool() awaitable"
```

---

## Chunk 2: Connector Infrastructure

### Task 5: `supports_async` capability and `is_async_connector` concept

**Files:**
- Modify: `lib/include/ORM/connector/capabilities.hpp`
- Modify: `lib/include/ORM/connector/trait.hpp`
- Test: `tests/unit/test_types.cpp` (append)

- [ ] **Step 1: Write failing tests**

```cpp
TEST(AsyncCapability, MockDBNotAsync)
{
    EXPECT_FALSE(orm::has_capability<MockDB, orm::cap::supports_async>);
}
// After implementing an async mock, test the positive case too
```

- [ ] **Step 2: Add `cap::supports_async` tag to capabilities.hpp**

- [ ] **Step 3: Add `capability_check` specialization for `supports_async`**

- [ ] **Step 4: Add `is_async_connector<DB>` concept to trait.hpp**

```cpp
template <typename DB>
concept is_async_connector = is_connector<DB> &&
    has_capability<DB, cap::supports_async> &&
    requires {
        typename connector_trait<DB>::async_execute;
    };
```

- [ ] **Step 5: Run all tests, verify pass**

- [ ] **Step 6: Commit**

```bash
git commit -m "feat(connector): add supports_async capability and is_async_connector concept"
```

---

### Task 6: `orm::async_db<DB>` handle

**Files:**
- Create: `lib/include/ORM/connector/async_db.hpp`
- Modify: `lib/include/ORM/connector/db.hpp` (add async overload)
- Test: `tests/unit/test_async_pool.cpp`

- [ ] **Step 1: Write failing tests with async MockDB**

- [ ] **Step 2: Implement `async_db<DB>`**

```cpp
template <typename DB>
    requires is_async_connector<DB>
class async_db
{
public:
    explicit async_db(DB& connection, IoContext& ctx)
        : conn_(&connection), ctx_(&ctx) {}

    template <typename Query>
    [[nodiscard]] auto operator<<(Query q) -> Task</* result type */>
    {
        co_return co_await connector_trait<DB>::async_execute(
            *conn_, *ctx_, std::move(q));
    }

    // ... async execute, find_one, prepare ...

private:
    DB* conn_;
    IoContext* ctx_;
};
```

- [ ] **Step 3: Run all tests, verify pass**

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(connector): add async_db<DB> coroutine-aware handle"
```

---

### Task 7: Async connection pool and transaction guard

**Files:**
- Create: `lib/include/ORM/db/connectors/ThreadSafety/async_transaction.hpp`
- Modify: `lib/include/ORM/db/connectors/ThreadSafety/thread_safety.hpp`
- Test: `tests/unit/test_async_pool.cpp`

- [ ] **Step 1: Write failing tests**

```cpp
TEST(AsyncPoolTest, AcquireReturnsExclusiveConnection)
{
    // Test with async MockDB
}

TEST(AsyncTransactionTest, CommitSendsCommit)
{
    // Test BEGIN -> queries -> COMMIT sequence
}

TEST(AsyncTransactionTest, DestructorRollsBack)
{
    // Test that destruction without commit sends ROLLBACK
}

TEST(AsyncTransactionTest, TwoCoroutinesDifferentConnections)
{
    // Two coroutines acquire from same pool — get different connections
    // Run interleaved queries — verify no cross-contamination
}
```

- [ ] **Step 2: Implement `async_connection_pool<DB, N>`**

Key difference from sync `connection_pool`: `acquire()` returns `Task<async_connection_guard<DB>>` and suspends via `IoContext::post()` instead of blocking on a condition variable.

- [ ] **Step 3: Implement `async_transaction_guard<DB>`**

Same as sync `transaction_guard` but `begin()`, `commit()`, `rollback()` are coroutines.

- [ ] **Step 4: Run all tests, verify pass**

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(async): add async_connection_pool and async_transaction_guard"
```

---

## Chunk 3: Per-Connector Async Implementations

### Task 8: MySQL async connector

**Files:**
- Create: `lib/include/ORM/db/connectors/MySQLDB/mysql_async.hpp`
- Test: `tests/unit/test_async_mysql_connector.cpp`
- Integration: `tests/integration/test_async_mysql_live.cpp`

- [ ] **Step 1: Write failing unit tests** (mock-based, no real MySQL)

- [ ] **Step 2: Implement `AsyncMySQLDB` struct and `connector_trait` specialization**

Key pattern — `mysql_async()` helper:
```cpp
template <typename RetT, typename StartFn, typename ContFn>
Task<RetT> mysql_async(IoContext& ctx, MYSQL* conn, StartFn start_fn, ContFn cont_fn)
{
    RetT ret{};
    int status = start_fn(&ret);
    while (status)
    {
        int fd = mysql_get_socket(conn);
        if (status & MYSQL_WAIT_WRITE)
            co_await ctx.watch_writable(fd);
        else
            co_await ctx.watch_readable(fd);
        status = cont_fn(&ret, status);
    }
    co_return ret;
}
```

Reuse all SQL rendering and hydration from `mysql_live_detail` namespace.

- [ ] **Step 3: Run unit tests, verify pass**

- [ ] **Step 4: Write integration test** (requires Docker MySQL)

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(mysql): add async MySQL connector using _start/_cont API"
```

---

### Task 9: PostgreSQL async connector

**Files:**
- Create: `lib/include/ORM/db/connectors/PostgreSQLDB/postgresql_async.hpp`
- Test: `tests/unit/test_async_postgresql_connector.cpp`
- Integration: `tests/integration/test_async_postgresql_live.cpp`

- [ ] **Step 1: Write failing unit tests**

- [ ] **Step 2: Implement `AsyncPostgreSQLDB`**

Key pattern — libpq async state machine:
```cpp
Task<PGresult*> pg_async_query(IoContext& ctx, PGconn* conn,
                                std::string_view sql, ...)
{
    PQsetnonblocking(conn, 1);
    PQsendQueryParams(conn, sql.data(), ...);
    int fd = PQsocket(conn);

    // Flush send buffer
    while (true)
    {
        int flush = PQflush(conn);
        if (flush == 0) break;
        if (flush == -1) /* error */;
        co_await ctx.watch_writable(fd);
    }

    // Wait for result
    while (PQisBusy(conn))
    {
        co_await ctx.watch_readable(fd);
        PQconsumeInput(conn);
    }

    co_return PQgetResult(conn);
}
```

Reuse all SQL rendering from `pg_live_detail` namespace.

- [ ] **Step 3: Run unit tests, verify pass**

- [ ] **Step 4: Write integration test**

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(postgresql): add async PostgreSQL connector using PQsendQuery"
```

---

### Task 10: Redis async connector

**Files:**
- Create: `lib/include/ORM/db/connectors/RedisDB/redis_async.hpp`
- Test: `tests/unit/test_async_redis_connector.cpp`
- Integration: `tests/integration/test_async_redis_live.cpp`

- [ ] **Step 1: Write failing unit tests**

- [ ] **Step 2: Implement `AsyncRedisDB`**

Strategy: hiredis async API with event loop adapter — `redisAsyncSetConnectCallback`, `redisAsyncSetDisconnectCallback`, `redisAsyncCommand` + bridge `addRead`/`addWrite`/`delRead`/`delWrite` to `IoContext::watch_readable`/`watch_writable`.

Reuse all command rendering from `redis_live_detail` namespace.

- [ ] **Step 3: Run unit tests, verify pass**

- [ ] **Step 4: Write integration test**

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(redis): add async Redis connector using hiredis async API"
```

---

### Task 11: Cassandra async connector

**Files:**
- Create: `lib/include/ORM/db/connectors/CassandraDB/cassandra_async.hpp`
- Test: `tests/unit/test_async_cassandra_connector.cpp`
- Integration: `tests/integration/test_async_cassandra_live.cpp`

- [ ] **Step 1: Write failing unit tests**

- [ ] **Step 2: Implement `AsyncCassandraDB`**

Strategy: `CassFuture` callback bridge — `cass_future_set_callback()` invokes `IoContext::post()` which resumes the coroutine on the event loop thread.

```cpp
Task<CassResult*> cass_async_execute(IoContext& ctx, CassSession* session,
                                      CassStatement* stmt)
{
    CassFuture* future = cass_session_execute(session, stmt);
    CassResult* result = co_await CassFutureAwaitable{future, ctx};
    co_return result;
}
```

Reuse all CQL rendering and binding from `cass_live_detail` namespace.

- [ ] **Step 3: Run unit tests, verify pass**

- [ ] **Step 4: Write integration test**

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(cassandra): add async Cassandra connector via CassFuture bridge"
```

---

### Task 12: MongoDB async connector

**Files:**
- Create: `lib/include/ORM/db/connectors/MongoDB/mongodb_async.hpp`
- Test: `tests/unit/test_async_mongodb_connector.cpp`
- Integration: `tests/integration/test_async_mongodb_live.cpp`

- [ ] **Step 1: Write failing unit tests**

- [ ] **Step 2: Implement `AsyncMongoDBLive`**

Strategy: Thread pool offload — `run_on_pool()` wrapping the sync mongoc calls. Each acquired connection is used exclusively on the pool thread, then result is marshalled back.

Reuse all BSON rendering from `mongo_live_detail` namespace.

- [ ] **Step 3: Run unit tests, verify pass**

- [ ] **Step 4: Write integration test**

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(mongodb): add async MongoDB connector via thread pool offload"
```

---

### Task 13: Neo4j async connector

**Files:**
- Create: `lib/include/ORM/db/connectors/Neo4jDB/neo4j_async.hpp`
- Test: `tests/unit/test_async_neo4j_connector.cpp`

- [ ] **Step 1: Write failing unit tests**

- [ ] **Step 2: Implement `AsyncNeo4jLiveDB`**

Strategy: Thread pool offload (same as MongoDB). Reuse `neo4j_live_detail` namespace.

- [ ] **Step 3: Run unit tests, verify pass**

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(neo4j): add async Neo4j connector via thread pool offload"
```

---

### Task 14: SQLite async connector

**Files:**
- Create: `lib/include/ORM/db/connectors/SQLite/sqlite_async.hpp`
- Test: `tests/unit/test_async_sqlite_connector.cpp`
- Integration: `tests/integration/test_async_sqlite_live.cpp`

- [ ] **Step 1: Write failing unit tests**

- [ ] **Step 2: Implement `AsyncSQLiteDB`**

Strategy: Dedicated worker thread (single-writer constraint). All operations dispatched via `run_on_pool(single_thread_pool, ...)`. Enable WAL mode for concurrent readers.

Reuse `sqlite_detail` namespace.

- [ ] **Step 3: Run unit tests, verify pass**

- [ ] **Step 4: Write integration test**

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(sqlite): add async SQLite connector via dedicated worker thread"
```

---

## Chunk 4: Cross-Cutting Tests

### Task 15: Transaction isolation tests

**Files:**
- Create: `tests/integration/test_async_transaction_isolation.cpp`

- [ ] **Step 1: Write tests that verify transaction isolation across coroutines**

Test scenario:
1. Launch N coroutines on the same `async_connection_pool`
2. Each coroutine: acquire connection → begin transaction → insert unique row → delay → verify only own row visible → commit
3. After all complete: verify all N rows present
4. Repeat with some coroutines rolling back — verify rolled-back rows absent

- [ ] **Step 2: Write tests for interleaved multi-thread execution**

Test scenario:
1. IoContext with 4 worker threads
2. 20 coroutines each executing a transaction
3. Verify no query from coroutine A appears inside coroutine B's transaction view

- [ ] **Step 3: Run all tests, verify pass**

- [ ] **Step 4: Commit**

```bash
git commit -m "test(async): add transaction isolation tests across coroutines and threads"
```

---

### Task 16: Existing test compatibility

**Files:**
- Modify: `tests/unit/CMakeLists.txt`
- Modify: `tests/integration/CMakeLists.txt`
- Verify: All existing tests still pass unchanged

- [ ] **Step 1: Verify all existing sync tests still compile and pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: All pre-existing tests PASS (sync connectors untouched)

- [ ] **Step 2: Add async test executables to CMake**

Async unit tests: separate executable `test_async_unit` linked to `orm::async` + GTest.
Async integration tests: per-connector executables gated on `ORM_ENABLE_*` + `ORM_ENABLE_ASYNC`.

- [ ] **Step 3: Commit**

```bash
git commit -m "test: integrate async test targets into CMake, verify sync tests unchanged"
```

---

## Chunk 5: Benchmark Infrastructure

### Task 17: Benchmark framework setup

**Files:**
- Create: `benchmarks/CMakeLists.txt`
- Create: `benchmarks/bench_sync_vs_async.cpp`
- Create: `benchmarks/bench_concurrent_queries.cpp`
- Create: `benchmarks/bench_connection_pool.cpp`
- Create: `docker/docker-compose.benchmark.yml`
- Create: `benchmarks/run_benchmarks.sh`
- Modify: `CMakeLists.txt` (root — add `ORM_BUILD_BENCHMARKS` option)

- [ ] **Step 1: Add Google Benchmark dependency**

```cmake
# benchmarks/CMakeLists.txt
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
    GIT_SHALLOW TRUE
)
set(BENCHMARK_ENABLE_TESTING OFF)
FetchContent_MakeAvailable(benchmark)
```

- [ ] **Step 2: Implement `bench_sync_vs_async.cpp`**

Metrics to measure:
- **Throughput:** queries/second for sync vs async (same query, same DB)
- **Latency:** p50/p95/p99 per query
- **CPU utilization:** wall time vs CPU time ratio

Structure:
```cpp
static void BM_SyncInsert(benchmark::State& state) { /* sync loop */ }
static void BM_AsyncInsert(benchmark::State& state) { /* async loop */ }
BENCHMARK(BM_SyncInsert)->Arg(1)->Arg(10)->Arg(100);
BENCHMARK(BM_AsyncInsert)->Arg(1)->Arg(10)->Arg(100);
```

- [ ] **Step 3: Implement `bench_concurrent_queries.cpp`**

Metrics:
- N concurrent queries (N = 1, 10, 50, 100, 500) — measure total wall time
- Compare sync (N threads) vs async (1 thread, N coroutines)

- [ ] **Step 4: Implement `bench_connection_pool.cpp`**

Metrics:
- Pool contention: measure acquire() latency under N concurrent requestors
- Compare blocking pool vs async pool

- [ ] **Step 5: Create docker-compose.benchmark.yml**

Separate compose file with:
- Database services (same as existing)
- Benchmark runner service with resource limits (CPU pinning, memory)

- [ ] **Step 6: Create `run_benchmarks.sh`**

```bash
#!/usr/bin/env bash
# 1. docker compose -f docker/docker-compose.benchmark.yml up -d
# 2. Wait for healthy databases
# 3. Run each benchmark binary, capture JSON output
# 4. docker compose down
# 5. Print summary table
```

- [ ] **Step 7: Run benchmarks locally to verify they work**

- [ ] **Step 8: Commit**

```bash
git commit -m "feat(bench): add sync-vs-async benchmark infrastructure with Docker"
```

---

## Chunk 6: Documentation and CMake Polish

### Task 18: Documentation updates

**Files:**
- Modify: `README.md`
- Update: `docs/superpowers/specs/2026-04-06-async-connectors-*.md` (mark as implemented)

- [ ] **Step 1: Update README.md**

Add section on async connectors:
- New CMake option: `ORM_ENABLE_ASYNC`
- Usage example: `co_await async_db << select(...)`
- Transaction example
- Platform requirements (io_uring on Linux, kqueue on macOS, IOCP on Windows)

- [ ] **Step 2: Update spec files with implementation status**

- [ ] **Step 3: Commit**

```bash
git commit -m "docs: update README and specs for async connector implementation"
```

---

## Summary: Task Dependency Graph

```
Task 1 (Task<T>) ─────┐
Task 2 (Cancellation) ─┤
Task 3 (IoContext) ─────┤
Task 4 (ThreadPool) ────┴─── Task 5 (Capabilities) ─── Task 6 (async_db) ─── Task 7 (Pool/Txn)
                                                                                      │
                              ┌───────────────────────────────────────────────────────┘
                              ▼
                    Tasks 8-14 (Per-connector, parallelizable)
                              │
                              ▼
                    Task 15 (Transaction isolation tests)
                    Task 16 (Existing test compat)
                              │
                              ▼
                    Task 17 (Benchmarks)
                    Task 18 (Documentation)
```

Tasks 8-14 are **independent** and can be implemented in parallel once Tasks 1-7 are complete.
