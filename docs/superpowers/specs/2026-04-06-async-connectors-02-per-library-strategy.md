# Async Connectors — Per-Library Implementation Strategy

**Date:** 2026-04-06
**Status:** Design notes — pre-implementation
**Author:** Design session with Alex Tsvetanov

---

## 1. MySQL — Direct Coroutine Integration via `_start`/`_cont`

### 1.1 Library: libmysqlclient (MariaDB Connector/C)

The MariaDB fork of libmysqlclient (which is the de-facto standard on most Linux distributions) provides a **complete non-blocking API**. Every blocking function `mysql_X()` has a corresponding pair:

- `mysql_X_start(&result, ...) → int status` — initiates the operation, returns a bitmask of what to wait for.
- `mysql_X_cont(&result, ..., status) → int status` — continues the operation after the socket is ready.

When `status == 0`, the operation is complete. When `status != 0`, it contains `MYSQL_WAIT_READ`, `MYSQL_WAIT_WRITE`, and/or `MYSQL_WAIT_TIMEOUT` flags indicating what the event loop should wait for.

### 1.2 Available `_start`/`_cont` Pairs

| Blocking Call | Start | Cont | Used In |
|---|---|---|---|
| `mysql_real_connect()` | `mysql_real_connect_start()` | `mysql_real_connect_cont()` | `connect()` |
| `mysql_query()` | `mysql_real_query_start()` | `mysql_real_query_cont()` | `exec_select()` |
| `mysql_store_result()` | `mysql_store_result_start()` | `mysql_store_result_cont()` | `exec_select()` |
| `mysql_fetch_row()` | `mysql_fetch_row_start()` | `mysql_fetch_row_cont()` | `exec_select()` |
| `mysql_stmt_prepare()` | `mysql_stmt_prepare_start()` | `mysql_stmt_prepare_cont()` | `exec_select_prepared()` |
| `mysql_stmt_execute()` | `mysql_stmt_execute_start()` | `mysql_stmt_execute_cont()` | `exec_select_prepared()`, `exec_prepared()` |
| `mysql_stmt_store_result()` | `mysql_stmt_store_result_start()` | `mysql_stmt_store_result_cont()` | `exec_select_prepared()` |
| `mysql_stmt_fetch()` | `mysql_stmt_fetch_start()` | `mysql_stmt_fetch_cont()` | `exec_select_prepared()` |
| `mysql_close()` | `mysql_close_start()` | `mysql_close_cont()` | `~AsyncMySQLDB()` (sync fallback) |

### 1.3 Enabling Non-Blocking Mode

```cpp
MYSQL* conn = mysql_init(nullptr);
mysql_options(conn, MYSQL_OPT_NONBLOCK, nullptr);  // <-- this one line
```

After this, all `_start`/`_cont` pairs become available. The socket returned by `mysql_get_socket(conn)` can be registered with our event loop.

### 1.4 Universal Async Pattern

Every non-blocking MySQL operation follows an identical pattern that can be abstracted into a helper:

```cpp
// Helper: run any mysql _start/_cont pair as a coroutine
// StartFn signature: int start_fn(RetT* result, MYSQL*/MYSQL_STMT*, ...)
// ContFn signature:  int cont_fn(RetT* result, MYSQL*/MYSQL_STMT*, int status)
template <typename RetT, typename StartFn, typename ContFn>
Task<RetT> mysql_async(int fd, StartFn start_fn, ContFn cont_fn)
{
    RetT ret{};
    int status = start_fn(&ret);
    while (status)
    {
        IOInterest interest = (status & MYSQL_WAIT_WRITE)
            ? IOInterest::Write
            : IOInterest::Read;
        co_await IOAwaitable{fd, interest, &EventLoop::current()};
        status = cont_fn(&ret, status);
    }
    co_return ret;
}
```

### 1.5 Async Connect

```cpp
struct AsyncMySQLDB
{
    MYSQL* conn_{nullptr};

    [[nodiscard]] static Task<AsyncMySQLDB> connect(
        const char* host, unsigned int port,
        const char* user, const char* password, const char* database)
    {
        AsyncMySQLDB db;
        db.conn_ = mysql_init(nullptr);
        if (!db.conn_)
            throw std::runtime_error("MySQL init failed");

        mysql_options(db.conn_, MYSQL_OPT_NONBLOCK, nullptr);

        MYSQL* ret = nullptr;
        int status = mysql_real_connect_start(
            &ret, db.conn_, host, user, password, database, port, nullptr, 0);

        while (status)
        {
            co_await IOAwaitable{
                mysql_get_socket(db.conn_),
                (status & MYSQL_WAIT_WRITE) ? IOInterest::Write : IOInterest::Read,
                &EventLoop::current()
            };
            status = mysql_real_connect_cont(&ret, db.conn_, status);
        }

        if (!ret)
        {
            std::string err = mysql_error(db.conn_);
            mysql_close(db.conn_);
            db.conn_ = nullptr;
            throw std::runtime_error("MySQL connect failed: " + err);
        }

        co_return std::move(db);
    }

    [[nodiscard]] int fd() const noexcept { return mysql_get_socket(conn_); }
};
```

### 1.6 Async SELECT (Prepared Statement Path)

This is the most complex operation because it has five sequential async steps:

```cpp
template <typename Row, typename Response, typename... Params>
static Task<result<Row, Response>> exec_select_prepared_async(
    AsyncMySQLDB& db, const std::string& sql, Params... params)  // by value!
{
    // ── Step 1: Prepare ──────────────────────────────────────────────
    MYSQL_STMT* stmt = mysql_stmt_init(db.conn_);
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    int err = 0;
    int status = mysql_stmt_prepare_start(&err, stmt, sql.c_str(), sql.size());
    while (status)
    {
        co_await IOAwaitable{db.fd(),
            (status & MYSQL_WAIT_WRITE) ? IOInterest::Write : IOInterest::Read,
            &EventLoop::current()};
        status = mysql_stmt_prepare_cont(&err, stmt, status);
    }
    if (err)
    {
        std::string e = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        throw std::runtime_error("MySQL prepare failed: " + e);
    }

    // ── Step 2: Bind params (synchronous — no I/O) ──────────────────
    mysql_live_detail::param_binder binder(params...);
    binder.bind(stmt);

    // ── Step 3: Execute ──────────────────────────────────────────────
    err = 0;
    status = mysql_stmt_execute_start(&err, stmt);
    while (status)
    {
        co_await IOAwaitable{db.fd(),
            (status & MYSQL_WAIT_WRITE) ? IOInterest::Write : IOInterest::Read,
            &EventLoop::current()};
        status = mysql_stmt_execute_cont(&err, stmt, status);
    }
    if (err)
    {
        std::string e = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        throw std::runtime_error("MySQL execute failed: " + e);
    }

    // ── Step 4: Store result ─────────────────────────────────────────
    err = 0;
    status = mysql_stmt_store_result_start(&err, stmt);
    while (status)
    {
        co_await IOAwaitable{db.fd(),
            (status & MYSQL_WAIT_WRITE) ? IOInterest::Write : IOInterest::Read,
            &EventLoop::current()};
        status = mysql_stmt_store_result_cont(&err, stmt, status);
    }
    if (err)
    {
        std::string e = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        throw std::runtime_error("MySQL store_result failed: " + e);
    }

    // ── Step 5: Bind result buffers + fetch rows ─────────────────────
    // (Result buffer setup is identical to the sync version)
    MYSQL_RES* meta = mysql_stmt_result_metadata(stmt);
    unsigned int num_fields = mysql_num_fields(meta);

    std::vector<MYSQL_BIND> result_binds(num_fields);
    std::vector<std::vector<char>> buffers(num_fields);
    std::vector<unsigned long> lengths(num_fields);
    std::vector<char> is_nulls(num_fields);

    for (unsigned int i = 0; i < num_fields; ++i)
    {
        buffers[i].resize(1024);
        std::memset(&result_binds[i], 0, sizeof(MYSQL_BIND));
        result_binds[i].buffer_type = MYSQL_TYPE_STRING;
        result_binds[i].buffer = buffers[i].data();
        result_binds[i].buffer_length = buffers[i].size();
        result_binds[i].length = &lengths[i];
        result_binds[i].is_null = reinterpret_cast<bool*>(&is_nulls[i]);
    }
    mysql_stmt_bind_result(stmt, result_binds.data());

    // Fetch rows — each fetch is async
    std::vector<Row> rows;
    int fetch_status = 0;
    while (true)
    {
        status = mysql_stmt_fetch_start(&fetch_status, stmt);
        while (status)
        {
            co_await IOAwaitable{db.fd(),
                (status & MYSQL_WAIT_WRITE) ? IOInterest::Write : IOInterest::Read,
                &EventLoop::current()};
            status = mysql_stmt_fetch_cont(&fetch_status, stmt, status);
        }
        if (fetch_status == MYSQL_NO_DATA) break;
        if (fetch_status != 0)
        {
            std::string e = mysql_stmt_error(stmt);
            mysql_free_result(meta);
            mysql_stmt_close(stmt);
            throw std::runtime_error("MySQL fetch failed: " + e);
        }

        // Hydration — identical to sync version
        // (reads from in-memory buffers, no I/O)
        MYSQL_ROW row_data = new char*[num_fields];
        for (unsigned int i = 0; i < num_fields; ++i)
            row_data[i] = is_nulls[i] ? nullptr : buffers[i].data();
        rows.push_back(hydrate_row<Row>(row_data, lengths.data()));
        delete[] row_data;
    }

    mysql_free_result(meta);
    mysql_stmt_close(stmt);
    co_return result<Row, Response>{std::move(rows)};
}
```

### 1.7 What's Reused from `mysql_live.hpp`

| Component | Reused? | Notes |
|---|---|---|
| `mysql_live_detail::render_operand()` | Yes | Pure string function |
| `mysql_live_detail::render_rule()` | Yes | Pure string function |
| `mysql_live_detail::render_columns()` | Yes | Pure string function |
| `mysql_live_detail::render_wheres()` | Yes | Pure string function |
| `mysql_live_detail::render_order_by()` | Yes | Pure string function |
| `mysql_live_detail::render_limits()` | Yes | Pure string function |
| `mysql_live_detail::render_set()` | Yes | Pure string function |
| `mysql_live_detail::positional_placeholders()` | Yes | Pure string function |
| `mysql_live_detail::param_binder` | Yes | Fills structs, no I/O |
| `mysql_live_detail::mysql_type_traits<>` | Yes | Type mapping |
| `mysql_live_detail::convert_field<>()` | Yes | Type conversion |
| `mysql_live_detail::to_sql_literal()` | Yes | String formatting |
| `mysql_live_detail::escape_string()` | Yes | Buffer operation |
| `MySQLLiveDB` (sync tag type) | No | New `AsyncMySQLDB` tag type |
| `exec_select()` | No | Replaced by async coroutine |
| `exec_select_prepared()` | No | Replaced by async coroutine |
| `exec_prepared()` | No | Replaced by async coroutine |
| `hydrate_row()` / `hydrate_impl()` | Yes | In-memory buffer read |

**The entire `mysql_live_detail::` namespace is reusable without modification.**

### 1.8 Edge Cases and Pitfalls

- **`MYSQL_WAIT_TIMEOUT`:** Some `_start` functions can also return `MYSQL_WAIT_TIMEOUT` if a connection timeout is set. The event loop must support timeouts (via `IORING_OP_TIMEOUT` or similar). For MVP, we can ignore this and let the OS TCP timeout handle it.
- **Thread affinity:** A `MYSQL*` handle is not thread-safe. The async connector must ensure all operations on a single handle happen on the same event loop thread.
- **`mysql_close()`:** The destructor cannot be a coroutine. We call `mysql_close()` synchronously in the destructor. For graceful shutdown, provide `async_close() -> Task<void>`.

---

## 2. PostgreSQL — The Best Async Story

### 2.1 Library: libpq

libpq has the most mature and well-documented non-blocking API of any C database library. The key functions:

| Function | Purpose |
|---|---|
| `PQconnectStart(conninfo)` | Begin async connection; returns `PGconn*` |
| `PQconnectPoll(conn)` | Drive the connection state machine |
| `PQsetnonblocking(conn, 1)` | Enable non-blocking mode for queries |
| `PQsocket(conn)` | Get the raw fd for event loop registration |
| `PQsendQuery(conn, sql)` | Enqueue a simple query (non-blocking) |
| `PQsendQueryParams(conn, sql, nParams, ...)` | Enqueue a parameterized query |
| `PQflush(conn)` | Flush the send buffer; returns 0 if done, 1 if more to write |
| `PQconsumeInput(conn)` | Read available data from the socket into libpq's internal buffer |
| `PQisBusy(conn)` | Returns 1 if the result is not yet complete |
| `PQgetResult(conn)` | Get the completed result (non-blocking once `PQisBusy()` returns 0) |

### 2.2 The Async State Machine

libpq's async model is a state machine driven by the caller:

```
                ┌──────────────────┐
                │ PQsendQueryParams│  ← enqueue the query (non-blocking)
                └────────┬─────────┘
                         │
                ┌────────▼─────────┐
           ┌──▶ │    PQflush()     │  ← drain send buffer
           │    └────────┬─────────┘
           │             │
           │     returns 1? ───▶ wait_writable(fd) ──┐
           │             │                            │
           │     returns 0                            │
           │             │                            │
           │    ┌────────▼─────────┐                  │
           │    │   PQisBusy()     │ ◀────────────────┘
           │    └────────┬─────────┘
           │             │
           │     returns 1? ───▶ wait_readable(fd)
           │             │       PQconsumeInput()
           │             │       └──▶ loop back to PQisBusy()
           │             │
           │     returns 0
           │             │
           │    ┌────────▼─────────┐
           │    │  PQgetResult()   │  ← non-blocking, result is ready
           │    └──────────────────┘
           │
           └── (for pipelined queries, repeat)
```

### 2.3 Async Connect

```cpp
struct AsyncPgDB
{
    PGconn* conn_{nullptr};

    [[nodiscard]] static Task<AsyncPgDB> connect(const char* conninfo)
    {
        AsyncPgDB db;
        db.conn_ = PQconnectStart(conninfo);
        if (!db.conn_)
            throw std::runtime_error("PQconnectStart failed");

        if (PQstatus(db.conn_) == CONNECTION_BAD)
        {
            std::string err = PQerrorMessage(db.conn_);
            PQfinish(db.conn_);
            db.conn_ = nullptr;
            throw std::runtime_error("PostgreSQL connection failed: " + err);
        }

        // Drive the connection state machine
        PostgresPollingStatusType poll_status;
        while (true)
        {
            poll_status = PQconnectPoll(db.conn_);

            if (poll_status == PGRES_POLLING_OK)
                break;
            if (poll_status == PGRES_POLLING_FAILED)
            {
                std::string err = PQerrorMessage(db.conn_);
                PQfinish(db.conn_);
                db.conn_ = nullptr;
                throw std::runtime_error("PostgreSQL connect failed: " + err);
            }

            if (poll_status == PGRES_POLLING_WRITING)
                co_await wait_writable(PQsocket(db.conn_));
            else if (poll_status == PGRES_POLLING_READING)
                co_await wait_readable(PQsocket(db.conn_));
        }

        // Enable non-blocking mode for subsequent queries
        PQsetnonblocking(db.conn_, 1);

        co_return std::move(db);
    }

    [[nodiscard]] int fd() const noexcept { return PQsocket(conn_); }
};
```

### 2.4 Async Query Execution

```cpp
// Core async query helper — used by all CRUD operations
static Task<PGresult*> exec_async(AsyncPgDB& db, const std::string& sql,
                                   const std::vector<std::string>& vals)
{
    // Prepare parameter arrays
    std::vector<const char*> param_values;
    param_values.reserve(vals.size());
    for (const auto& v : vals)
        param_values.push_back(v.c_str());

    // Send query (non-blocking — just enqueues)
    int ok = PQsendQueryParams(
        db.conn_,
        sql.c_str(),
        static_cast<int>(vals.size()),
        nullptr,                      // let server infer types
        param_values.data(),
        nullptr,                      // text format lengths
        nullptr,                      // text format
        0                             // result in text format
    );
    if (!ok)
        throw std::runtime_error(std::string("PQsendQueryParams failed: ") +
            PQerrorMessage(db.conn_));

    // Flush the send buffer
    while (PQflush(db.conn_) == 1)
        co_await wait_writable(db.fd());

    // Wait for the result to be ready
    while (PQisBusy(db.conn_))
    {
        co_await wait_readable(db.fd());
        if (!PQconsumeInput(db.conn_))
            throw std::runtime_error(std::string("PQconsumeInput failed: ") +
                PQerrorMessage(db.conn_));
    }

    // Result is ready — PQgetResult is non-blocking now
    PGresult* res = PQgetResult(db.conn_);

    // Drain any remaining results (for simple query protocol)
    PGresult* extra = nullptr;
    while ((extra = PQgetResult(db.conn_)) != nullptr)
        PQclear(extra);

    co_return res;
}
```

### 2.5 Async SELECT

```cpp
template <typename Row, typename Response>
static Task<result<Row, Response>> exec_select_async(
    AsyncPgDB& db,
    const std::string& sql,
    const std::vector<std::string>& vals)
{
    PGresult* res = co_await exec_async(db, sql, vals);

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        std::string err = PQerrorMessage(db.conn_);
        PQclear(res);
        throw std::runtime_error("PostgreSQL SELECT failed: " + err);
    }

    // Hydration — identical to sync version (reads from PGresult in memory)
    const int nrows = PQntuples(res);
    std::vector<Row> rows;
    rows.reserve(nrows);

    for (int i = 0; i < nrows; ++i)
        rows.push_back(hydrate_row<Row>(res, i));

    PQclear(res);
    co_return result<Row, Response>{std::move(rows)};
}
```

### 2.6 Async Transactions

```cpp
static Task<void> begin_async(AsyncPgDB& db)
{
    PGresult* res = co_await exec_async(db, "BEGIN", {});
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        std::string err = PQerrorMessage(db.conn_);
        PQclear(res);
        throw std::runtime_error("BEGIN failed: " + err);
    }
    PQclear(res);
}

// commit_async and rollback_async follow identically
```

### 2.7 What's Reused from `postgresql_live.hpp`

**Everything in `pg_live_detail::` is reused without modification:**
- `RenderCtx`, `render_leaf()`, `render_rule()`, `render_columns()`, `render_wheres()`, `render_order_by()`, `render_limits()`, `dollar_placeholders()`, `render_set()`, `param_to_string()`, `collect_params()`, `convert_field()`
- `hydrate_row()`, `hydrate_impl()`

### 2.8 libpq Pipeline Mode (Advanced — Post-MVP)

libpq 14+ supports **pipeline mode** (`PQpipelineStatus`, `PQenterPipelineMode`), which allows sending multiple queries without waiting for each response. This maps naturally to coroutines:

```cpp
// Fire multiple queries concurrently, each as a separate coroutine
auto t1 = exec_select_async<Row1, Resp1>(db, sql1, vals1);
auto t2 = exec_select_async<Row2, Resp2>(db, sql2, vals2);
// ... both queries are in flight simultaneously over the same connection
auto [r1, r2] = co_await when_all(t1, t2);
```

This requires a `when_all()` combinator and careful handling of pipelined result ordering. Deferred to post-MVP.

---

## 3. Cassandra — Callback-to-Coroutine Bridge

### 3.1 Library: DataStax C/C++ Driver

The Cassandra driver is **already asynchronous internally**. It manages its own I/O thread pool, connection pool, and request routing. The `CassFuture*` returned by `cass_session_execute()` represents an in-flight request that will complete on one of the driver's internal threads.

The current sync code calls `cass_future_wait(future)` to block until completion. The async conversion simply replaces this with `cass_future_set_callback()`.

### 3.2 The Bridge

```cpp
// RAII wrapper for CassFuture* with coroutine suspension
struct CassFutureAwaitable
{
    CassFuture* future_;

    [[nodiscard]] bool await_ready() const noexcept
    {
        return cass_future_ready(future_) == cass_true;
    }

    void await_suspend(std::coroutine_handle<> h) noexcept
    {
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
            std::string err(msg, msg_len);
            throw std::runtime_error("Cassandra error: " + err);
        }
    }
};
```

### 3.3 Converted Operations

The conversion is mechanical — every `cass_future_wait(f)` becomes `co_await CassFutureAwaitable{f}`:

```cpp
// Before (sync):
CassFuture* future = cass_session_execute(db.session_, stmt);
cass_future_wait(future);
if (cass_future_error_code(future) != CASS_OK) { /* error */ }
const CassResult* res = cass_future_get_result(future);
cass_future_free(future);

// After (async):
CassFuture* future = cass_session_execute(db.session_, stmt);
co_await CassFutureAwaitable{future};
const CassResult* res = cass_future_get_result(future);
cass_future_free(future);
```

### 3.4 Threading Concern

**Important:** The callback set by `cass_future_set_callback()` is invoked on one of the Cassandra driver's internal I/O threads — **not** our event loop thread. This means `handle.resume()` runs the coroutine on the driver's thread.

Options:
1. **Accept it** — the coroutine continues on the driver's thread. Simple, but means our coroutine code must be thread-safe and must not assume it's on the event loop thread. For short hydration logic, this is fine.
2. **Post back to event loop** — the callback posts a "resume" event to our `EventLoop` via eventfd/IOCP, and the coroutine resumes on the event loop thread. Safer, but adds latency.

**Recommendation:** Option 1 for MVP. The post-callback work (result hydration) is short and CPU-bound, so running it on the driver thread is acceptable. Add a `resume_on(EventLoop&)` awaitable adapter for option 2 if needed.

### 3.5 Async Connect

```cpp
struct AsyncCassandraDB
{
    CassCluster* cluster_{nullptr};
    CassSession* session_{nullptr};

    [[nodiscard]] static Task<AsyncCassandraDB> connect(
        const char* contact_points,
        const char* keyspace,
        unsigned int port = 9042)
    {
        AsyncCassandraDB db;
        db.cluster_ = cass_cluster_new();
        db.session_ = cass_session_new();

        cass_cluster_set_contact_points(db.cluster_, contact_points);
        cass_cluster_set_port(db.cluster_, static_cast<int>(port));

        CassFuture* connect_future = cass_session_connect_keyspace(
            db.session_, db.cluster_, keyspace);

        co_await CassFutureAwaitable{connect_future};
        cass_future_free(connect_future);

        co_return std::move(db);
    }
};
```

### 3.6 What's Reused from `cassandra_live.hpp`

**Everything in `cass_live_detail::` is reused without modification:**
- `render_operand()`, `render_rule()`, `render_columns()`, `render_wheres()`, `positional_placeholders()`
- `bind_value()`, `bind_params()`
- `read_cass_value<>()`
- `hydrate_row()`

**Only the `exec_select`, `exec_select_params`, and `exec_no_result` functions change** (add `co_await` before future wait).

---

## 4. Redis — hiredis/async.h Callback Bridge

### 4.1 Library: hiredis

hiredis provides two APIs:
- **Synchronous** (`hiredis/hiredis.h`): `redisCommand()` blocks until the reply arrives.
- **Asynchronous** (`hiredis/async.h`): `redisAsyncCommand()` takes a callback, returns immediately.

The async API requires the caller to provide an event loop adapter that tells hiredis when to read/write on the socket.

### 4.2 Async Context Setup

```cpp
struct AsyncRedisDB
{
    redisAsyncContext* ctx_{nullptr};

    [[nodiscard]] static Task<AsyncRedisDB> connect(const char* host, int port = 6379)
    {
        AsyncRedisDB db;
        db.ctx_ = redisAsyncConnect(host, port);
        if (!db.ctx_ || db.ctx_->err)
        {
            std::string err = db.ctx_ ? db.ctx_->errstr : "redisAsyncConnect returned null";
            if (db.ctx_) redisAsyncFree(db.ctx_);
            db.ctx_ = nullptr;
            throw std::runtime_error("redisAsyncConnect failed: " + err);
        }

        // Attach our event loop adapter
        HiredisAdapter::attach(db.ctx_);

        // Wait for connection to complete
        // (hiredis calls the connect callback when ready)
        co_await HiredisConnectAwaitable{db.ctx_};

        co_return std::move(db);
    }
};
```

### 4.3 Command Awaitable

```cpp
struct RedisCommandAwaitable
{
    redisAsyncContext* ctx_;
    std::vector<std::string> args_;  // command arguments, stored by value

    std::coroutine_handle<> handle_{};
    redisReply* reply_{nullptr};
    std::exception_ptr exception_{};

    [[nodiscard]] bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h)
    {
        handle_ = h;

        // Build argv for redisAsyncCommandArgv
        std::vector<const char*> argv;
        std::vector<size_t> argvlen;
        for (const auto& a : args_)
        {
            argv.push_back(a.c_str());
            argvlen.push_back(a.size());
        }

        int status = redisAsyncCommandArgv(ctx_,
            [](redisAsyncContext* /*ac*/, void* reply, void* privdata) {
                auto* self = static_cast<RedisCommandAwaitable*>(privdata);
                self->reply_ = static_cast<redisReply*>(reply);
                if (!self->reply_ || self->reply_->type == REDIS_REPLY_ERROR)
                {
                    self->exception_ = std::make_exception_ptr(
                        std::runtime_error(
                            self->reply_ ? self->reply_->str : "null reply"));
                }
                self->handle_.resume();
            },
            this,
            static_cast<int>(argv.size()),
            argv.data(),
            argvlen.data());

        if (status != REDIS_OK)
        {
            exception_ = std::make_exception_ptr(
                std::runtime_error("redisAsyncCommandArgv failed"));
            h.resume();  // resume immediately with error
        }
    }

    redisReply* await_resume()
    {
        if (exception_)
            std::rethrow_exception(exception_);
        return reply_;
    }
};
```

### 4.4 Event Loop Adapter (Detailed)

hiredis needs the event loop to tell it when the socket is readable/writable. We do this through the `redisAsyncContext::ev` struct:

```cpp
struct OrmHiredisEvents
{
    // Per-context state
    struct EvState
    {
        redisAsyncContext* ctx;
        bool reading{false};
        bool writing{false};
        std::coroutine_handle<> read_handle{};
        std::coroutine_handle<> write_handle{};
    };

    static void add_read(void* privdata)
    {
        auto* state = static_cast<EvState*>(privdata);
        if (!state->reading)
        {
            state->reading = true;
            EventLoop::current().watch(state->ctx->c.fd, IOInterest::Read,
                /* a handle that calls redisAsyncHandleRead */);
        }
    }

    static void del_read(void* privdata)
    {
        auto* state = static_cast<EvState*>(privdata);
        state->reading = false;
        // unwatch read
    }

    static void add_write(void* privdata)
    {
        auto* state = static_cast<EvState*>(privdata);
        if (!state->writing)
        {
            state->writing = true;
            EventLoop::current().watch(state->ctx->c.fd, IOInterest::Write,
                /* a handle that calls redisAsyncHandleWrite */);
        }
    }

    static void del_write(void* privdata)
    {
        auto* state = static_cast<EvState*>(privdata);
        state->writing = false;
    }

    static void cleanup(void* privdata)
    {
        auto* state = static_cast<EvState*>(privdata);
        delete state;
    }

    static void attach(redisAsyncContext* ac)
    {
        auto* state = new EvState{ac};
        ac->ev.addRead  = add_read;
        ac->ev.delRead  = del_read;
        ac->ev.addWrite = add_write;
        ac->ev.delWrite = del_write;
        ac->ev.cleanup  = cleanup;
        ac->ev.data     = state;
    }
};
```

When the event loop detects the fd is ready for reading, it must call `redisAsyncHandleRead(ac)`. When ready for writing, `redisAsyncHandleWrite(ac)`. This drives hiredis' internal state machine, which eventually invokes the command callback, which resumes the coroutine.

### 4.5 What's Reused from `redis_live.hpp`

- `redis_live_detail::to_string<>()`, `redis_live_detail::from_string<>()`
- `redis_live_detail::key_prefix<>()`
- `make_row_from_strings<>()`

The `redis_live_detail::command()` function and `ReplyRAII` are **not reused** — they're synchronous. The async version uses `RedisCommandAwaitable` instead.

---

## 5. MongoDB — Thread Pool Offload

### 5.1 Library: libmongoc

libmongoc is fundamentally synchronous. There is no non-blocking API, no socket accessor, and no callback mechanism. The driver manages its own internal connection pool, authentication, topology monitoring, and server selection — all synchronously from the caller's perspective.

### 5.2 Strategy

Wrap every blocking `mongoc_*` call in `run_on_pool()`:

```cpp
template <>
struct connector_trait<AsyncMongoDBLive>
{
    using is_async = void;  // opt-in tag

    template <typename Response, typename Joins, typename Wheres,
              typename Limits, typename Groups, typename Orders,
              typename... Params>
    static auto execute(
        AsyncMongoDBLive& db,
        select_query<Response, Joins, Wheres, Limits, Groups, Orders> q,
        Params... params)  // by value!
        -> Task<result<projected_type<Response>, Response>>
    {
        using Row = projected_type<Response>;

        // Capture everything by value (coroutine lifetime!)
        auto p = mongo_live_detail::collect_params(params...);

        co_return co_await run_on_pool([&db, q, p]() {
            // This runs on a pool thread — blocking is fine.
            // We call the exact same logic as the sync connector.
            return sync_exec_select<Row, Response>(db, q, p);
        });
    }
};
```

### 5.3 `sync_exec_select` — Extracted Sync Logic

We extract the body of `connector_trait<MongoDBLive>::exec_select()` into a free function so both the sync and async connectors can call it:

```cpp
namespace mongo_live_detail {

    template <typename Row, typename Response, typename Query>
    auto sync_exec_select(
        MongoDBLiveBase& db,
        const Query& q,
        const std::vector<std::string>& params)
        -> result<Row, Response>
    {
        // ... exact same code as current exec_select() ...
        // Build filter, projection, run mongoc_collection_find_with_opts,
        // iterate cursor, hydrate rows, return result.
    }

} // namespace mongo_live_detail
```

### 5.4 Connection Type

```cpp
struct AsyncMongoDBLive
{
    mongoc_client_t* client_{nullptr};
    std::string      default_db_;

    // Same connect() as MongoDBLive, but returns Task<>
    [[nodiscard]] static Task<AsyncMongoDBLive> connect(
        const char* uri_string, const char* db_name)
    {
        // Connection can also be offloaded to the pool
        co_return co_await run_on_pool([=]() {
            // Reuse the same connection logic
            AsyncMongoDBLive db;
            // ... same as MongoDBLive::connect() ...
            return db;
        });
    }
};
```

### 5.5 Thread Safety Concern

`mongoc_client_t*` is **not thread-safe** for concurrent operations. However, since each `run_on_pool()` call captures the client by reference and the pool runs the lambda to completion before any other lambda can use the same client, serialization is implicit as long as we don't `co_await` multiple operations on the same client concurrently.

For concurrent MongoDB operations, use `mongoc_client_pool_t` (libmongoc's own connection pool), which is thread-safe. Each `run_on_pool()` call would pop a client from the pool, use it, and return it.

---

## 6. Neo4j — Thread Pool Offload

### 6.1 Library: libneo4j-client

Same situation as MongoDB — no non-blocking API, no socket accessor.

### 6.2 Strategy

Identical to MongoDB: `run_on_pool()` wrapping the synchronous code.

```cpp
struct AsyncNeo4jDB
{
    neo4j_connection_t* conn_{nullptr};

    [[nodiscard]] static Task<AsyncNeo4jDB> connect(
        const char* url,
        const char* username = "neo4j",
        const char* password = "neo4j")
    {
        co_return co_await run_on_pool([=]() {
            // Same logic as Neo4jLiveDB::connect()
            AsyncNeo4jDB db;
            // ...
            return db;
        });
    }
};

template <>
struct connector_trait<AsyncNeo4jDB>
{
    using is_async = void;

    template <typename Response, typename Joins, typename Wheres,
              typename Limits, typename Groups, typename Orders,
              typename... Params>
    static auto execute(
        AsyncNeo4jDB& db,
        select_query<Response, Joins, Wheres, Limits, Groups, Orders> q,
        Params... params)
        -> Task<result<projected_type<Response>, Response>>
    {
        co_return co_await run_on_pool([&db, q, params...]() {
            // Reuse sync logic from neo4j_live_detail::
            return sync_exec_select<projected_type<Response>, Response>(
                db, q, params...);
        });
    }
};
```

### 6.3 Alternative: Raw Bolt Protocol

For high-performance async Neo4j access, we could implement the Bolt protocol directly over a raw TCP socket controlled by our event loop. The Bolt protocol is a binary protocol with:
- Handshake negotiation
- Authentication (INIT message)
- RUN + PULL_ALL message pairs for queries
- RECORD messages for result rows

This is significant effort (~2000 lines) but would give us true async without any thread pool overhead. **Deferred to post-MVP.**

### 6.4 What's Reused from `neo4j_live.hpp`

**Everything in `neo4j_live_detail::` is reused:**
- `CypherCtx`, `render_return()`, `render_leaf()`, `render_where_rule()`, `render_where()`, `stringify()`, `build_map_entries()`, `read_neo4j_value<>()`
- `hydrate_row()`

---

## 7. SQLite — Dedicated Worker Thread

### 7.1 Library: sqlite3

SQLite is not a network database — it's an in-process embedded database that reads/writes files on the local filesystem. There is no socket and no network I/O. However, file I/O can still block (especially on NFS or slow storage).

### 7.2 Strategy: Single Writer Thread

SQLite's threading model:
- **WAL mode** allows concurrent readers from multiple threads.
- But only **one writer at a time** (the second writer gets `SQLITE_BUSY`).
- `sqlite3_open()` with `SQLITE_OPEN_FULLMUTEX` provides serialized access.

The canonical async pattern:
- A single dedicated thread owns the `sqlite3*` handle.
- All operations are posted to this thread's work queue.
- The coroutine suspends until the work completes.

```cpp
struct AsyncSQLiteDB
{
    sqlite3* handle{nullptr};
    // Dedicated worker thread with a work queue
    SQLiteWorker worker_;

    [[nodiscard]] static Task<AsyncSQLiteDB> open(const char* path)
    {
        AsyncSQLiteDB db;
        co_return co_await run_on_pool([&db, path]() {
            if (sqlite3_open(path, &db.handle) != SQLITE_OK)
                throw std::runtime_error(sqlite3_errmsg(db.handle));
            // Enable WAL mode for concurrent reads
            sqlite3_exec(db.handle, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
            return db;
        });
    }
};
```

### 7.3 Read Operations

Since WAL mode allows concurrent reads, read operations can be posted to the general thread pool:

```cpp
// Reads can use the shared thread pool
auto execute(...) -> Task<result<Row, Response>>
{
    co_return co_await run_on_pool([&] {
        return sync_exec_select(db, sql, params...);
    });
}
```

### 7.4 Write Operations

Write operations must be serialized on the dedicated writer thread:

```cpp
// Writes use the dedicated SQLite worker thread
auto execute(...) -> Task<result<std::tuple<>>>
{
    co_return co_await db.worker_.post([&] {
        return sync_exec_insert(db, sql, params...);
    });
}
```

### 7.5 What's Reused from `sqlite_db.hpp`

**Everything in `sqlite_detail::` is reused:**
- `bind_value()`, `read_column<>()`, `hydrate_row()`
- `columns_from_tuple()`, `placeholders()`, `render_columns()`, `render_wheres_sqlite()`, `render_operand_sqlite()`, `render_rule_sqlite()`
- `bind_params()`

---

## 8. Summary: Effort vs. Impact Matrix

| Connector | Effort | Impact | ROI | Priority |
|---|---|---|---|---|
| **Cassandra** | Very low (replace `_wait` with `co_await`) | High (already async internally) | Highest | 1st |
| **PostgreSQL** | Medium (state machine, but clean API) | Very high (most-used SQL DB) | Very high | 2nd |
| **MongoDB** | Low (thread pool wrapper) | Medium | High | 3rd |
| **MySQL** | Medium (5 sequential async steps per query) | High | High | 4th |
| **Redis** | Medium (event loop adapter) | Medium | Medium | 5th |
| **SQLite** | Low (thread pool + WAL) | Low (embedded, no network) | Low | 6th |
| **Neo4j** | Low (thread pool wrapper) | Low (niche DB) | Low | 7th |

### Recommended Implementation Order

1. **Cassandra** — proves the coroutine infrastructure works with minimal risk.
2. **PostgreSQL** — proves the event loop + fd-based async path works.
3. **MongoDB** — proves the thread pool offload path works.
4. **MySQL** — most complex conversion; builds on proven infrastructure.
5. **Redis** — requires the hiredis adapter; builds on event loop.
6. **SQLite** — lowest priority; embedded DB, minimal async benefit.
7. **Neo4j** — lowest priority; same pattern as MongoDB.
