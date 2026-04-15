# Async Architecture

> [!info] Prerequisites
> Read [[06_Connector_Architecture]] first — the async layer builds directly on top of `connector_trait<DB>`.

## Overview

The ORM async subsystem provides C++20 coroutine primitives and infrastructure for non-blocking database access. It consists of:

- **`Task<T>`** — lazy coroutine task with `sync_wait()`, exception propagation, and `[[nodiscard]]` enforcement
- **`CancellationToken`** / **`CancellationSource`** — cooperative cancellation with callback registration
- **`IoContext`** — cross-platform event loop with reactor-style fd readiness (kqueue on macOS)
- **`ThreadPool`** + **`run_on_pool()`** — offload blocking callables to a fixed thread pool, resume coroutine with result
- **`async_db<DB>`** — async wrapper around `db<DB>` that returns `Task<result<...>>` from `operator<<`
- **`async_connection_pool<DB, N>`** — coroutine-aware connection pool
- **`async_transaction_guard<DB>`** — async RAII transaction guard

## Two Async Strategies

### 1. Thread-Pool Offload (Universal)

Any connector works with `async_db<DB>` — queries are offloaded to a `ThreadPool` via `run_on_pool()`:

```
caller coroutine
  │
  co_await (adb << query)
  │
  ├── run_on_pool(pool, [&]{ return db_ << query; })
  │     │
  │     └── pool thread executes sync query
  │           │
  │           └── result returned to coroutine frame
  │
  co_return result
```

### 2. Native Non-Blocking I/O (Optimized)

Connectors declaring `using supports_async = void;` in their `connector_trait<DB>` provide `async_execute()` that uses the library's native non-blocking API:

| Connector | Strategy | Key API |
|-----------|----------|---------|
| MySQL | `_start`/`_cont` pairs + fd watch | `mysql_real_query_start/cont` |
| PostgreSQL | libpq state machine + fd watch | `PQsendQueryParams` + `PQconsumeInput` |
| Cassandra | `CassFuture` callback bridge | `cass_future_set_callback` |
| Redis | hiredis async + event adapter | `redisAsyncCommandArgv` |
| MongoDB | Thread-pool offload only | libmongoc is fundamentally sync |
| Neo4j | Thread-pool offload only | libneo4j-client is sync |

## Key Headers

| Header | Contents |
|--------|----------|
| `ORM/async/task.hpp` | `Task<T>`, `TaskPromise<T>`, `sync_wait()`, `start_detached()` |
| `ORM/async/cancellation.hpp` | `CancellationToken`, `CancellationSource`, `CancellationGuard` |
| `ORM/async/io_context.hpp` | `IoContext` (abstract), `PollInterest`, `PollOperation` |
| `ORM/async/thread_pool.hpp` | `ThreadPool`, `run_on_pool()` |
| `ORM/connector/async_db.hpp` | `async_db<DB>` |
| `ORM/connector/capabilities.hpp` | `cap::supports_async` tag |
| `ORM/db/connectors/ThreadSafety/async_thread_safety.hpp` | `async_connection_pool`, `async_connection_guard`, `async_transaction_guard`, `async_begin_transaction()` |

## Capability Tag

The `supports_async` capability tag follows the same pattern as other capabilities:

```cpp
namespace cap { struct supports_async {}; }

template <>
struct connector_trait<AsyncPostgreSQLDB>
{
    using supports_async = void;  // opt-in

    // Native async path — called by async_db<DB> when supports_async is present
    template <typename Response, ...>
    static auto async_execute(AsyncPostgreSQLDB& db, select_query<...> q)
        -> Task<result<...>>;

    // Sync fallback — called by db<DB>
    template <typename Response, ...>
    static auto execute(AsyncPostgreSQLDB& db, select_query<...> q)
        -> result<...>;
};
```

When `has_capability<DB, cap::supports_async>` is true, `async_db<DB>::operator<<` calls `connector_trait<DB>::async_execute()` directly. Otherwise it wraps the sync `execute()` in `run_on_pool()`.

## Usage Example

```cpp
orm::ThreadPool pool(4);
orm::MockDB conn;
orm::async_db<orm::MockDB> adb(conn, pool);

auto task = [&]() -> orm::Task<void> {
    constexpr auto q = orm::select(orm::field<&User::id>);
    auto result = co_await (adb << q);
    co_return;
}();
task.sync_wait();
```

## Async Connection Pool

```cpp
orm::ThreadPool pool(4);
orm::async_connection_pool<orm::MockDB, 4> apool(pool);

auto task = [&]() -> orm::Task<void> {
    auto guard = co_await apool.acquire();
    auto adb = guard.get();  // async_db<MockDB>
    auto result = co_await (adb << orm::select(orm::field<&User::id>));
    co_return;
    // guard destructs → connection returned to pool
}();
```

## Async Transactions

```cpp
auto task = [&]() -> orm::Task<void> {
    auto txn = co_await orm::async_begin_transaction(conn, pool);
    // ... do work ...
    co_await txn.commit();
    // or let txn destruct for automatic ROLLBACK
    co_return;
}();
```

## See Also

- [[06_Connector_Architecture]] — sync connector pattern
- [[05_CRUD_Builder_Architecture]] — query IR construction
