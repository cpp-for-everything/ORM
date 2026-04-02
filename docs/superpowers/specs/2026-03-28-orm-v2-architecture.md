# ORM v2 Architecture Specification

**Date:** 2026-03-28  
**Status:** Approved by user — ready for implementation planning  
**Author:** Design session with Alex Tsvetanov

---

## 1. Goals

Rewrite the C++ ORM (previously SQL-only, compile-time, header-heavy) to:

1. Support both **SQL** (MySQL, PostgreSQL, SQLite, …) and **NoSQL** (MongoDB, Cassandra, Redis, …) databases.
2. Require **zero user-code changes** except the type of the database variable.
3. Keep the user entirely in **C++ — no DB-native types, no SQL strings, no BSON** ever visible at call sites.
4. Stay **compile-time first**: all query structure is encoded in template parameters; runtime query construction is also possible but the compile-time path is the primary one.
5. Be **ecosystem-friendly**: third-party connector authors can publish connectors without PRs to this repo.
6. Support **C++26 static reflection** (`^^` operator, `std::meta`) where available; fall back to Boost.PFR where not.

---

## 2. Connector Architecture — `connector_trait<DB>`

### 2.1 Rationale

The connector is a **dumb tag type** + a `connector_trait<DB>` specialization. This follows the `std::iterator_traits` pattern: the tag can be a third-party type the author cannot modify; all ORM logic lives in the trait specialization.

```cpp
// A connector author ships two things:

// 1. A tag type (can be an existing third-party DB handle wrapper)
struct MyPostgresDB {
    // holds a libpq PGconn* internally — never exposed to ORM users
};

// 2. A trait specialization
template<>
struct orm::connector_trait<MyPostgresDB> {

    // ── Capability tags ──────────────────────────────────────────────
    // Presence = supported. Absence = compile error if used.
    using supports_joins         = void;
    using supports_transactions  = void;
    using supports_aggregation   = void;
    // (no supports_embedding → embedded relationships become FK joins)

    // ── C++ ↔ wire type mapping ──────────────────────────────────────
    template<typename CppType> struct wire_type;
    // e.g.:
    //   wire_type<int>                → int
    //   wire_type<orm::datetime>      → std::string  (ISO 8601 for libpq)
    //   wire_type<std::u8string>      → const char*
    //   wire_type<std::vector<uint8_t>> → libpq bytea

    // ── The single entry point the ORM core calls ────────────────────
    template<typename QueryIR, typename... Params>
    static orm::result<typename QueryIR::response_type>
    execute(MyPostgresDB& conn, QueryIR query, Params... params);
};
```

### 2.2 Capability gating

The ORM core uses `if constexpr` + concept checks:

```cpp
template<typename DB, typename Query>
auto orm::db<DB>::operator<<(Query q) {
    if constexpr (has_join_clauses_v<Query>) {
        static_assert(orm::has_capability<DB, orm::supports_joins>,
            "This connector does not support JOIN. "
            "Use store_as::reference relationships or switch connectors.");
    }
    return orm::connector_trait<DB>::execute(_conn, q);
}
```

Missing capability + usage = **hard compile error with a clear message**. No runtime surprises.

### 2.3 Third-party connector checklist

A connector author must provide:
- A tag type (or reuse an existing class)
- A `connector_trait<Tag>` specialization with:
  - Capability tags for what the DB supports
  - `wire_type<CppType>` specializations for each C++ type used
  - `execute(conn, query_ir, params...)` returning `orm::result<...>`

No changes to the ORM repository are required.

---

## 3. Type System — Pure C++ Types

Users **never write DB-native types**. The `connector_trait` maps every C++ type to the DB wire format internally.

### 3.1 Standard C++ types

| C++ type | Default DB mapping |
|----------|--------------------|
| `int`, `int32_t`, `int64_t`, … | `INTEGER` / numeric |
| `bool` | `TINYINT(1)` / boolean |
| `float`, `double` | `FLOAT`, `DOUBLE` |
| `std::u8string` | `VARCHAR` / `TEXT` (UTF-8) |
| `std::u16string` | `NVARCHAR` / UTF-16 aware column |
| `std::u32string` | `LONGTEXT` / full Unicode |
| `std::vector<uint8_t>` | `BLOB` / binary |

### 3.2 ORM size-constrained wrappers

Zero-overhead wrappers carrying size as a template parameter. Still pure C++ — no DB vocabulary.

| ORM type | DB mapping |
|----------|-----------|
| `orm::fixed_string<N>` | `CHAR(N)` |
| `orm::varbinary<N>` | `VARBINARY(N)` |
| `orm::binary<N>` | `BINARY(N)` |

### 3.3 ORM chrono aliases

Thin aliases over `std::chrono`. Users stay in C++ time types; connectors convert to their wire format.

| ORM alias | Underlying C++ type | DB mapping |
|-----------|---------------------|-----------|
| `orm::datetime` | `std::chrono::system_clock::time_point` | `DATETIME` / ISODate |
| `orm::timestamp` | `std::chrono::utc_clock::time_point` | `TIMESTAMP` / UTC epoch |
| `orm::date` | `std::chrono::year_month_day` | `DATE` |
| `orm::time_of_day` | `std::chrono::hh_mm_ss<std::chrono::seconds>` | `TIME` |
| `orm::year` | `std::chrono::year` | `YEAR` |

### 3.4 ORM constrained value types

| ORM type | Underlying C++ type | DB mapping |
|----------|---------------------|-----------|
| `orm::enum_t<"a","b","c">` | compile-time-validated `std::u8string` | `ENUM('a','b','c')` |
| `orm::set_t<"x","y","z">` | `std::set<std::u8string>` (validated values) | `SET('x','y','z')` |

### 3.5 Encoding

- **No `wchar_t` or `std::wstring`** anywhere in the ORM.
- Text encoding is expressed via the C++ type: `u8string` = UTF-8, `u16string` = UTF-16, `u32string` = UTF-32.
- Connectors transcode from the C++ encoding to their wire encoding internally.

---

## 4. Entity Model

### 4.1 `property<>` — scalar fields

```cpp
// PFR fallback path (no C++26 reflection): string arg is mandatory
property<int, "id">                         id;
property<std::u8string, "name">             name;
property<orm::fixed_string<50>, "code">     code;
property<orm::datetime, "created_at">       created_at;
property<orm::enum_t<"active","banned">, "status"> status;

// C++26 reflection path (__cpp_impl_reflection defined):
// string arg is OPTIONAL — inferred from the field identifier via std::meta::name_v
property<int>          id;           // column name = "id"
property<std::u8string, "user_name"> name; // override: column name = "user_name"
```

### 4.2 Relationships — defaults inferred from C++ shape

The user writes natural C++ member types. The ORM core inspects the shape:

| C++ member type | Inferred relationship | SQL storage | NoSQL storage |
|-----------------|----------------------|-------------|---------------|
| `OtherTable field` | `one2one` | FK column | Embedded document |
| `std::vector<OtherTable> field` | `one2many` | Separate table + FK | Embedded array |
| `std::list<OtherTable> field` | `one2many` | Separate table + FK | Embedded array |

### 4.3 `relationship<>` — explicit override

When the default inference is wrong, the user overrides with an explicit annotation:

```cpp
// Always store as reference (FK join in SQL, $lookup in MongoDB)
relationship<store_as::reference, Post, "posts">             posts;

// Always embed (denormalized JSON column in SQL, embedded doc in NoSQL)
relationship<store_as::embed, Post, "archived">              archived;

// One-to-many reference
relationship<store_as::reference, std::vector<Post>, "likes"> likes;
```

### 4.4 Full example

```cpp
struct Post {
    property<int, "id">             id;
    property<std::u8string, "body"> body;
    property<orm::datetime, "ts">   ts;
};

struct User {
    property<int, "id">                                id;
    property<orm::fixed_string<100>, "name">           name;
    property<orm::enum_t<"active","banned">, "status"> status;

    Post             latest_post;               // inferred: one2one
    std::vector<Post> drafts;                  // inferred: one2many

    relationship<store_as::reference, Post,              "posts">    posts;
    relationship<store_as::embed,     Post,              "archived"> archived;
    relationship<store_as::reference, std::vector<Post>, "likes">    likes;
};
```

---

## 5. Query IR — Compile-Time Fluent Builder

The query IR is **entirely type-level**: all structure (selected fields, join conditions, where clauses, ordering, limits) is encoded in template parameters. The full query object is `constexpr`-constructible with no runtime overhead.

### 5.1 Field references in queries

Two co-existing syntaxes:

```cpp
// C++26 path (__cpp_impl_reflection defined): direct ^^ operator
select(^^User::id, ^^User::name, ^^Post::body)
    .where(^^User::id == Placeholder<int>);

// Fallback path (PFR): field<> wrapper — replaces old P<>
select(field<&User::id>, field<&User::name>, field<&Post::body>)
    .where(field<&User::id> == Placeholder<int>);
```

`field<&User::id>` is identical in mechanism to the old `P<&User::id>` — a `mem_ptr` wrapper enabling `operator==`, `operator<`, etc. to produce `Rule<...>` objects — but with a meaningful name.

### 5.2 CRUD builders

```cpp
// SELECT
constexpr auto q = select(field<&User::id>, field<&Post::body>)
    .join<orm::join::inner, Post>(field<&User::id> == field<&Post::author_id>)
    .where(field<&User::status> == Placeholder<std::u8string>)
    .order_by<orm::order::direction::asc>(field<&User::name>)
    .limit(10_per_page, 1_page);

// INSERT
constexpr auto q = insert(field<&User::name>, field<&User::status>);

// UPDATE
constexpr auto q = update(field<&User::status> = Placeholder<std::u8string>)
    .where(field<&User::id> == Placeholder<int>);

// DELETE
constexpr auto q = deleteq<User>()
    .where(field<&User::id> == Placeholder<int>);
```

### 5.3 Runtime query construction

Runtime paths are also supported for dynamic filters:

```cpp
auto q = select(field<&User::id>);
if (filter_by_status)
    q = q.where(field<&User::status> == Placeholder<std::u8string>);
auto result = db << q("active");
```

### 5.4 Execution

```cpp
orm::db<MyDB> db(conn);

// Constexpr query, parameterised execution
auto rows = db << q(42, "active");

// Constexpr query with no parameters
auto all  = db << select_all_users;
```

`db << query(params...)` returns `orm::result<...>`.

---

## 6. Result Type

```cpp
// Lazy range — does not materialise rows until iterated
orm::result<...> rows = db << UserPost::select_all(user_id);

for (auto& row : rows) {
    int  id   = row.get<&User::id>();
    auto name = row.get<&User::name>();  // → std::u8string (or orm::fixed_string<100>)
}

// Materialise all rows at once
auto vec = rows.to_vector();

// Single-row queries
orm::optional_result<User> maybe = db << find_user(42);
if (maybe) { auto& u = *maybe; }
```

`orm::result<...>` is backend-agnostic. The connector drives iteration via a cursor abstraction declared in `connector_trait`.

---

## 7. Reflection Strategy

| Condition | Mechanism | Effect on user code |
|-----------|-----------|---------------------|
| `__cpp_impl_reflection` defined (GCC 16+, Bloomberg clang fork) | C++26 `std::meta`, `^^` operator, `std::meta::name_v` | `property<int> id;` (no string), `^^User::id` in queries |
| No reflection support (MSVC, older GCC/Clang) | Boost.PFR | `property<int, "id"> id;`, `field<&User::id>` in queries |

The two paths are selected via `#ifdef __cpp_impl_reflection`. User code that uses `field<>` will compile on all compilers. User code that uses `^^` requires a C++26-capable compiler.

---

## 8. Directory / Module Structure (proposed)

```
lib/
  include/
    ORM/
      ORM.hpp               — top-level include
      entity/
        property.hpp        — property<> template
        relationship.hpp    — relationship<> + store_as
        table.hpp           — Table / DBTable
      types/
        wrappers.hpp        — orm::fixed_string, binary, varbinary
        chrono.hpp          — orm::datetime, timestamp, date, …
        constrained.hpp     — orm::enum_t, orm::set_t
      query/
        field.hpp           — field<> / mem_ptr wrapper
        rules.hpp           — Rule<>, operator overloads
        placeholders.hpp    — Placeholder<T>
        select.hpp          — select_query
        insert.hpp          — insert_query
        update.hpp          — update_query
        delete.hpp          — delete_query
        limits.hpp          — Pagification / _per_page / _page
        join_rule.hpp       — JoinRule, GroupByRule, OrderBy
      result/
        result.hpp          — orm::result<>, orm::optional_result<>
      connector/
        trait.hpp           — connector_trait<DB> primary template
        capabilities.hpp    — supports_joins, supports_transactions, …
        db.hpp              — orm::db<DB> — the user-facing DB handle
      details/
        orm_tuple.hpp       — extended tuple utilities
        member_pointer.hpp  — mem_ptr, i_mem_ptr, is_property concepts
        reflection.hpp      — #ifdef __cpp_impl_reflection adapter
        pfr_adapter.hpp     — Boost.PFR adapter (fallback)
  src/
    ORM/
      rules.cpp             — Rule operator implementations (template .cpp)
      details/
        result_type.cpp
```

---

## 9. Decision Log

| # | Topic | Decision |
|---|-------|----------|
| 1 | Connector model | `connector_trait<DB>` specialization — tag type + trait |
| 2 | Third-party connectors | No PR needed; specialize `orm::connector_trait<TheirDB>` |
| 3 | Type vocabulary | Pure C++ types; `orm::` thin wrappers for DB-semantic types |
| 4 | String encoding | `std::u8string`/`u16string`/`u32string`; no `wchar_t` |
| 5 | Fixed-size types | `orm::fixed_string<N>`, `orm::binary<N>`, `orm::varbinary<N>` |
| 6 | Temporal types | `orm::datetime/timestamp/date/time_of_day/year` over `std::chrono` |
| 7 | Constrained values | `orm::enum_t<vals...>`, `orm::set_t<vals...>` |
| 8 | Relationships | Natural C++ members inferred; `relationship<store_as::X>` to override |
| 9 | Reflection | C++26 `std::meta` when `__cpp_impl_reflection`; PFR fallback |
| 10 | Column naming | C++26: string arg optional (inferred); PFR: mandatory |
| 11 | Query field syntax | `^^User::id` (C++26) and `field<&User::id>` (fallback) — both supported |
| 12 | Query IR shape | SQL-superset constexpr fluent builder; NoSQL connectors translate |
| 13 | Capability gating | Trait nested type tags; missing = `static_assert` with clear message |
| 14 | Result type | `orm::result<...>` lazy range; `.get<&Table::field>()` accessor |
| 15 | Execution API | `db << query(params...)` → `orm::result` |
