# Async Connectors — Architectural Overview

**Date:** 2026-04-06
**Status:** Design notes — pre-implementation
**Author:** Design session with Alex Tsvetanov

---

## 1. Problem Statement

Every connector in `lib/include/ORM/db/connectors/` currently performs **blocking I/O** on the calling thread. The `connector_trait<DB>::execute(...)` functions call C library functions that send a query over a socket and then block until the response arrives. This means:

- A thread running `db << select(...)` is **parked** for the entire round-trip time (network latency + server query execution time).
- Under concurrent load (e.g., a web server handling many requests), this requires **one OS thread per in-flight query**, which does not scale.
- The existing `connection_pool<DB, N>` in `ThreadSafety/thread_safety.hpp` mitigates this by allowing N parallel blocking calls, but each still occupies a full thread.

The goal is to convert the I/O path from blocking to **non-blocking**, using C++20 coroutines (`co_await` / `co_return`) and a platform-specific event loop (io_uring on Linux, IOCP on Windows, kqueue on macOS).

---

## 2. What Changes and What Does Not

### 2.1 What stays identical (zero changes)

| Layer | Why it doesn't change |
|---|---|
| **Entity model** (`property<>`, `relationship<>`, `table.hpp`) | Pure type-level declarations; no I/O. |
| **Query IR** (`select_query`, `insert_query`, `update_query`, `delete_query`) | Compile-time fluent builder; pure computation. |
| **SQL/CQL/Cypher rendering** (`mysql_live_detail::`, `pg_live_detail::`, `cass_live_detail::`, etc.) | String-building helpers; pure functions, no I/O. |
| **Parameter binding logic** (`param_binder`, `bind_value`, `bind_params`) | Fills driver structs; no network calls. |
| **Row hydration** (`hydrate_row`, `convert_field`, `read_cass_value`, etc.) | Reads from in-memory result buffers; no I/O. |
| **Capability system** (`capabilities.hpp`, `trait.hpp`) | Type-level tags; no runtime behavior. |
| **Result type** (`result.hpp`) | In-memory container of hydrated rows. |

### 2.2 What changes

| Layer | Nature of change |
|---|---|
| **`connector_trait<DB>::execute()` return type** | `result<Row, Response>` → `Task<result<Row, Response>>` for async connectors. |
| **Private `exec_select`, `exec_prepared`, `exec_no_result`** | These are the **only** functions that perform socket I/O. They become coroutines. |
| **`db.hpp` `operator<<` and `execute()`** | Must `co_await` the trait's execute if `DB` is async. A concept `is_async_connector<DB>` gates this. |
| **Connection types** | Each async connector gets a new tag type (e.g., `AsyncMySQLDB`) that wraps the same C handle but is configured for non-blocking mode. |
| **New infrastructure** | `EventLoop`, `Task<T>`, `IOAwaitable`, `FutureAwaitable`, thread pool for connectors without native async APIs. |

### 2.3 The key architectural invariant

> **The I/O boundary is the only thing that changes.**
> Everything above it (query building, SQL rendering, parameter binding) and everything below it (row hydration, result construction) stays identical.
> The rendering namespaces (`mysql_live_detail::`, `pg_live_detail::`, etc.) are **100% reusable** in both sync and async connectors.

---

## 3. Current Connector Inventory

Below is a complete audit of every connector, the C library it uses, the blocking I/O call sites, and whether the library provides a native non-blocking API.

### 3.1 MySQL — `MySQLLiveDB` (`mysql_live.hpp`)

- **C library:** `libmysqlclient` via `<mysql/mysql.h>`
- **Connection handle:** `MYSQL* conn_`
- **Blocking call sites:**
  - `mysql_real_connect()` — in `MySQLLiveDB::connect()`
  - `mysql_query()` — in `exec_select()`
  - `mysql_store_result()` — in `exec_select()`
  - `mysql_fetch_row()` — in `exec_select()`
  - `mysql_stmt_prepare()` — in `exec_select_prepared()`
  - `mysql_stmt_execute()` — in `exec_select_prepared()` and `exec_prepared()`
  - `mysql_stmt_store_result()` — in `exec_select_prepared()`
  - `mysql_stmt_fetch()` — in `exec_select_prepared()`
- **Native async API:** **YES** — complete `_start`/`_cont` pairs for every operation listed above. Enable with `mysql_options(conn, MYSQL_OPT_NONBLOCK, nullptr)`. Each `_start` returns a wait-status bitmask; when the socket is ready, call `_cont` with the status.
- **Socket accessor:** `mysql_get_socket(conn)` returns the raw fd.
- **Async readiness:** Excellent — direct coroutine integration without any thread pool.

### 3.2 PostgreSQL — `PostgreSQLLiveDB` (`postgresql_live.hpp`)

- **C library:** `libpq` via `<libpq-fe.h>`
- **Connection handle:** `PGconn* conn_`
- **Blocking call sites:**
  - `PQconnectdb()` — in `PostgreSQLLiveDB::connect()`
  - `PQexecParams()` — in `exec_select()` and `exec_no_result()`
  - `PQexec()` — in `begin()`, `commit()`, `rollback()`
- **Native async API:** **YES** — the best async story of any C database library.
  - `PQconnectStart()` + `PQconnectPoll()` for async connect.
  - `PQsendQuery()` / `PQsendQueryParams()` for async query dispatch.
  - `PQsocket()` returns the fd to watch.
  - `PQflush()` to drain the send buffer.
  - `PQconsumeInput()` + `PQisBusy()` to drive the state machine on read-ready.
  - `PQgetResult()` to collect completed results.
  - `PQsetnonblocking(conn, 1)` to enable non-blocking mode.
- **Socket accessor:** `PQsocket(conn)` returns the raw fd.
- **Async readiness:** Excellent — the cleanest coroutine integration path.

### 3.3 MongoDB — `MongoDBLive` (`mongodb_live.hpp`)

- **C library:** `libmongoc` via `<mongoc/mongoc.h>`
- **Connection handle:** `mongoc_client_t* client_`
- **Blocking call sites:**
  - `mongoc_client_command_simple()` — in `MongoDBLive::connect()` (ping)
  - `mongoc_collection_find_with_opts()` + `mongoc_cursor_next()` — in `exec_select()`
  - `mongoc_collection_insert_one()` — in `exec_insert()`
  - `mongoc_collection_delete_many()` — in `exec_delete()`
- **Native async API:** **NO** — `libmongoc` is fundamentally synchronous. It manages its own internal connection pool and event loop, but does not expose socket fds or non-blocking entry points to the caller.
- **Async strategy:** Thread pool offload. Wrap each synchronous call in `run_on_pool([&] { return sync_call(); })` which posts the blocking work to a dedicated thread pool and suspends the coroutine until completion.
- **Alternative:** Switch to `mongocxx` (C++ driver) which has experimental Asio-based async support. This is a larger migration.
- **Async readiness:** Medium — requires thread pool infrastructure but reuses existing synchronous code unchanged.

### 3.4 Cassandra — `CassandraLiveDB` (`cassandra_live.hpp`)

- **C library:** DataStax C driver via `<cassandra.h>`
- **Connection handle:** `CassSession* session_` + `CassCluster* cluster_`
- **Blocking call sites:**
  - `cass_session_connect_keyspace()` + `cass_future_wait()` — in `CassandraLiveDB::connect()`
  - `cass_session_execute()` + `cass_future_wait()` — in `exec_no_result()`, `exec_select()`, `exec_select_params()`
  - `cass_session_close()` + `cass_future_wait()` — in `close()`
- **Native async API:** **YES** — `CassFuture*` is already an asynchronous future. The driver performs all I/O internally on its own thread pool. The blocking pattern `cass_future_wait()` can be replaced with `cass_future_set_callback()` which invokes a user callback when the result is ready.
- **Coroutine bridge:** Set callback that stores the coroutine handle, then resumes it when the future completes. No socket-level integration needed — the driver handles all I/O internally.
- **Async readiness:** Excellent — the driver is inherently async; we just need to stop calling `cass_future_wait()`.

### 3.5 Redis — `RedisLiveDB` (`redis_live.hpp`)

- **C library:** `hiredis` via `<hiredis/hiredis.h>`
- **Connection handle:** `redisContext* ctx_`
- **Blocking call sites:**
  - `redisConnect()` — in `RedisLiveDB::connect()`
  - `redisCommand()` — in `redis_live_detail::command()`
  - `redisCommandArgv()` — in INSERT handler
- **Native async API:** **YES** — `hiredis/async.h` provides:
  - `redisAsyncConnect()` for async connect.
  - `redisAsyncCommand()` for async command dispatch with a callback.
  - `redisAsyncSetConnectCallback()`, `redisAsyncSetDisconnectCallback()`.
  - Custom event loop adapter via `redisAsyncHandleRead()` / `redisAsyncHandleWrite()` and `redisAsyncContext::ev` hooks.
- **Socket accessor:** `redisAsyncContext::c.fd` gives the raw fd.
- **Coroutine bridge:** Register event loop fd watches via the hiredis adapter hooks. On command completion, the callback resumes the suspended coroutine.
- **Async readiness:** Good — well-documented callback-based async API; needs a coroutine bridge adapter.

### 3.6 Neo4j — `Neo4jLiveDB` (`neo4j_live.hpp`)

- **C library:** `libneo4j-client` via `<neo4j-client.h>`
- **Connection handle:** `neo4j_connection_t* conn_`
- **Blocking call sites:**
  - `neo4j_connect()` — in `Neo4jLiveDB::connect()`
  - `neo4j_run()` — in `exec_no_result()` and `exec_select()`
  - `neo4j_fetch_next()` — in `exec_select()`
- **Native async API:** **NO** — `libneo4j-client` provides no non-blocking API, no socket accessor, and no callback mechanism. It owns the socket internally.
- **Async strategy:** Thread pool offload only. Alternatively, implement the Bolt protocol directly over a raw TCP socket controlled by the event loop (significant effort).
- **Async readiness:** Low — thread pool offload is the only practical option.

### 3.7 SQLite — `SQLiteDB` (`sqlite_db.hpp`)

- **C library:** `sqlite3` via `<sqlite3.h>`
- **Connection handle:** `sqlite3* handle`
- **Blocking call sites:**
  - `sqlite3_prepare_v2()` — in every execute overload
  - `sqlite3_step()` — in every execute overload
- **Native async API:** **N/A** — SQLite is an in-process embedded database, not a network database. There is no socket; all I/O is file I/O to the local filesystem.
- **Async strategy:** Offload to a dedicated SQLite worker thread. SQLite in WAL mode allows concurrent reads from multiple threads, but only one writer at a time. A single dedicated thread with a work queue is the canonical pattern.
- **Async readiness:** Special case — no network I/O, but file I/O can still block. Thread offload is appropriate.

---

## 4. Async Readiness Summary

| Connector | Library | Native Async | Strategy | Effort |
|---|---|---|---|---|
| **MySQL** | libmysqlclient | `_start`/`_cont` pairs | Direct coroutine integration via fd watch | Medium |
| **PostgreSQL** | libpq | `PQsendQuery` + fd polling | Direct coroutine integration via fd watch | Medium |
| **Cassandra** | DataStax C driver | `CassFuture*` + callbacks | Callback → coroutine bridge | Low |
| **Redis** | hiredis | `hiredis/async.h` callbacks | Callback → coroutine bridge via event adapter | Medium |
| **MongoDB** | libmongoc | None | Thread pool offload | Low (reuses sync code) |
| **Neo4j** | libneo4j-client | None | Thread pool offload | Low (reuses sync code) |
| **SQLite** | sqlite3 | N/A (embedded) | Dedicated worker thread | Low (reuses sync code) |

---

## 5. Design Principles

1. **Keep sync connectors working.** Async is additive — `MySQLLiveDB` stays as-is; `AsyncMySQLDB` is a new type.
2. **Share rendering logic.** The `*_live_detail::` namespaces contain pure string-building functions. Both sync and async connectors call them.
3. **Single event loop abstraction.** `EventLoop` wraps io_uring (Linux) / IOCP (Windows) / kqueue (macOS). All fd-based async connectors (MySQL, PostgreSQL, Redis) register with it.
4. **Single `Task<T>` type.** A minimal coroutine task with continuation support. All async `execute()` methods return `Task<result<...>>`.
5. **`is_async_connector<DB>` concept.** The ORM core detects at compile time whether `execute()` returns `Task<...>` and gates `co_await` accordingly.
6. **Thread pool for connectors without native async.** MongoDB, Neo4j, and SQLite wrap their sync calls in `run_on_pool()`, which suspends the coroutine and resumes it when the sync work completes on a pool thread.
7. **No behavioral change to sync connectors.** Existing user code compiles and runs identically. Users opt into async by using `AsyncMySQLDB` instead of `MySQLLiveDB`.

---

## 6. Companion Documents

- **`2026-04-06-async-connectors-01-event-loop-and-task.md`** — Event loop abstraction, `Task<T>`, `IOAwaitable`, `FutureAwaitable`, thread pool design.
- **`2026-04-06-async-connectors-02-per-library-strategy.md`** — Detailed per-connector implementation plans with code sketches.
- **`2026-04-06-async-connectors-03-migration-plan.md`** — File layout, phasing, test strategy, rollout plan.
