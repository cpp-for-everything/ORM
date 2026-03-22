# ORM v2 — Plan C: Connector Trait, Capabilities, orm::db<>, orm::result<>

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the connector abstraction layer: `connector_trait<DB>` primary template, capability tags, compile-time capability gating, the `orm::db<DB>` user-facing handle, and the `orm::result<>` / `orm::optional_result<>` lazy result type.

**Architecture:** `connector_trait<DB>` is a trait specialization pattern (iterator_traits style). The `orm::db<DB>` handle enforces capability checks via `static_assert` at the point of `operator<<`. `orm::result<>` is a lazy input range driven by a cursor abstraction declared in the trait. No actual database I/O is implemented here — that is Plan D (MockDB).

**Tech Stack:** C++20; GoogleTest; depends on Plan A (types) and Plan B (query IR).

**Spec:** `docs/superpowers/specs/2026-03-28-orm-v2-architecture.md` §2, §6

**Depends on:** Plan A + Plan B complete  
**Required by:** Plan D (MockDB connector)

---

## Chunk 1: Capability tags and `connector_trait<DB>` primary template

### File map

| Action | Path | Responsibility |
|--------|------|---------------|
| Create | `lib/include/ORM/connector/capabilities.hpp` | Capability tag structs + `has_capability<DB, Cap>` concept |
| Create | `lib/include/ORM/connector/trait.hpp` | `connector_trait<DB>` primary template (unspecialized = compile error) |
| Create | `tests/connector/CMakeLists.txt` | Connector test suite |
| Create | `tests/connector/test_capabilities.cpp` | Capability tag tests |

---

### Task 1: Capability tags

**Files:**
- Create: `lib/include/ORM/connector/capabilities.hpp`
- Create: `tests/connector/CMakeLists.txt`
- Create: `tests/connector/test_capabilities.cpp`

- [ ] **Step 1: Write failing tests**

Create `tests/connector/CMakeLists.txt`:

```cmake
add_executable(test_connector
    test_capabilities.cpp
    test_trait.cpp
    test_db_handle.cpp
    test_result.cpp
)

target_link_libraries(test_connector PRIVATE
    orm::orm
    GTest::gtest_main
)

gtest_discover_tests(test_connector)
```

Add to `tests/CMakeLists.txt`:

```cmake
add_subdirectory(connector)
```

Create stub files:

```bash
touch tests/connector/test_trait.cpp tests/connector/test_db_handle.cpp
touch tests/connector/test_result.cpp
```

Each stub:
```cpp
#include <gtest/gtest.h>
// Tests added in subsequent tasks
```

Create `tests/connector/test_capabilities.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/connector/capabilities.hpp"

// A connector with joins and transactions
struct FullDB {};
template<> struct orm::connector_trait<FullDB> {
    using supports_joins        = void;
    using supports_transactions = void;
};

// A connector with no capabilities declared
struct MinimalDB {};
template<> struct orm::connector_trait<MinimalDB> {};

TEST(Capabilities, HasJoinsWhenDeclared) {
    static_assert(orm::has_capability<FullDB, orm::cap::supports_joins>);
}

TEST(Capabilities, NoJoinsWhenNotDeclared) {
    static_assert(!orm::has_capability<MinimalDB, orm::cap::supports_joins>);
}

TEST(Capabilities, HasTransactionsWhenDeclared) {
    static_assert(orm::has_capability<FullDB, orm::cap::supports_transactions>);
}

TEST(Capabilities, NoTransactionsWhenNotDeclared) {
    static_assert(!orm::has_capability<MinimalDB, orm::cap::supports_transactions>);
}

TEST(Capabilities, NoAggregationByDefault) {
    static_assert(!orm::has_capability<FullDB, orm::cap::supports_aggregation>);
    static_assert(!orm::has_capability<MinimalDB, orm::cap::supports_aggregation>);
}
```

- [ ] **Step 2: Confirm compile error**

```bash
cmake --build build 2>&1 | head -20
```

- [ ] **Step 3: Implement capabilities.hpp**

Create `lib/include/ORM/connector/capabilities.hpp`:

```cpp
#pragma once
#include <type_traits>

namespace orm {

// ── connector_trait<DB> primary template (forward declaration) ────────────────
// Must be specialised by every connector. An unspecialized use is an error.
template <typename DB>
struct connector_trait {
    static_assert(sizeof(DB) == 0,
        "orm::connector_trait<DB> has not been specialised for this DB type. "
        "Provide a template<> struct orm::connector_trait<YourDB> { ... }; "
        "specialisation — see docs/superpowers/specs/2026-03-28-orm-v2-architecture.md §2.");
};

// ── Capability tag structs ────────────────────────────────────────────────────
namespace cap {
    struct supports_joins {};
    struct supports_transactions {};
    struct supports_aggregation {};
    struct supports_embedding {};      // NoSQL: native document embedding
    struct supports_upsert {};         // INSERT ... ON CONFLICT / upsert
    struct supports_bulk_insert {};
} // namespace cap

// ── has_capability<DB, Cap> ───────────────────────────────────────────────────
// True if connector_trait<DB> has a nested type alias for Cap.
// Convention: alias name = Cap struct name (e.g. `using supports_joins = void;`).

namespace detail {

// Map capability struct -> member alias name check
template <typename DB, typename Cap>
struct capability_check : std::false_type {};

template <typename DB>
struct capability_check<DB, cap::supports_joins>
    : std::bool_constant<requires { typename connector_trait<DB>::supports_joins; }> {};

template <typename DB>
struct capability_check<DB, cap::supports_transactions>
    : std::bool_constant<requires { typename connector_trait<DB>::supports_transactions; }> {};

template <typename DB>
struct capability_check<DB, cap::supports_aggregation>
    : std::bool_constant<requires { typename connector_trait<DB>::supports_aggregation; }> {};

template <typename DB>
struct capability_check<DB, cap::supports_embedding>
    : std::bool_constant<requires { typename connector_trait<DB>::supports_embedding; }> {};

template <typename DB>
struct capability_check<DB, cap::supports_upsert>
    : std::bool_constant<requires { typename connector_trait<DB>::supports_upsert; }> {};

template <typename DB>
struct capability_check<DB, cap::supports_bulk_insert>
    : std::bool_constant<requires { typename connector_trait<DB>::supports_bulk_insert; }> {};

} // namespace detail

template <typename DB, typename Cap>
inline constexpr bool has_capability = detail::capability_check<DB, Cap>::value;

} // namespace orm
```

- [ ] **Step 4: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_connector
```

Expected: `Capabilities.*` PASS.

- [ ] **Step 5: Commit**

```bash
git add lib/include/ORM/connector/capabilities.hpp \
        tests/connector/
git commit -m "feat: capability tag structs and has_capability<DB,Cap> trait check"
```

---

### Task 2: `connector_trait<DB>` interface contract

**Files:**
- Create: `lib/include/ORM/connector/trait.hpp`
- Modify: `tests/connector/test_trait.cpp`

- [ ] **Step 1: Write failing tests**

Replace `tests/connector/test_trait.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/connector/trait.hpp"
#include "ORM/connector/capabilities.hpp"
#include "ORM/query/select.hpp"
#include "ORM/entity/property.hpp"
#include "ORM/query/field.hpp"

struct User {
    orm::property<int, "id">            id;
    orm::property<std::u8string, "name"> name;
};

// Minimal valid connector_trait specialization
struct TestDB {};
template<>
struct orm::connector_trait<TestDB> {
    using supports_joins = void;

    // wire_type mapping
    template<typename T> struct wire_type { using type = T; };

    // cursor type for result iteration
    struct cursor_type {
        bool has_next() const { return false; }
        // fetch not called in this test
    };

    template<typename QueryIR, typename... Params>
    static orm::result<int> execute(TestDB&, QueryIR, Params...) {
        return orm::result<int>{};
    }
};

// Unspecialized connector_trait should produce a static_assert
// (we cannot test this at runtime — it is verified by trying to instantiate)

// A valid specialization satisfies the connector concept
TEST(ConnectorTrait, ValidSpecializationSatisfiesConcept) {
    static_assert(orm::is_connector<TestDB>);
}

// wire_type mapping works
TEST(ConnectorTrait, WireTypePassthrough) {
    using W = orm::connector_trait<TestDB>::wire_type<int>::type;
    static_assert(std::is_same_v<W, int>);
}
```

- [ ] **Step 2: Implement trait.hpp**

Create `lib/include/ORM/connector/trait.hpp`:

```cpp
#pragma once
#include "ORM/connector/capabilities.hpp"
#include <type_traits>

namespace orm {

// Forward declaration of result<> (defined in result/result.hpp)
template <typename T> struct result;

// ── is_connector<DB> concept ─────────────────────────────────────────────────
// A valid connector_trait<DB> specialization must provide:
//   - wire_type<CppType>::type   (C++ → wire type mapping)
//   - cursor_type                (result iteration cursor)
//   - execute(DB&, QueryIR, Params...) -> orm::result<...>
template <typename DB>
concept is_connector = requires {
    // wire_type must be a template with ::type
    typename connector_trait<DB>::template wire_type<int>::type;
    // cursor_type must exist
    typename connector_trait<DB>::cursor_type;
};

} // namespace orm
```

- [ ] **Step 3: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_connector
```

Expected: `ConnectorTrait.*` PASS.

- [ ] **Step 4: Commit**

```bash
git add lib/include/ORM/connector/trait.hpp tests/connector/test_trait.cpp
git commit -m "feat: connector_trait<DB> interface + is_connector concept"
```

---

## Chunk 2: `orm::result<>` lazy range

### File map

| Action | Path | Responsibility |
|--------|------|---------------|
| Create | `lib/include/ORM/result/result.hpp` | `orm::result<T>`, `orm::optional_result<T>`, row accessor `.get<&T::f>()` |
| Modify | `tests/connector/test_result.cpp` | Result range tests |

---

### Task 3: `orm::result<>` and `orm::optional_result<>`

**Files:**
- Create: `lib/include/ORM/result/result.hpp`
- Modify: `tests/connector/test_result.cpp`

- [ ] **Step 1: Write failing tests**

Replace `tests/connector/test_result.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/result/result.hpp"
#include "ORM/entity/property.hpp"
#include "ORM/query/field.hpp"
#include <vector>

struct User {
    orm::property<int, "id">            id;
    orm::property<std::u8string, "name"> name;
};

// orm::result<T> is default constructible (empty)
TEST(Result, DefaultConstructibleEmpty) {
    orm::result<int> r;
    EXPECT_TRUE(r.empty());
}

// to_vector on empty result gives empty vector
TEST(Result, ToVectorEmpty) {
    orm::result<int> r;
    auto v = r.to_vector();
    EXPECT_TRUE(v.empty());
}

// result<T> constructed from a vector of values is iterable
TEST(Result, IterableFromVector) {
    std::vector<int> data{1, 2, 3};
    orm::result<int> r(std::move(data));
    int sum = 0;
    for (auto v : r) sum += v;
    EXPECT_EQ(sum, 6);
}

// to_vector roundtrip
TEST(Result, ToVectorRoundtrip) {
    std::vector<int> data{10, 20};
    orm::result<int> r(data);
    EXPECT_EQ(r.to_vector(), data);
}

// optional_result: empty
TEST(OptionalResult, EmptyHasNoValue) {
    orm::optional_result<int> r;
    EXPECT_FALSE(r.has_value());
    EXPECT_FALSE(static_cast<bool>(r));
}

// optional_result: with value
TEST(OptionalResult, WithValueIsAccessible) {
    orm::optional_result<int> r(42);
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(*r, 42);
}
```

- [ ] **Step 2: Confirm compile error**

```bash
cmake --build build 2>&1 | head -20
```

- [ ] **Step 3: Implement result.hpp**

Create `lib/include/ORM/result/result.hpp`:

```cpp
#pragma once
#include <vector>
#include <optional>
#include <iterator>
#include <cstddef>
#include <type_traits>

namespace orm {

// ── orm::result<T> ────────────────────────────────────────────────────────────
// Lazy input range over query results.
// In this implementation, backed by std::vector<T> (materialised by connector).
// Future: extend with cursor-driven lazy fetch via connector_trait::cursor_type.
template <typename T>
struct result {
    using value_type = T;

    result() = default;

    explicit result(std::vector<T> rows) : _rows(std::move(rows)) {}

    // ── Range interface ───────────────────────────────────────────────────────
    auto begin() const { return _rows.cbegin(); }
    auto end()   const { return _rows.cend();   }
    auto begin()       { return _rows.begin();  }
    auto end()         { return _rows.end();    }

    [[nodiscard]] bool empty() const noexcept { return _rows.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return _rows.size(); }

    // ── Materialise ───────────────────────────────────────────────────────────
    std::vector<T> to_vector() const& { return _rows; }
    std::vector<T> to_vector() &&     { return std::move(_rows); }

private:
    std::vector<T> _rows;
};

// ── orm::optional_result<T> ───────────────────────────────────────────────────
// For queries expected to return at most one row.
template <typename T>
struct optional_result {
    optional_result() = default;
    explicit optional_result(T val) : _value(std::move(val)) {}

    [[nodiscard]] bool has_value() const noexcept { return _value.has_value(); }
    explicit operator bool() const noexcept { return has_value(); }

    T&       operator*()       { return *_value; }
    const T& operator*() const { return *_value; }

    T*       operator->()       { return &*_value; }
    const T* operator->() const { return &*_value; }

private:
    std::optional<T> _value;
};

} // namespace orm
```

- [ ] **Step 4: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_connector
```

Expected: `Result.*`, `OptionalResult.*` PASS.

- [ ] **Step 5: Commit**

```bash
git add lib/include/ORM/result/result.hpp tests/connector/test_result.cpp
git commit -m "feat: orm::result<T> lazy range and orm::optional_result<T>"
```

---

## Chunk 3: `orm::db<DB>` — user-facing handle with capability gating

### File map

| Action | Path | Responsibility |
|--------|------|---------------|
| Create | `lib/include/ORM/connector/db.hpp` | `orm::db<DB>` with `operator<<`, capability gating via `static_assert` |
| Modify | `tests/connector/test_db_handle.cpp` | db<> handle tests |

---

### Task 4: `orm::db<DB>` handle

**Files:**
- Create: `lib/include/ORM/connector/db.hpp`
- Modify: `tests/connector/test_db_handle.cpp`

- [ ] **Step 1: Write failing tests**

Replace `tests/connector/test_db_handle.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/connector/db.hpp"
#include "ORM/connector/capabilities.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/result/result.hpp"
#include "ORM/query/select.hpp"
#include "ORM/query/insert.hpp"
#include "ORM/query/update.hpp"
#include "ORM/query/delete.hpp"
#include "ORM/entity/property.hpp"
#include "ORM/query/field.hpp"

struct User {
    orm::property<int, "id">            id;
    orm::property<std::u8string, "name"> name;
};

// ── Spy connector: records executed query type, returns fixed result ──────────
struct SpyDB {
    enum class last_op { none, select_, insert_, update_, delete_ } last{last_op::none};
};

template<>
struct orm::connector_trait<SpyDB> {
    using supports_joins        = void;
    using supports_transactions = void;

    template<typename T> struct wire_type { using type = T; };
    struct cursor_type { bool has_next() const { return false; } };

    template<typename QueryIR, typename... Params>
    static orm::result<int> execute(SpyDB& db, QueryIR, Params...) {
        if constexpr (orm::is_select_query<QueryIR>)
            db.last = SpyDB::last_op::select_;
        else if constexpr (orm::is_insert_query<QueryIR>)
            db.last = SpyDB::last_op::insert_;
        else if constexpr (orm::is_update_query<QueryIR>)
            db.last = SpyDB::last_op::update_;
        else if constexpr (orm::is_delete_query<QueryIR>)
            db.last = SpyDB::last_op::delete_;
        return orm::result<int>{};
    }
};

// db<> wraps connection and executes queries via operator<<
TEST(DbHandle, ConstructsFromConnection) {
    SpyDB conn;
    orm::db<SpyDB> db(conn);
    // just construction — no exception
}

TEST(DbHandle, ExecutesSelectQuery) {
    SpyDB conn;
    orm::db<SpyDB> db(conn);
    constexpr auto q = orm::select(orm::field<&User::id>);
    db << q;
    EXPECT_EQ(conn.last, SpyDB::last_op::select_);
}

TEST(DbHandle, ExecutesInsertQuery) {
    SpyDB conn;
    orm::db<SpyDB> db(conn);
    constexpr auto q = orm::insert(orm::field<&User::name>);
    db << q;
    EXPECT_EQ(conn.last, SpyDB::last_op::insert_);
}

TEST(DbHandle, ExecutesUpdateQuery) {
    SpyDB conn;
    orm::db<SpyDB> db(conn);
    constexpr auto q = orm::update<User>()
        .set(orm::field<&User::name>, orm::Placeholder<std::u8string>{});
    db << q;
    EXPECT_EQ(conn.last, SpyDB::last_op::update_);
}

TEST(DbHandle, ExecutesDeleteQuery) {
    SpyDB conn;
    orm::db<SpyDB> db(conn);
    constexpr auto q = orm::deleteq<User>();
    db << q;
    EXPECT_EQ(conn.last, SpyDB::last_op::delete_);
}

// Capability gating: JOIN on a connector without supports_joins = compile error
// (tested by negative compilation — not a runtime test)
// To verify: uncomment the following and expect a static_assert failure:
//
// struct NoJoinDB {};
// template<> struct orm::connector_trait<NoJoinDB> {
//     template<typename T> struct wire_type { using type = T; };
//     struct cursor_type { bool has_next() const { return false; } };
//     template<typename Q, typename... P>
//     static orm::result<int> execute(NoJoinDB&, Q, P...) { return {}; }
// };
// void check_nojoin() {
//     NoJoinDB conn;
//     orm::db<NoJoinDB> db(conn);
//     constexpr auto q = orm::select(orm::field<&User::id>)
//         .join<orm::join::mode::inner, User>(
//             orm::field<&User::id> == orm::field<&User::id>);
//     db << q;  // <-- should static_assert here
// }
```

- [ ] **Step 2: Confirm compile error**

```bash
cmake --build build 2>&1 | head -20
```

- [ ] **Step 3: Implement db.hpp**

Create `lib/include/ORM/connector/db.hpp`:

```cpp
#pragma once
#include "ORM/connector/trait.hpp"
#include "ORM/connector/capabilities.hpp"
#include "ORM/result/result.hpp"
#include "ORM/query/select.hpp"
#include "ORM/query/insert.hpp"
#include "ORM/query/update.hpp"
#include "ORM/query/delete.hpp"
#include <type_traits>
#include <utility>

namespace orm {

template <typename DB>
    requires is_connector<DB>
class db {
public:
    explicit db(DB& connection) : _conn(connection) {}

    // ── operator<< dispatches to connector_trait::execute ─────────────────────
    template <typename Query>
    auto operator<<(Query q) {
        // ── Capability gating ────────────────────────────────────────────────
        if constexpr (is_select_query<Query>) {
            if constexpr (decltype(q)::Joins::size > 0) {
                static_assert(has_capability<DB, cap::supports_joins>,
                    "orm::db: This connector does not support JOIN operations. "
                    "Remove .join() clauses or use store_as::reference relationships, "
                    "or switch to a connector that declares supports_joins.");
            }
        }
        return connector_trait<DB>::execute(_conn, std::move(q));
    }

    // ── operator<< with runtime parameters ───────────────────────────────────
    template <typename Query, typename... Params>
    auto execute(Query q, Params&&... params) {
        return connector_trait<DB>::execute(
            _conn, std::move(q), std::forward<Params>(params)...);
    }

    [[nodiscard]] DB& connection() noexcept { return _conn; }

private:
    DB& _conn;
};

} // namespace orm
```

- [ ] **Step 4: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_connector
```

Expected: all `DbHandle.*` PASS. Full connector suite PASS.

- [ ] **Step 5: Commit**

```bash
git add lib/include/ORM/connector/db.hpp tests/connector/test_db_handle.cpp
git commit -m "feat: orm::db<DB> handle with operator<< and compile-time capability gating"
```

---

### Task 5: Update ORM.hpp and run full suite

- [ ] **Step 1: Update ORM.hpp**

Add to `lib/include/ORM/ORM.hpp`:

```cpp
#include "ORM/result/result.hpp"
#include "ORM/connector/capabilities.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/connector/db.hpp"
```

- [ ] **Step 2: Build and run all tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: ALL tests PASS — foundation, query, connector suites.

- [ ] **Step 3: Final commit**

```bash
git add lib/include/ORM/ORM.hpp
git commit -m "feat: complete Plan C — connector_trait, capability gating, orm::db<>, orm::result<>"
```

---

## Plan C completion checklist

- [ ] `orm::cap::supports_joins/transactions/aggregation/embedding/upsert/bulk_insert` tags
- [ ] `orm::has_capability<DB, Cap>` compile-time predicate
- [ ] `orm::connector_trait<DB>` primary template (unspecialized = static_assert)
- [ ] `orm::is_connector<DB>` concept
- [ ] `orm::result<T>` lazy range with `begin/end`, `empty()`, `size()`, `to_vector()`
- [ ] `orm::optional_result<T>` with `has_value()`, `operator*`, `operator->`
- [ ] `orm::db<DB>` with `operator<<`, capability gating, `.execute(q, params...)`
- [ ] SpyDB connector (in tests) validates the full dispatch path
- [ ] All tests pass, zero warnings

**Next:** Plan D — MockDB connector (SQL string renderer) + full integration tests
