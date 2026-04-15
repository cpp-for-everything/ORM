# Async Connectors — Migration Plan, File Layout, and Test Strategy

**Date:** 2026-04-06
**Status:** Design notes — pre-implementation
**Author:** Design session with Alex Tsvetanov

---

## 1. Guiding Principles

1. **Additive, not destructive.** Sync connectors remain untouched. Async connectors are new types alongside them.
2. **Share rendering logic.** The `*_live_detail::` namespaces are pure computation — used by both sync and async connectors.
3. **Incremental delivery.** Each async connector is independently useful and testable. No big-bang release.
4. **Test parity.** Every async connector gets the same coverage as its sync counterpart: unit tests (MockDB-style), integration tests (Docker containers).
5. **Zero new external dependencies** for the core infrastructure (`Task<T>`, `EventLoop`, `IOAwaitable`). Driver-level dependencies remain the same C libraries.

---

## 2. Proposed File Layout

### 2.1 New Async Infrastructure

```
lib/include/ORM/async/
├── event_loop.hpp              ← Platform-agnostic EventLoop interface
├── io_awaitable.hpp            ← IOAwaitable (fd-based suspension)
├── task.hpp                    ← Task<T> and Task<void> coroutine types
├── thread_pool.hpp             ← ThreadPool (for blocking offload)
├── pool_awaitable.hpp          ← PoolAwaitable / run_on_pool()
├── cassandra_awaitable.hpp     ← CassFuture → coroutine bridge
├── hiredis_adapter.hpp         ← hiredis event loop adapter
└── concepts.hpp                ← is_async_connector<DB> concept

lib/src/ORM/async/
├── event_loop_iouring.cpp      ← Linux io_uring backend
├── event_loop_epoll.cpp        ← Linux epoll fallback (kernels < 5.1)
├── event_loop_iocp.cpp         ← Windows IOCP backend
├── event_loop_kqueue.cpp       ← macOS kqueue backend
└── thread_pool.cpp             ← ThreadPool implementation
```

### 2.2 New Async Connectors

Each async connector lives alongside its sync counterpart in the same directory:

```
lib/include/ORM/db/connectors/
├── MySQLDB/
│   ├── mysql_db.hpp            ← (existing) entity definitions
│   ├── mysql_live.hpp          ← (existing) sync MySQLLiveDB + connector_trait
│   └── mysql_async.hpp         ← NEW: AsyncMySQLDB + connector_trait (coroutine)
├── PostgreSQLDB/
│   ├── postgresql_db.hpp       ← (existing)
│   ├── postgresql_live.hpp     ← (existing) sync PostgreSQLLiveDB
│   └── postgresql_async.hpp    ← NEW: AsyncPgDB + connector_trait (coroutine)
├── MongoDB/
│   ├── mongodb_db.hpp          ← (existing)
│   ├── mongodb_live.hpp        ← (existing) sync MongoDBLive
│   └── mongodb_async.hpp       ← NEW: AsyncMongoDBLive + connector_trait (thread pool)
├── CassandraDB/
│   ├── cassandra_db.hpp        ← (existing)
│   ├── cassandra_live.hpp      ← (existing) sync CassandraLiveDB
│   └── cassandra_async.hpp     ← NEW: AsyncCassandraDB + connector_trait (callback bridge)
├── RedisDB/
│   ├── redis_db.hpp            ← (existing)
│   ├── redis_live.hpp          ← (existing) sync RedisLiveDB
│   └── redis_async.hpp         ← NEW: AsyncRedisDB + connector_trait (hiredis/async.h)
├── Neo4jDB/
│   ├── neo4j_db.hpp            ← (existing)
│   ├── neo4j_live.hpp          ← (existing) sync Neo4jLiveDB
│   └── neo4j_async.hpp         ← NEW: AsyncNeo4jDB + connector_trait (thread pool)
├── SQLite/
│   ├── sqlite_db.hpp           ← (existing) sync SQLiteDB
│   └── sqlite_async.hpp        ← NEW: AsyncSQLiteDB + connector_trait (worker thread)
├── ThreadSafety/
│   ├── thread_safety.hpp       ← (existing) connection_pool, transaction_guard
│   └── async_pool.hpp          ← NEW: async connection pool (optional)
└── WireProtocol/
    └── wire_protocol.hpp       ← (existing) — io_uring_awaitable already sketched here
```

### 2.3 Updated Core Headers

```
lib/include/ORM/connector/
├── capabilities.hpp            ← ADD: supports_async capability tag
├── trait.hpp                   ← ADD: is_async_connector concept
├── db.hpp                      ← ADD: async operator<< overload (if constexpr gated)
├── async_db.hpp                ← NEW: async_db<DB> class (alternative to adding to db.hpp)
└── prepared_query.hpp          ← (existing, no changes needed for MVP)
```

### 2.4 Design Decision: `db.hpp` vs. `async_db.hpp`

Two options for the user-facing async API:

**Option A — Extend `db<DB>` with `if constexpr` gating:**
```cpp
template <typename DB>
class db
{
    template <typename Query>
    auto operator<<(Query q)
    {
        if constexpr (is_async_connector<DB>)
            return connector_trait<DB>::execute(*conn_, std::move(q));  // returns Task<>
        else
            return connector_trait<DB>::execute(*conn_, std::move(q));  // returns result<>
    }
};
```
Pros: Single class, no code duplication.
Cons: Return type differs based on DB — may confuse users.

**Option B — Separate `async_db<DB>` class:**
```cpp
template <typename DB>
    requires is_async_connector<DB>
class async_db
{
    template <typename Query>
    auto operator<<(Query q) -> Task</* deduced */>
    {
        co_return co_await connector_trait<DB>::execute(*conn_, std::move(q));
    }
};
```
Pros: Clear separation; `db<DB>` never becomes a coroutine.
Cons: Separate class to maintain; users must know which to use.

**Recommendation:** Option A for simplicity. The return type is already deduced (`auto`), so the caller sees `result<>` or `Task<result<>>` naturally. The sync path never includes `<coroutine>` because the `if constexpr` branch is not instantiated.

---

## 3. Rendering Logic Extraction

Currently, each connector's rendering logic (e.g., `mysql_live_detail::render_wheres()`) is defined inside the `*_live.hpp` file, co-located with the sync connector. For the async connector to reuse it without including the sync connector, we have two options:

### 3.1 Option A: Include the sync header from the async header

```cpp
// mysql_async.hpp
#include "ORM/db/connectors/MySQLDB/mysql_live.hpp"  // brings in mysql_live_detail::
```

Pros: Zero refactoring. The rendering logic is already compiled.
Cons: The async header also pulls in the sync `MySQLLiveDB` type and its `connector_trait<>` specialization. This is harmless but slightly noisy.

### 3.2 Option B: Extract rendering into a shared detail header

```cpp
// mysql_detail.hpp — NEW: pure rendering logic, no connector type
namespace orm::mysql_live_detail { ... }

// mysql_live.hpp — includes mysql_detail.hpp + defines MySQLLiveDB
// mysql_async.hpp — includes mysql_detail.hpp + defines AsyncMySQLDB
```

Pros: Clean separation; async header doesn't pull in sync type.
Cons: Requires splitting every `*_live.hpp` into `*_detail.hpp` + `*_live.hpp`.

**Recommendation:** Option A for MVP. The rendering namespaces are small and the include overhead is negligible. Extraction can be done later as a cleanup pass. This follows the "clean over minimal diff" rule in a pragmatic way — we avoid a large refactoring that doesn't change behavior.

---

## 4. Phased Implementation Plan

### Phase 0: Core Infrastructure (prerequisite for all async connectors)

| # | Task | Files | Tests |
|---|---|---|---|
| 0.1 | Implement `Task<T>` and `Task<void>` | `async/task.hpp` | `tests/unit/test_task.cpp` — verify lazy start, value return, exception propagation, symmetric transfer, move-only semantics |
| 0.2 | Implement `ThreadPool` | `async/thread_pool.hpp`, `async/thread_pool.cpp` | `tests/unit/test_thread_pool.cpp` — verify post(), global(), shutdown |
| 0.3 | Implement `PoolAwaitable` / `run_on_pool()` | `async/pool_awaitable.hpp` | `tests/unit/test_pool_awaitable.cpp` — verify coroutine suspension and cross-thread resumption |
| 0.4 | Implement `EventLoop` (kqueue backend for macOS dev, epoll for Linux CI) | `async/event_loop.hpp`, `async/event_loop_kqueue.cpp`, `async/event_loop_epoll.cpp` | `tests/unit/test_event_loop.cpp` — verify fd watch/resume with pipe() fds |
| 0.5 | Implement `IOAwaitable` | `async/io_awaitable.hpp` | Tested via event loop tests |
| 0.6 | Add `is_async_connector` concept | `connector/trait.hpp` or `async/concepts.hpp` | Compile-time concept check tests |
| 0.7 | Extend `db<DB>::operator<<` for async path | `connector/db.hpp` | Tested via connector-level tests |

**Estimated effort:** 3–5 days.

### Phase 1: Cassandra Async (lowest risk, proves infrastructure)

| # | Task | Files | Tests |
|---|---|---|---|
| 1.1 | Implement `CassFutureAwaitable` | `async/cassandra_awaitable.hpp` | `tests/unit/test_cassandra_awaitable.cpp` |
| 1.2 | Implement `AsyncCassandraDB` tag type | `connectors/CassandraDB/cassandra_async.hpp` | — |
| 1.3 | Implement `connector_trait<AsyncCassandraDB>` | Same file | `tests/unit/test_cassandra_async_connector.cpp` |
| 1.4 | Integration test | — | `tests/integration/test_cassandra_async_live.cpp` (Docker) |

**What changes vs. sync:** Replace `cass_future_wait(f)` with `co_await CassFutureAwaitable{f}` in 4 places. Everything else is identical.

**Estimated effort:** 1 day.

### Phase 2: PostgreSQL Async (proves fd-based event loop path)

| # | Task | Files | Tests |
|---|---|---|---|
| 2.1 | Implement `AsyncPgDB` tag type with async connect | `connectors/PostgreSQLDB/postgresql_async.hpp` | — |
| 2.2 | Implement `exec_async()` helper (send/flush/wait state machine) | Same file | — |
| 2.3 | Implement `connector_trait<AsyncPgDB>` (SELECT, INSERT, UPDATE, DELETE) | Same file | `tests/unit/test_postgresql_async_connector.cpp` |
| 2.4 | Implement async transaction helpers (begin, commit, rollback) | Same file | Tested via integration |
| 2.5 | Integration test | — | `tests/integration/test_postgresql_async_live.cpp` (Docker) |

**What changes vs. sync:** `PQexecParams()` → `PQsendQueryParams()` + flush/wait loop. Hydration identical.

**Estimated effort:** 2–3 days.

### Phase 3: MongoDB Async (proves thread pool path)

| # | Task | Files | Tests |
|---|---|---|---|
| 3.1 | Implement `AsyncMongoDBLive` tag type | `connectors/MongoDB/mongodb_async.hpp` | — |
| 3.2 | Implement `connector_trait<AsyncMongoDBLive>` wrapping sync calls in `run_on_pool()` | Same file | `tests/unit/test_mongodb_async_connector.cpp` |
| 3.3 | Integration test | — | `tests/integration/test_mongodb_async_live.cpp` (Docker) |

**What changes vs. sync:** Each `execute()` overload wraps the sync body in `co_return co_await run_on_pool([&]{...})`.

**Estimated effort:** 1 day.

### Phase 4: MySQL Async (most complex, builds on proven event loop)

| # | Task | Files | Tests |
|---|---|---|---|
| 4.1 | Implement `AsyncMySQLDB` tag type with async connect | `connectors/MySQLDB/mysql_async.hpp` | — |
| 4.2 | Implement the `_start`/`_cont` coroutine helper | Same file or `async/mysql_helpers.hpp` | — |
| 4.3 | Implement async `exec_select()` (non-prepared path) | Same file | — |
| 4.4 | Implement async `exec_select_prepared()` (5-step async) | Same file | — |
| 4.5 | Implement async `exec_prepared()` (INSERT/UPDATE/DELETE) | Same file | — |
| 4.6 | Implement `connector_trait<AsyncMySQLDB>` | Same file | `tests/unit/test_mysql_async_connector.cpp` |
| 4.7 | Integration test | — | `tests/integration/test_mysql_async_live.cpp` (Docker) |

**What changes vs. sync:** Every `mysql_X()` call becomes a `_start`/`_cont` loop with `co_await IOAwaitable`.

**Estimated effort:** 3–4 days.

### Phase 5: Redis Async (requires hiredis adapter)

| # | Task | Files | Tests |
|---|---|---|---|
| 5.1 | Implement hiredis event loop adapter | `async/hiredis_adapter.hpp` | `tests/unit/test_hiredis_adapter.cpp` |
| 5.2 | Implement `RedisCommandAwaitable` | Same file | — |
| 5.3 | Implement `AsyncRedisDB` tag type | `connectors/RedisDB/redis_async.hpp` | — |
| 5.4 | Implement `connector_trait<AsyncRedisDB>` | Same file | `tests/unit/test_redis_async_connector.cpp` |
| 5.5 | Integration test | — | `tests/integration/test_redis_async_live.cpp` (Docker) |

**Estimated effort:** 2–3 days.

### Phase 6: SQLite Async (worker thread)

| # | Task | Files | Tests |
|---|---|---|---|
| 6.1 | Implement `AsyncSQLiteDB` with worker thread | `connectors/SQLite/sqlite_async.hpp` | — |
| 6.2 | Implement `connector_trait<AsyncSQLiteDB>` | Same file | `tests/unit/test_sqlite_async_connector.cpp` |
| 6.3 | Unit test (no Docker needed — SQLite is in-process) | — | Same file |

**Estimated effort:** 1 day.

### Phase 7: Neo4j Async (thread pool offload)

| # | Task | Files | Tests |
|---|---|---|---|
| 7.1 | Implement `AsyncNeo4jDB` tag type | `connectors/Neo4jDB/neo4j_async.hpp` | — |
| 7.2 | Implement `connector_trait<AsyncNeo4jDB>` wrapping sync calls | Same file | `tests/unit/test_neo4j_async_connector.cpp` |
| 7.3 | Integration test | — | `tests/integration/test_neo4j_async_live.cpp` (Docker) |

**Estimated effort:** 1 day.

### Phase 8: Polish and Integration

| # | Task |
|---|---|
| 8.1 | Add `supports_async` capability tag to `capabilities.hpp` |
| 8.2 | Add async connection pool (`async_pool.hpp`) — `co_await pool.acquire()` instead of blocking `pool.acquire()` |
| 8.3 | Add async transaction guard — `co_await begin_transaction_async(db)` |
| 8.4 | Add `when_all()` combinator for concurrent Task execution |
| 8.5 | Update `README.md` with async usage examples |
| 8.6 | Update `doc/obsidian/` with async architecture documentation |
| 8.7 | CI pipeline: add async integration test targets to `ci.yaml` |

**Estimated effort:** 2–3 days.

---

## 5. Total Estimated Effort

| Phase | Days |
|---|---|
| Phase 0: Infrastructure | 3–5 |
| Phase 1: Cassandra | 1 |
| Phase 2: PostgreSQL | 2–3 |
| Phase 3: MongoDB | 1 |
| Phase 4: MySQL | 3–4 |
| Phase 5: Redis | 2–3 |
| Phase 6: SQLite | 1 |
| Phase 7: Neo4j | 1 |
| Phase 8: Polish | 2–3 |
| **Total** | **16–22 days** |

---

## 6. Test Strategy

### 6.1 Unit Tests (No Database Required)

Each async connector gets a unit test that verifies:

1. **Coroutine mechanics:** `Task<T>` starts lazily, returns the correct value, propagates exceptions.
2. **Rendering reuse:** Async connector generates the same SQL/CQL/Cypher as the sync connector for the same query IR.
3. **Awaitable contracts:** `IOAwaitable::await_ready()` returns false; `CassFutureAwaitable::await_ready()` returns true for already-resolved futures.
4. **Type correctness:** `connector_trait<AsyncXDB>::execute()` returns `Task<result<...>>`.
5. **Concept satisfaction:** `is_async_connector<AsyncMySQLDB>` is true; `is_async_connector<MySQLLiveDB>` is false.

### 6.2 Integration Tests (Docker Containers)

Each async connector gets an integration test identical in structure to its sync counterpart (`tests/integration/test_*_live.cpp`), but using the async tag type and `co_await`:

```cpp
// test_postgresql_async_live.cpp
TEST(PostgreSQLAsyncLive, SelectWithParams)
{
    // Run inside a coroutine driver
    run_coroutine([&]() -> Task<void> {
        auto db = co_await AsyncPgDB::connect("host=localhost ...");
        orm::async_db<AsyncPgDB> orm_db(db);

        auto result = co_await orm_db.execute(
            orm::select(orm::field<&User::id>, orm::field<&User::name>)
                .where(orm::field<&User::id> == orm::ph<int>),
            42);

        EXPECT_FALSE(result.empty());
        co_return;
    });
}
```

The `run_coroutine()` helper creates an event loop, starts the coroutine, and drives the loop until the coroutine completes:

```cpp
template <typename F>
void run_coroutine(F&& f)
{
    auto task = f();
    EventLoop::current().run_until(task);
}
```

### 6.3 Docker Test Infrastructure

The existing `docker/` directory and `tests/integration/DockerRun.cmake` already provide Docker containers for each database. The async integration tests reuse the same containers — the database doesn't know or care whether the client is sync or async.

### 6.4 CI Pipeline

Add to `.github/workflows/ci.yaml`:

```yaml
- name: Build async connectors
  run: cmake --build build --target orm_async_tests

- name: Run async unit tests
  run: ctest --test-dir build -R "test_.*_async" --output-on-failure

- name: Run async integration tests
  run: |
    docker compose -f docker/docker-compose.yml up -d
    ctest --test-dir build -R "test_.*_async_live" --output-on-failure
    docker compose -f docker/docker-compose.yml down
```

---

## 7. CMake Integration

### 7.1 New CMake Targets

```cmake
# lib/CMakeLists.txt additions

# Async infrastructure library
add_library(orm_async STATIC
    src/ORM/async/thread_pool.cpp
    src/ORM/async/event_loop_${ORM_EVENT_LOOP_BACKEND}.cpp
)
target_link_libraries(orm_async PUBLIC orm_core)
target_compile_features(orm_async PUBLIC cxx_std_20)  # coroutines require C++20

# Platform detection for event loop backend
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Check for io_uring support
    find_package(PkgConfig)
    pkg_check_modules(URING QUIET liburing)
    if(URING_FOUND)
        set(ORM_EVENT_LOOP_BACKEND "iouring")
        target_link_libraries(orm_async PRIVATE ${URING_LIBRARIES})
    else()
        set(ORM_EVENT_LOOP_BACKEND "epoll")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(ORM_EVENT_LOOP_BACKEND "kqueue")
elseif(WIN32)
    set(ORM_EVENT_LOOP_BACKEND "iocp")
endif()
```

### 7.2 Async Connector Availability Flags

```cmake
# Similar to existing ORM_MYSQL_LIVE_AVAILABLE, etc.
if(ORM_MYSQL_LIVE_AVAILABLE)
    target_compile_definitions(orm_async PUBLIC ORM_MYSQL_ASYNC_AVAILABLE)
endif()
if(ORM_POSTGRESQL_LIVE_AVAILABLE)
    target_compile_definitions(orm_async PUBLIC ORM_POSTGRESQL_ASYNC_AVAILABLE)
endif()
# ... etc for each connector
```

### 7.3 Coroutine Compiler Flags

```cmake
# GCC and Clang require -fcoroutines for C++20 coroutines
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(orm_async PUBLIC -fcoroutines)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang enables coroutines with -std=c++20, no extra flag needed
endif()
# MSVC enables coroutines with /std:c++20, no extra flag needed
```

---

## 8. User-Facing API Example

### 8.1 Sync (Existing — No Changes)

```cpp
#include <ORM/ORM.hpp>
#include <ORM/db/connectors/PostgreSQLDB/postgresql_live.hpp>

int main()
{
    auto conn = orm::PostgreSQLLiveDB::connect("host=localhost dbname=mydb ...");
    orm::db<orm::PostgreSQLLiveDB> db(conn);

    auto result = db.execute(
        orm::select(orm::field<&User::id>, orm::field<&User::name>)
            .where(orm::field<&User::id> == orm::ph<int>),
        42);

    for (const auto& row : result)
        std::println("id={}, name={}", std::get<0>(row), std::get<1>(row));
}
```

### 8.2 Async (New)

```cpp
#include <ORM/ORM.hpp>
#include <ORM/db/connectors/PostgreSQLDB/postgresql_async.hpp>
#include <ORM/async/task.hpp>
#include <ORM/async/event_loop.hpp>

orm::async::Task<void> app()
{
    auto conn = co_await orm::AsyncPgDB::connect("host=localhost dbname=mydb ...");
    orm::db<orm::AsyncPgDB> db(conn);

    auto result = co_await db.execute(
        orm::select(orm::field<&User::id>, orm::field<&User::name>)
            .where(orm::field<&User::id> == orm::ph<int>),
        42);

    for (const auto& row : result)
        std::println("id={}, name={}", std::get<0>(row), std::get<1>(row));
}

int main()
{
    auto task = app();
    orm::async::EventLoop::current().run_until(task);
}
```

**Key observation:** The query construction is identical. Only the connection type, `co_await`, and the event loop driver change.

### 8.3 Concurrent Async Operations

```cpp
orm::async::Task<void> concurrent_example()
{
    auto pg = co_await orm::AsyncPgDB::connect("...");
    auto mongo = co_await orm::AsyncMongoDBLive::connect("...", "mydb");

    orm::db<orm::AsyncPgDB> pg_db(pg);
    orm::db<orm::AsyncMongoDBLive> mongo_db(mongo);

    // These two queries execute concurrently — one on the event loop (PG),
    // one on the thread pool (MongoDB).
    auto [pg_result, mongo_result] = co_await when_all(
        pg_db.execute(orm::select(orm::field<&User::id>), 42),
        mongo_db.execute(orm::select(orm::field<&Product::name>), "widget")
    );

    // Both results are ready here.
}
```

---

## 9. Risks and Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| **MariaDB Connector/C `_start`/`_cont` API not available on all platforms** | MySQL async connector won't compile | Feature-gate with `#ifdef MYSQL_OPT_NONBLOCK`; fall back to thread pool offload if unavailable |
| **io_uring not available on CI Linux kernel** | Event loop tests fail | Fall back to epoll; detect at CMake time |
| **Coroutine support varies across compilers** | Build failures | Require C++20 minimum; test on GCC 12+, Clang 14+, MSVC 19.30+ |
| **Thread pool bridge latency** | MongoDB/Neo4j/SQLite async connectors add overhead vs. sync | Expected and documented; the benefit is freeing the caller's thread, not reducing latency |
| **hiredis/async.h header not present in all hiredis installations** | Redis async connector won't compile | Feature-gate with `#ifdef ORM_HIREDIS_ASYNC_AVAILABLE`; detect in CMake |
| **CassFuture callback thread vs. event loop thread** | Coroutine resumes on wrong thread | Document; add optional `resume_on(EventLoop&)` adapter for safety |
| **Coroutine parameter lifetime (dangling references)** | Use-after-free crashes | Enforce by-value capture in coroutine signatures; lint rule |

---

## 10. Backward Compatibility Guarantee

- **All existing sync connector code compiles and runs unchanged.**
- **No existing headers are modified** in a way that changes behavior.
- **`db.hpp` changes are purely additive** — a new `if constexpr` branch that is never instantiated for sync connectors.
- **No new dependencies** for users who don't use async connectors. The `ORM/async/` headers are only included when async connectors are used.
- **CMake changes are additive** — new targets, not modifications to existing ones.

---

## 11. Post-MVP Roadmap

After all 7 async connectors are functional:

1. **Connection pool for async connectors** — `async_connection_pool<DB, N>` with `co_await acquire()` instead of blocking wait.
2. **Pipeline mode for PostgreSQL** — `PQenterPipelineMode` for concurrent queries over a single connection.
3. **Cancellation** — `CancellationToken` threaded through `Task<T>` for cooperative timeout/cancellation.
4. **Structured concurrency** — `when_all()`, `when_any()`, `TaskGroup` combinators.
5. **Raw Bolt protocol** — implement Neo4j's Bolt protocol directly for true async without thread pool.
6. **Sender/Receiver (P2300)** — migrate `Task<T>` to the standard sender/receiver model when C++26 ships it.
7. **io_uring direct I/O** — for SQLite on Linux, use io_uring `IORING_OP_READ`/`IORING_OP_WRITE` to make file I/O truly async without a worker thread.
8. **Observability** — coroutine-aware tracing (span per `Task<T>`) for distributed tracing integration.

---

## 12. Companion Documents

- **`2026-04-06-async-connectors-00-overview.md`** — High-level architecture, problem statement, connector inventory.
- **`2026-04-06-async-connectors-01-event-loop-and-task.md`** — Event loop, Task<T>, IOAwaitable, FutureAwaitable design.
- **`2026-04-06-async-connectors-02-per-library-strategy.md`** — Detailed per-connector code sketches and reuse analysis.
