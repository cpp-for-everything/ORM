# `orm::prepared_query<DB, Query>`

> [!info] Header
> `#include <ORM/connector/prepared_query.hpp>` (included automatically via `ORM/connector/db.hpp`)

A query IR bound to a specific `db<DB>` instance. Created by [[db#prepare|`db::prepare()`]]. Enables the **construct-once, execute-many** pattern — ideal for `static constexpr` locals in hot-path functions.

> [!tip] Compile-time queries
> As of the current version, every query builder call (`.where()`, `.limit()`, `.join()`, etc.) is a constant expression. Query objects can therefore be declared `static constexpr` directly, giving the compiler maximum opportunity to optimise the query structure.

## Synopsis

```cpp
template <typename DB, typename Query>
class prepared_query
{
public:
    prepared_query(DB& conn, Query q);

    template <typename... Params>
    auto execute(Params&&... params) const;

    auto execute() const;

    [[nodiscard]] const Query& query() const noexcept;
};
```

## Creating a prepared query

```cpp
orm::SQLiteDB conn = orm::SQLiteDB::open("app.db");
orm::db<orm::SQLiteDB> db{conn};

// Construct once — static constexpr query object
static constexpr auto q =
    orm::select(orm::field<&User::id>, orm::field<&User::name>)
        .where(orm::field<&User::id> == orm::ph<int, _1>);

using namespace std::placeholders;
static const auto pq = db.prepare(q);
```

## Executing repeatedly

```cpp
auto res1 = pq.execute(1);
auto res2 = pq.execute(42);
auto res3 = pq.execute(99);
```

Each call to `.execute()` re-uses the stored query IR and database connection reference. No query reconstruction occurs on repeated calls.

## `execute()` is `const`-qualified

The `execute` method is `const`, which means the prepared query can be:

- Stored as `const auto` or `static const auto`
- Passed as `const prepared_query<DB, Q>&`
- Held in a `const`-qualified struct member

## Inspecting the query IR

```cpp
const auto& ir = pq.query();   // returns const Query& — the stored query IR
```

## With indexed placeholders

```cpp
using namespace std::placeholders;

static const auto pq = db.prepare(
    orm::select(orm::field<&User::id>)
        .where((orm::field<&User::id> == orm::ph<int, _1>)
            && (orm::field<&User::id> == orm::ph<int, _1>)));  // _1 reused

auto rows = pq.execute(42);   // both ?1 slots receive 42
```

## Without parameters

```cpp
static const auto all_users = db.prepare(
    orm::select(orm::field<&User::id>, orm::field<&User::name>));

auto rows = all_users.execute();   // no params needed
```

## Related

- [[db|`orm::db<DB>`]] — the factory for prepared queries (`db.prepare()`)
- [[placeholders|Placeholders]] — `Placeholder<T>` and `ph<T,N>`
- [[connectors|Connectors]] — `MockDB`, `SQLiteDB`
