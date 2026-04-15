# ORM Library for C++23

A zero-overhead, compile-time query builder ORM for C++23. All query structure lives in the type system — no runtime parsing, no string manipulation at the call site.

## Requirements

| Compiler | Minimum standard | Notes |
|:--------:|:----------------:|:------|
| GCC 15+  | C++23            | Primary CI target |
| Clang 18+ | C++23           | Supported |
| MSVC 19.38+ | C++23         | Supported |

> **C++26 reflection** (`__cpp_impl_reflection`): when detected, property column names are inferred automatically. Otherwise Boost.PFR is used and the string argument is mandatory.

## CMake integration

```cmake
add_subdirectory(lib)          # builds orm::orm (header-only interface)

target_link_libraries(my_app PRIVATE
    orm::orm          # core ORM headers
    orm::async        # async event loop, coroutines, thread pool
    orm::mockdb       # in-memory SQL renderer (testing)
    orm::sqlite       # SQLite3 connector (requires SQLite3 installed)
)
```

The `orm::sqlite` target is only created when `find_package(SQLite3)` succeeds.
The `orm::async` target provides C++20 coroutine primitives (`Task<T>`, `IoContext`, `ThreadPool`).

## Quick start

### 1 — Define an entity

```cpp
#include <ORM/ORM.hpp>

struct User
{
    orm::property<int,          "id">    id;
    orm::property<std::u8string,"name">  name;
    orm::property<double,       "score"> score;
};

namespace orm {
    template <> struct table_name_trait<User> {
        static constexpr std::string_view value = "users";
    };
}
```

### 2 — Open a connection

```cpp
// SQLite (real storage)
orm::SQLiteDB conn = orm::SQLiteDB::open("my.db");
orm::db<orm::SQLiteDB> db{conn};

// MockDB (in-memory SQL renderer — ideal for unit tests)
orm::MockDB mock;
orm::db<orm::MockDB> db{mock};
```

### 3 — Query

```cpp
// SELECT id, name FROM users
constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&User::name>);
auto result = db << q;                    // orm::result<std::tuple<int,std::u8string>, ...>

// Iterate rows
for (const auto& row : result)
    std::cout << std::get<0>(row) << "\n";

// Access by member pointer (compile-time column lookup)
for (const auto& row : result)
    std::cout << result.get_field<&User::name>(row) << "\n";

// Materialise
std::vector<std::tuple<int,std::u8string>> rows = result.to_vector();
```

### 4 — WHERE + runtime parameters

**Anonymous placeholders** — consumed left-to-right:

```cpp
constexpr auto q = orm::select(orm::field<&User::id>)
    .where(orm::field<&User::id> == orm::Placeholder<int>{});

auto res = db.execute(q, 42);   // binds 42 to the single slot
```

**Indexed placeholders** — `orm::ph<T, std::placeholders::_N>` lets you name each slot explicitly. The same index can appear multiple times to reuse one argument:

```cpp
using namespace std::placeholders;

// _1 appears twice: both conditions receive the same runtime argument
constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&User::score>)
    .where((orm::field<&User::id>    == orm::ph<int, _1>)
        && (orm::field<&User::score>  > orm::ph<double, _2>)
        && (orm::field<&User::id>    == orm::ph<int, _1>));   // _1 reused

auto res = db.execute(q, 42, 9.5);  // _1 → 42, _2 → 9.5
```

> `orm::ph<T, _N>` is a `constexpr` variable template of type `orm::IndexedPlaceholder<T, N>`.
> The SQL rendered by indexed placeholders uses SQLite's `?NNN` syntax (`?1`, `?2`, …),
> which natively supports binding the same slot multiple times.

### 5 — ORDER BY, GROUP BY, LIMIT

```cpp
constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&User::score>)
    .order_by<orm::order::direction::desc>(orm::field<&User::score>)
    .group_by(orm::field<&User::id>)
    .limit(10_per_page & 1_page);
```

### 6 — JOIN

```cpp
constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&Post::body>)
    .join<orm::join::mode::inner, Post>(
        orm::field<&User::id> == orm::field<&Post::author_id>);
```

### 7 — INSERT / UPDATE / DELETE

```cpp
// INSERT INTO users (id, name) VALUES (?, ?)
constexpr auto ins = orm::insert(orm::field<&User::id>, orm::field<&User::name>);
db.execute(ins, 1, u8"alice");

// UPDATE users SET name = ? WHERE id = ?
constexpr auto upd = orm::update<User>()
    .set(orm::field<&User::name>, orm::Placeholder<std::u8string>{})
    .where(orm::field<&User::id> == orm::Placeholder<int>{});
db.execute(upd, u8"bob", 1);

// DELETE FROM users WHERE id = ?
constexpr auto del = orm::deleteq<User>()
    .where(orm::field<&User::id> == orm::Placeholder<int>{});
db.execute(del, 1);
```

### 8 — Prepared statements (`db.prepare()`)

Call `db.prepare(query)` to bind a query IR to a specific `db` instance, returning a `prepared_query<DB, Query>`. Store it as `static const` (or any long-lived object) to avoid reconstructing the IR on every call:

```cpp
using namespace std::placeholders;

// constructed once — ideal as a static local inside a hot function
static const auto pq = db.prepare(
    orm::select(orm::field<&User::id>, orm::field<&User::name>)
        .where(orm::field<&User::id> == orm::ph<int, _1>));

// executed cheaply on every call — no query IR reconstruction
auto res1 = pq.execute(1);    // WHERE id = 1
auto res2 = pq.execute(42);   // WHERE id = 42
auto res3 = pq.execute(99);   // WHERE id = 99
```

`pq.execute()` is `const`-qualified, so the prepared query can also be stored in a `const` variable or a member of a const-qualified object.

### 9 — find_one

```cpp
constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&User::name>);
orm::optional_result<std::tuple<int,std::u8string>> opt = db.find_one(q);
if (opt)
    std::cout << std::get<1>(*opt) << "\n";
```

## Architecture

```
orm::db<DB>           (sync)         orm::async_db<DB>        (async)
  ├── operator<<                       ├── operator<<  → Task<result<...>>
  ├── execute(q, params...)            ├── async_execute(q, params...)
  ├── find_one(q)                      └── sync_handle() → db<DB>&
  └── prepare(q)
        └── connector_trait<DB>::execute(conn, query_ir, params...)
              ├── connector_trait<MockDB>   — renders SQL, stores in MockDB::last_sql
              └── connector_trait<SQLiteDB> — prepares + executes sqlite3 statement

orm::async primitives
  ├── Task<T>              — C++20 coroutine task with sync_wait()
  ├── IoContext            — cross-platform event loop (kqueue on macOS)
  ├── ThreadPool           — fixed-size worker thread pool
  ├── run_on_pool(pool, f) — offload blocking callable to pool, resume coroutine
  └── CancellationToken    — cooperative cancellation with callbacks
```

- **Query IR** — fully compile-time, all structure in template parameters. No runtime parsing.
- **`connector_trait<DB>`** — specialise this struct to add a new backend.
- **`prepared_query<DB, Query>`** — returned by `db.prepare()`; stores the IR + connection ref; `execute()` is `const`-qualified for safe use in `static const` locals.
- **Capability gating** — connectors declare `using supports_joins = void;` etc.; missing capability + usage = `static_assert` at the call site.
- **`orm::result<Row, FieldTuple>`** — lazy range over `std::vector<Row>`; supports `get<I>()`, `get_field<&T::m>()`, `to_vector()`, range-for, `find_one()`.
- **`async_db<DB>`** — wraps any sync connector; offloads queries to `ThreadPool` via `run_on_pool`; connectors declaring `supports_async` use native non-blocking I/O instead.
- **`async_connection_pool<DB, N>`** — coroutine-aware pool; `acquire()` returns `Task<async_connection_guard<DB>>`.
- **`async_transaction_guard<DB>`** — async RAII transaction with `co_await txn.commit()` / auto-rollback on destruction.

## Connectors

| Connector | Header | CMake target | Async | Status |
|-----------|--------|--------------|-------|--------|
| MockDB    | `MockDB/mock_db.hpp` | `orm::mockdb` | via `async_db` | Full — renders SQL for test inspection |
| SQLite    | `SQLite/sqlite_db.hpp` | `orm::sqlite` | via `async_db` | Full — SELECT/INSERT/UPDATE/DELETE |
| MySQL     | `MySQLDB/mysql_live.hpp` | `orm::mysql` | native `_start/_cont` | Integration — requires libmysqlclient |
| PostgreSQL| `PostgreSQLDB/postgresql_live.hpp` | `orm::postgresql` | native libpq async | Integration — requires libpq |
| Cassandra | `CassandraDB/cassandra_live.hpp` | `orm::cassandra` | native `CassFuture` bridge | Integration — requires DataStax driver |
| Redis     | `RedisDB/redis_live.hpp` | `orm::redis` | native hiredis async | Integration — requires hiredis |
| MongoDB   | `MongoDBLive/mongodb_live.hpp` | `orm::mongodb` | via `async_db` | Integration — requires libmongoc |
| Neo4j     | `Neo4jDB/neo4j_live.hpp` | `orm::neo4j` | via `async_db` | Integration — requires libneo4j-client |

All connectors automatically work with `async_db<DB>` (thread-pool offload). Connectors marked "native" additionally provide non-blocking I/O via `IoContext` for zero-copy event-driven async.

## Running tests

```bash
cmake -S . -B build -G "Ninja"   # or "MinGW Makefiles" on Windows
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

226 tests across unit + async + integration suites, all passing.

## Benefits

- **Zero-overhead abstractions** — all query structure resolved at compile time; no runtime parsing or allocation at query construction.
- **Type-safe results** — `orm::result<Row, FieldTuple>` carries the exact `std::tuple` type; `get_field<&T::m>()` is a compile-time index lookup.
- **SQL-injection safe** — runtime values always go through prepared-statement parameter binding; no string concatenation of user data.
- **Prepared statement caching** — `db.prepare(q)` returns a `prepared_query` that can be stored as `static const` and executed repeatedly with different parameters at zero IR-reconstruction cost.
- **Connector-agnostic IR** — the same fluent query compiles for any backend; NoSQL connectors translate the same IR to their wire format.
- **Capability-gated** — using `.join()` on a connector that doesn't declare `supports_joins` is a `static_assert`, not a runtime error.
- **Coroutine-native async** — `async_db<DB>` turns any sync connector into a coroutine-based async handle via `ThreadPool` + `run_on_pool`. Connectors with native non-blocking APIs (MySQL, PostgreSQL, Cassandra, Redis) use direct event-loop integration for maximum throughput.
- **Cross-platform event loop** — `IoContext` with kqueue (macOS), io_uring (Linux, planned), IOCP (Windows, planned) backends.

## Async quick start

```cpp
#include <ORM/connector/async_db.hpp>
#include <ORM/async/thread_pool.hpp>

orm::ThreadPool pool(4);
orm::MockDB conn;
orm::async_db<orm::MockDB> adb(conn, pool);

auto task = [&]() -> orm::Task<void> {
    constexpr auto q = orm::select(orm::field<&User::id>);
    auto result = co_await (adb << q);
    // result is ready, query ran on a pool thread
    co_return;
}();
task.sync_wait();
```

## Roadmap

- ~~MySQL connector~~ ✓ (sync + async)
- ~~PostgreSQL connector~~ ✓ (sync + async)
- ~~Cassandra connector~~ ✓ (sync + async)
- ~~Redis connector~~ ✓ (sync + async)
- MongoDB connector
- io_uring backend (Linux)
- IOCP backend (Windows)
- HAVING clause
- IN / NOT IN rules
- COUNT(*) aggregate
- Auto-migration tooling
- C++26 reflection path (column names inferred, no string argument needed)
- Pipeline mode for PostgreSQL (multi-query concurrency over single connection)
- Benchmark infrastructure

## Socials

[![LinkedIn](https://img.shields.io/badge/linkedin-%230077B5.svg?logo=linkedin&logoColor=white)](https://www.linkedin.com/in/alex-tsvetanov/)
