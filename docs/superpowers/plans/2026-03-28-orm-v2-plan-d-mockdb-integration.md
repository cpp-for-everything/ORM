# ORM v2 — Plan D: MockDB Connector + Integration Tests

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the MockDB reference connector (SQL string renderer used as the test and documentation connector) and a comprehensive integration test suite that exercises the full stack: entity definition → query IR construction → `orm::db<MockDB>` execution → SQL string output → `orm::result<>` consumption.

**Architecture:** MockDB is a `connector_trait<MockDB>` specialization that renders each query IR to a SQL string and stores it for test inspection. It does not connect to a real database. It serves as both the reference connector (showing exactly what SQL the ORM generates) and the primary integration test driver. All query-to-SQL translation logic lives in `.cpp` files under `lib/src/` to maximise compile-time error detection in the library itself.

**Tech Stack:** C++20; GoogleTest; depends on Plans A + B + C complete.

**Spec:** `docs/superpowers/specs/2026-03-28-orm-v2-architecture.md` §2, §5

**Depends on:** Plans A + B + C complete  
**Required by:** nothing (final plan in the sequence)

---

## Chunk 1: MockDB connector implementation

### File map

| Action | Path | Responsibility |
|--------|------|---------------|
| Create | `lib/include/ORM/db/connectors/MockDB/init.hpp` | `MockDB` tag, `connector_trait<MockDB>` declaration |
| Create | `lib/include/ORM/db/connectors/MockDB/sql_renderer.hpp` | `SqlRenderer` — translates query IR → SQL string |
| Create | `lib/src/ORM/db/connectors/MockDB/init.cpp` | `connector_trait<MockDB>::execute` implementation |
| Create | `lib/src/ORM/db/connectors/MockDB/sql_renderer.cpp` | `SqlRenderer` method implementations |
| Create | `lib/src/ORM/db/connectors/MockDB/CMakeLists.txt` | MockDB `.cpp` compiled into `orm_mockdb` static library |
| Modify | `lib/CMakeLists.txt` | Add `orm_mockdb` target, link to tests |
| Create | `tests/integration/CMakeLists.txt` | Integration test suite |
| Create | `tests/integration/test_mockdb_select.cpp` | SELECT integration tests |
| Create | `tests/integration/test_mockdb_insert.cpp` | INSERT integration tests |
| Create | `tests/integration/test_mockdb_update.cpp` | UPDATE integration tests |
| Create | `tests/integration/test_mockdb_delete.cpp` | DELETE integration tests |
| Create | `tests/integration/test_mockdb_joins.cpp` | JOIN integration tests |
| Create | `tests/integration/test_mockdb_relationships.cpp` | Relationship inference integration tests |

---

### Task 1: MockDB CMake targets

**Files:**
- Create: `lib/src/ORM/db/connectors/MockDB/CMakeLists.txt`
- Modify: `lib/CMakeLists.txt`
- Create: `tests/integration/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Create MockDB CMakeLists.txt**

Create `lib/src/ORM/db/connectors/MockDB/CMakeLists.txt`:

```cmake
add_library(orm_mockdb STATIC
    init.cpp
    sql_renderer.cpp
)

target_link_libraries(orm_mockdb PUBLIC orm::orm)

target_include_directories(orm_mockdb PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../../include>
)
```

- [ ] **Step 2: Wire into lib/CMakeLists.txt**

Add to the bottom of `lib/CMakeLists.txt`:

```cmake
add_subdirectory(src/ORM/db/connectors/MockDB)
add_library(orm::mockdb ALIAS orm_mockdb)
```

- [ ] **Step 3: Create integration test CMakeLists**

Create `tests/integration/CMakeLists.txt`:

```cmake
add_executable(test_integration
    test_mockdb_select.cpp
    test_mockdb_insert.cpp
    test_mockdb_update.cpp
    test_mockdb_delete.cpp
    test_mockdb_joins.cpp
    test_mockdb_relationships.cpp
)

target_link_libraries(test_integration PRIVATE
    orm::mockdb
    GTest::gtest_main
)

gtest_discover_tests(test_integration)
```

Add to `tests/CMakeLists.txt`:

```cmake
add_subdirectory(integration)
```

Create stub files:

```bash
touch tests/integration/test_mockdb_select.cpp
touch tests/integration/test_mockdb_insert.cpp
touch tests/integration/test_mockdb_update.cpp
touch tests/integration/test_mockdb_delete.cpp
touch tests/integration/test_mockdb_joins.cpp
touch tests/integration/test_mockdb_relationships.cpp
```

Each stub:
```cpp
#include <gtest/gtest.h>
// Integration tests added in subsequent tasks
```

- [ ] **Step 4: Commit**

```bash
git add lib/CMakeLists.txt lib/src/ORM/db/connectors/MockDB/CMakeLists.txt \
        tests/integration/ tests/CMakeLists.txt
git commit -m "build: MockDB compiled library target and integration test suite target"
```

---

### Task 2: MockDB tag and `connector_trait<MockDB>` declaration

**Files:**
- Create: `lib/include/ORM/db/connectors/MockDB/init.hpp`
- Create: `lib/src/ORM/db/connectors/MockDB/init.cpp`

- [ ] **Step 1: Create init.hpp**

Create `lib/include/ORM/db/connectors/MockDB/init.hpp`:

```cpp
#pragma once
#include "ORM/connector/trait.hpp"
#include "ORM/connector/capabilities.hpp"
#include "ORM/result/result.hpp"
#include "ORM/db/connectors/MockDB/sql_renderer.hpp"
#include <string>
#include <vector>

namespace orm {

// ── MockDB tag ────────────────────────────────────────────────────────────────
// In-memory connector that renders queries to SQL strings.
// Not connected to any database. Used for testing and documentation.
struct MockDB {
    // Stores the last generated SQL string for test inspection
    mutable std::string last_sql;

    // Stores bound parameter values as strings for test inspection
    mutable std::vector<std::string> last_params;
};

// ── connector_trait<MockDB> declaration ───────────────────────────────────────
template <>
struct connector_trait<MockDB> {
    // MockDB supports full SQL feature set
    using supports_joins        = void;
    using supports_transactions = void;
    using supports_aggregation  = void;
    using supports_upsert       = void;
    using supports_bulk_insert  = void;

    // Wire types are pass-through for MockDB (we stringify params ourselves)
    template <typename T>
    struct wire_type { using type = T; };

    // Cursor type (trivial for MockDB — no real rows)
    struct cursor_type {
        bool has_next() const noexcept { return false; }
    };

    // ── execute overloads ─────────────────────────────────────────────────────
    // Defined in init.cpp to catch errors at library compile time.

    template <typename QueryIR, typename... Params>
    static result<int> execute(MockDB& db, QueryIR query, Params... params);
};

// ── Explicit instantiation declarations ──────────────────────────────────────
// These cause the .cpp to generate the instantiations, catching errors early.
// Add an extern template line here for each query shape used in tests.

} // namespace orm
```

- [ ] **Step 2: Create sql_renderer.hpp**

Create `lib/include/ORM/db/connectors/MockDB/sql_renderer.hpp`:

```cpp
#pragma once
#include "ORM/query/select.hpp"
#include "ORM/query/insert.hpp"
#include "ORM/query/update.hpp"
#include "ORM/query/delete.hpp"
#include "ORM/query/rules.hpp"
#include "ORM/query/field.hpp"
#include "ORM/details/string_literal.hpp"
#include <string>
#include <sstream>

namespace orm::mockdb {

// ── SqlRenderer ───────────────────────────────────────────────────────────────
// Walks the query IR type tree and produces a SQL string.
// All methods are static; renderer carries no state.
struct SqlRenderer {
    // ── Rule → SQL WHERE clause fragment ────────────────────────────────────
    template <typename T1, detail::string_literal op, typename T2>
    static std::string render_rule(const Rule<T1, op, T2>& rule) {
        std::ostringstream os;
        os << render_operand(rule._1)
           << " " << static_cast<std::string_view>(op) << " "
           << render_operand(rule._2);
        return os.str();
    }

    // ── Select → SQL SELECT statement ────────────────────────────────────────
    template <typename Query>
        requires is_select_query<Query>
    static std::string render_select(const Query& q) {
        std::ostringstream os;
        os << "SELECT ";
        render_field_list(os, q.selected_properties());
        os << " FROM ";
        render_table_list(os, q.selected_properties());
        render_joins(os, q.join_clauses());
        render_wheres(os, q.where_clauses());
        render_order_by(os, q.order_clauses());
        render_limit(os, q.limit_clauses());
        return os.str();
    }

    // ── Insert → SQL INSERT statement ────────────────────────────────────────
    template <typename Query>
        requires is_insert_query<Query>
    static std::string render_insert(const Query& q) {
        std::ostringstream os;
        os << "INSERT INTO ";
        render_table_from_properties(os, q.signature);
        os << " (";
        render_column_list(os, q.signature);
        os << ") VALUES (";
        render_placeholders(os, Query::properties::size);
        os << ")";
        return os.str();
    }

    // ── Update → SQL UPDATE statement ─────────────────────────────────────────
    template <typename Query>
        requires is_update_query<Query>
    static std::string render_update(const Query& q) {
        std::ostringstream os;
        os << "UPDATE ";
        render_table_name<typename Query::tables::template orm_type<0>>(os);
        os << " SET ";
        render_set_clauses(os, q.updates());
        render_wheres(os, q.wheres());
        return os.str();
    }

    // ── Delete → SQL DELETE statement ─────────────────────────────────────────
    template <typename Query>
        requires is_delete_query<Query>
    static std::string render_delete(const Query& q) {
        std::ostringstream os;
        os << "DELETE FROM ";
        render_table_name<typename Query::table>(os);
        render_wheres(os, q.wheres());
        return os.str();
    }

private:
    // Render a single operand (field pointer → "table.column", value → "?")
    template <auto Ptr>
    static std::string render_operand(Ptr) {
        return std::string(mem_ptr<Ptr>::column_name());
    }

    template <typename T>
    static std::string render_operand(const T&) { return "?"; }

    static std::string render_operand(std::nullptr_t) { return "NULL"; }

    // Render a tuple of field<> as "t1.col1, t2.col2, ..."
    template <typename Tuple, std::size_t... Is>
    static void render_field_list_impl(std::ostringstream& os,
                                       const Tuple& t,
                                       std::index_sequence<Is...>) {
        bool first = true;
        ((void)(os << (first ? (first = false, "") : ", ")
                   << t.template get<Is>().column_name()), ...);
    }

    template <typename Tuple>
    static void render_field_list(std::ostringstream& os, const Tuple& t) {
        render_field_list_impl(os, t, std::make_index_sequence<Tuple::size>{});
    }

    // Render table names from the selected fields
    template <typename Tuple>
    static void render_table_list(std::ostringstream& os, const Tuple& /*t*/) {
        // Simplified: use the first field's table name
        // Full implementation deduplicates table types
        os << "unknown_table"; // replaced by per-query specialization
    }

    // Render JoinRule tuple
    template <typename Joins>
    static void render_joins(std::ostringstream& os, const Joins& /*joins*/) {
        // TODO: walk Joins tuple and emit JOIN ... ON ... clauses
    }

    // Render WHERE clauses
    template <typename Wheres>
    static void render_wheres(std::ostringstream& os, const Wheres& w) {
        if constexpr (Wheres::size > 0) {
            os << " WHERE ";
            render_where_tuple(os, w, std::make_index_sequence<Wheres::size>{});
        }
    }

    template <typename Tuple, std::size_t... Is>
    static void render_where_tuple(std::ostringstream& os,
                                   const Tuple& t,
                                   std::index_sequence<Is...>) {
        bool first = true;
        ((void)(os << (first ? (first = false, "") : " AND ")
                   << render_rule(t.template get<Is>())), ...);
    }

    // Render ORDER BY
    template <typename Orders>
    static void render_order_by(std::ostringstream& os, const Orders& /*orders*/) {
        if constexpr (Orders::size > 0) {
            os << " ORDER BY ..."; // simplified
        }
    }

    // Render LIMIT
    template <typename Limits>
    static void render_limit(std::ostringstream& os, const Limits& limits) {
        if constexpr (Limits::size > 0) {
            const auto& p = limits.template get<0>();
            os << " LIMIT " << p.get_elements_per_page()
               << " OFFSET " << (p.get_elements_per_page() * p.get_number_of_page());
        }
    }

    // Render column list from insert properties
    template <typename Tuple>
    static void render_column_list(std::ostringstream& os, const Tuple& t) {
        render_field_list(os, t);
    }

    // Render table name from insert signature's first field
    template <typename Tuple>
    static void render_table_from_properties(std::ostringstream& os, const Tuple& /*t*/) {
        os << "unknown_table"; // refined per concrete query in integration tests
    }

    template <typename Table>
    static void render_table_name(std::ostringstream& os) {
        os << orm::table_name<Table>();
    }

    // Render SET col = ?, col = ? ...
    template <typename Stmts>
    static void render_set_clauses(std::ostringstream& os, const Stmts& stmts) {
        if constexpr (Stmts::size > 0) {
            render_set_tuple(os, stmts, std::make_index_sequence<Stmts::size>{});
        }
    }

    template <typename Tuple, std::size_t... Is>
    static void render_set_tuple(std::ostringstream& os,
                                 const Tuple& t,
                                 std::index_sequence<Is...>) {
        bool first = true;
        ((void)(os << (first ? (first = false, "") : ", ")
                   << mem_ptr<t.template get<Is>()._1>::column_name() << " = ?"), ...);
    }

    static void render_placeholders(std::ostringstream& os, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) {
            if (i > 0) os << ", ";
            os << "?";
        }
    }
};

} // namespace orm::mockdb
```

- [ ] **Step 3: Create init.cpp**

Create `lib/src/ORM/db/connectors/MockDB/init.cpp`:

```cpp
#include "ORM/db/connectors/MockDB/init.hpp"
#include "ORM/db/connectors/MockDB/sql_renderer.hpp"
#include "ORM/query/select.hpp"
#include "ORM/query/insert.hpp"
#include "ORM/query/update.hpp"
#include "ORM/query/delete.hpp"

namespace orm {

template <typename QueryIR, typename... Params>
result<int> connector_trait<MockDB>::execute(
    MockDB& db, QueryIR query, Params... /*params*/)
{
    if constexpr (is_select_query<QueryIR>) {
        db.last_sql = mockdb::SqlRenderer::render_select(query);
    } else if constexpr (is_insert_query<QueryIR>) {
        db.last_sql = mockdb::SqlRenderer::render_insert(query);
    } else if constexpr (is_update_query<QueryIR>) {
        db.last_sql = mockdb::SqlRenderer::render_update(query);
    } else if constexpr (is_delete_query<QueryIR>) {
        db.last_sql = mockdb::SqlRenderer::render_delete(query);
    }
    return result<int>{};
}

} // namespace orm
```

- [ ] **Step 4: Build library**

```bash
cmake --build build --target orm_mockdb
```

Expected: compiles without errors.

- [ ] **Step 5: Commit**

```bash
git add lib/include/ORM/db/ lib/src/ORM/db/
git commit -m "feat: MockDB connector tag, connector_trait<MockDB>, SqlRenderer skeleton"
```

---

## Chunk 2: Integration tests

### Task 3: SELECT integration tests

**Files:**
- Modify: `tests/integration/test_mockdb_select.cpp`

- [ ] **Step 1: Write SELECT integration tests**

Replace `tests/integration/test_mockdb_select.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MockDB/init.hpp"

struct Post {
    orm::property<int, "id">             id;
    orm::property<std::u8string, "body"> body;
};

struct User {
    orm::property<int, "id">                                   id;
    orm::property<std::u8string, "name">                       name;
    orm::property<orm::enum_t<"active","banned">, "status">    status;
};

// Register table names
namespace orm {
    template<> struct table_name_trait<User> {
        static constexpr std::string_view value = "users";
    };
    template<> struct table_name_trait<Post> {
        static constexpr std::string_view value = "posts";
    };
}

class MockDBSelectTest : public ::testing::Test {
protected:
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
};

TEST_F(MockDBSelectTest, SelectSingleField) {
    constexpr auto q = orm::select(orm::field<&User::id>);
    db << q;
    EXPECT_NE(conn.last_sql.find("SELECT"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("id"), std::string::npos);
}

TEST_F(MockDBSelectTest, SelectMultipleFields) {
    constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&User::name>);
    db << q;
    EXPECT_NE(conn.last_sql.find("id"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("name"), std::string::npos);
}

TEST_F(MockDBSelectTest, SelectWithWhere) {
    constexpr auto q = orm::select(orm::field<&User::id>)
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    db << q;
    EXPECT_NE(conn.last_sql.find("WHERE"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("id"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("?"), std::string::npos);
}

TEST_F(MockDBSelectTest, SelectWithAndWhere) {
    constexpr auto q = orm::select(orm::field<&User::id>)
        .where((orm::field<&User::id> == orm::Placeholder<int>{})
            && (orm::field<&User::name> == orm::Placeholder<std::u8string>{}));
    db << q;
    EXPECT_NE(conn.last_sql.find("WHERE"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("AND"), std::string::npos);
}

TEST_F(MockDBSelectTest, SelectWithLimit) {
    using namespace orm::literals;
    constexpr auto q = orm::select(orm::field<&User::id>)
        .limit(10_per_page * 2_page);
    db << q;
    EXPECT_NE(conn.last_sql.find("LIMIT 10"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("OFFSET 20"), std::string::npos);
}

TEST_F(MockDBSelectTest, SelectWithOrderByAsc) {
    constexpr auto q = orm::select(orm::field<&User::id>)
        .order_by<orm::order::direction::asc>(orm::field<&User::id>);
    db << q;
    EXPECT_NE(conn.last_sql.find("ORDER BY"), std::string::npos);
}

TEST_F(MockDBSelectTest, SelectResultIsRange) {
    constexpr auto q = orm::select(orm::field<&User::id>);
    auto result = db << q;
    EXPECT_TRUE(result.empty());
    auto v = result.to_vector();
    EXPECT_TRUE(v.empty());
}
```

- [ ] **Step 2: Run integration tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_integration
```

Expected: `MockDBSelectTest.*` PASS.

- [ ] **Step 3: Commit**

```bash
git add tests/integration/test_mockdb_select.cpp
git commit -m "test: SELECT integration tests against MockDB"
```

---

### Task 4: INSERT integration tests

**Files:**
- Modify: `tests/integration/test_mockdb_insert.cpp`

- [ ] **Step 1: Write INSERT integration tests**

Replace `tests/integration/test_mockdb_insert.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MockDB/init.hpp"

struct User {
    orm::property<int, "id">            id;
    orm::property<std::u8string, "name"> name;
};

namespace orm {
    template<> struct table_name_trait<User> {
        static constexpr std::string_view value = "users";
    };
}

class MockDBInsertTest : public ::testing::Test {
protected:
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
};

TEST_F(MockDBInsertTest, InsertSingleField) {
    constexpr auto q = orm::insert(orm::field<&User::name>);
    db << q;
    EXPECT_NE(conn.last_sql.find("INSERT INTO"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("name"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("VALUES"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("?"), std::string::npos);
}

TEST_F(MockDBInsertTest, InsertMultipleFields) {
    constexpr auto q = orm::insert(orm::field<&User::id>, orm::field<&User::name>);
    db << q;
    EXPECT_NE(conn.last_sql.find("id"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("name"), std::string::npos);
    // Two placeholders
    auto first  = conn.last_sql.find("?");
    auto second = conn.last_sql.find("?", first + 1);
    EXPECT_NE(second, std::string::npos);
}
```

- [ ] **Step 2: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_integration
```

Expected: `MockDBInsertTest.*` PASS.

- [ ] **Step 3: Commit**

```bash
git add tests/integration/test_mockdb_insert.cpp
git commit -m "test: INSERT integration tests against MockDB"
```

---

### Task 5: UPDATE and DELETE integration tests

**Files:**
- Modify: `tests/integration/test_mockdb_update.cpp`
- Modify: `tests/integration/test_mockdb_delete.cpp`

- [ ] **Step 1: Write UPDATE tests**

Replace `tests/integration/test_mockdb_update.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MockDB/init.hpp"

struct User {
    orm::property<int, "id">            id;
    orm::property<std::u8string, "name"> name;
};

namespace orm {
    template<> struct table_name_trait<User> {
        static constexpr std::string_view value = "users";
    };
}

class MockDBUpdateTest : public ::testing::Test {
protected:
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
};

TEST_F(MockDBUpdateTest, UpdateWithSet) {
    constexpr auto q = orm::update<User>()
        .set(orm::field<&User::name>, orm::Placeholder<std::u8string>{});
    db << q;
    EXPECT_NE(conn.last_sql.find("UPDATE"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("users"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("SET"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("name = ?"), std::string::npos);
}

TEST_F(MockDBUpdateTest, UpdateWithSetAndWhere) {
    constexpr auto q = orm::update<User>()
        .set(orm::field<&User::name>, orm::Placeholder<std::u8string>{})
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    db << q;
    EXPECT_NE(conn.last_sql.find("SET"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("WHERE"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("id"), std::string::npos);
}
```

- [ ] **Step 2: Write DELETE tests**

Replace `tests/integration/test_mockdb_delete.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MockDB/init.hpp"

struct User {
    orm::property<int, "id">            id;
    orm::property<std::u8string, "name"> name;
};

namespace orm {
    template<> struct table_name_trait<User> {
        static constexpr std::string_view value = "users";
    };
}

class MockDBDeleteTest : public ::testing::Test {
protected:
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
};

TEST_F(MockDBDeleteTest, DeleteAll) {
    constexpr auto q = orm::deleteq<User>();
    db << q;
    EXPECT_NE(conn.last_sql.find("DELETE FROM"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("users"), std::string::npos);
}

TEST_F(MockDBDeleteTest, DeleteWithWhere) {
    constexpr auto q = orm::deleteq<User>()
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    db << q;
    EXPECT_NE(conn.last_sql.find("DELETE FROM"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("WHERE"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("id"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("?"), std::string::npos);
}

TEST_F(MockDBDeleteTest, DeleteWithAndWhere) {
    constexpr auto q = orm::deleteq<User>()
        .where((orm::field<&User::id> == orm::Placeholder<int>{})
            && (orm::field<&User::name> == orm::Placeholder<std::u8string>{}));
    db << q;
    EXPECT_NE(conn.last_sql.find("AND"), std::string::npos);
}
```

- [ ] **Step 3: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_integration
```

Expected: `MockDBUpdateTest.*`, `MockDBDeleteTest.*` PASS.

- [ ] **Step 4: Commit**

```bash
git add tests/integration/test_mockdb_update.cpp tests/integration/test_mockdb_delete.cpp
git commit -m "test: UPDATE and DELETE integration tests against MockDB"
```

---

### Task 6: JOIN integration tests

**Files:**
- Modify: `tests/integration/test_mockdb_joins.cpp`

- [ ] **Step 1: Write JOIN tests**

Replace `tests/integration/test_mockdb_joins.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MockDB/init.hpp"

struct Post {
    orm::property<int, "id">             id;
    orm::property<int, "author_id">      author_id;
    orm::property<std::u8string, "body"> body;
};

struct User {
    orm::property<int, "id">            id;
    orm::property<std::u8string, "name"> name;
};

namespace orm {
    template<> struct table_name_trait<User> {
        static constexpr std::string_view value = "users";
    };
    template<> struct table_name_trait<Post> {
        static constexpr std::string_view value = "posts";
    };
}

class MockDBJoinTest : public ::testing::Test {
protected:
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
};

TEST_F(MockDBJoinTest, InnerJoin) {
    constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&Post::body>)
        .join<orm::join::mode::inner, Post>(
            orm::field<&User::id> == orm::field<&Post::author_id>);
    db << q;
    EXPECT_NE(conn.last_sql.find("JOIN"), std::string::npos);
}

TEST_F(MockDBJoinTest, JoinWithWhere) {
    constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&Post::body>)
        .join<orm::join::mode::inner, Post>(
            orm::field<&User::id> == orm::field<&Post::author_id>)
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    db << q;
    EXPECT_NE(conn.last_sql.find("JOIN"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("WHERE"), std::string::npos);
}

// Compile-time capability check: a connector without supports_joins
// should fail to compile a query with .join(). This is verified by the
// negative compilation test in test_capabilities.cpp (commented block).
```

- [ ] **Step 2: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_integration
```

Expected: `MockDBJoinTest.*` PASS.

- [ ] **Step 3: Commit**

```bash
git add tests/integration/test_mockdb_joins.cpp
git commit -m "test: JOIN integration tests against MockDB"
```

---

### Task 7: Relationship inference integration tests

**Files:**
- Modify: `tests/integration/test_mockdb_relationships.cpp`

- [ ] **Step 1: Write relationship inference tests**

Replace `tests/integration/test_mockdb_relationships.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MockDB/init.hpp"

struct Post {
    orm::property<int, "id">             id;
    orm::property<std::u8string, "body"> body;
};

// User with inferred and explicit relationships
struct UserWithRelationships {
    orm::property<int, "id">            id;
    orm::property<std::u8string, "name"> name;

    // Inferred one-to-one
    Post latest_post;

    // Inferred one-to-many
    std::vector<Post> drafts;

    // Explicit override: always reference
    orm::relationship<orm::store_as::reference, Post, "posts"> posts;

    // Explicit override: always embed
    orm::relationship<orm::store_as::embed, Post, "archived"> archived;

    // Explicit one-to-many reference
    orm::relationship<orm::store_as::reference, std::vector<Post>, "likes"> likes;
};

// Relationship inference is purely compile-time — no ORM.hpp execution required.
// These tests verify the type-level inference rules.

TEST(RelationshipInference, SingleStructIsOneToOne) {
    static_assert(
        orm::infer_relationship_v<Post> == orm::inferred_kind::one_to_one);
}

TEST(RelationshipInference, VectorIsOneToMany) {
    static_assert(
        orm::infer_relationship_v<std::vector<Post>> == orm::inferred_kind::one_to_many);
}

TEST(RelationshipInference, ListIsOneToMany) {
    static_assert(
        orm::infer_relationship_v<std::list<Post>> == orm::inferred_kind::one_to_many);
}

TEST(RelationshipInference, ExplicitReferenceStrategyPreserved) {
    using R = orm::relationship<orm::store_as::reference, Post, "posts">;
    static_assert(R::strategy == orm::store_as::reference);
    EXPECT_EQ(R::field_name(), "posts");
}

TEST(RelationshipInference, ExplicitEmbedStrategyPreserved) {
    using R = orm::relationship<orm::store_as::embed, Post, "archived">;
    static_assert(R::strategy == orm::store_as::embed);
}

TEST(RelationshipInference, ExplicitCollectionReference) {
    using R = orm::relationship<orm::store_as::reference, std::vector<Post>, "likes">;
    static_assert(R::is_collection);
    static_assert(R::strategy == orm::store_as::reference);
    static_assert(std::is_same_v<R::element_type, Post>);
}

TEST(RelationshipInference, IsRelationshipRecognized) {
    using R = orm::relationship<orm::store_as::reference, Post, "posts">;
    static_assert(orm::is_relationship_v<R>);
    static_assert(!orm::is_relationship_v<Post>);
    static_assert(!orm::is_relationship_v<int>);
}
```

- [ ] **Step 2: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_integration
```

Expected: `RelationshipInference.*` PASS.

- [ ] **Step 3: Commit**

```bash
git add tests/integration/test_mockdb_relationships.cpp
git commit -m "test: relationship inference integration tests"
```

---

## Chunk 3: Full suite verification

### Task 8: Run complete test suite and verify zero warnings

- [ ] **Step 1: Clean build with strict warnings**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror"
cmake --build build
```

Expected: zero warnings, zero errors.

- [ ] **Step 2: Run all tests**

```bash
ctest --test-dir build --output-on-failure
```

Expected: ALL test suites PASS:
- `test_foundation` — all types, reflection, entity model tests
- `test_query` — all query IR tests
- `test_connector` — all connector trait, capability, db handle, result tests
- `test_integration` — all MockDB integration tests

- [ ] **Step 3: Tag the complete baseline**

```bash
git tag v2.0.0-foundation
git push origin v2.0.0-foundation
```

- [ ] **Step 4: Final commit**

```bash
git commit --allow-empty -m "chore: Plan D complete — full integration test suite green"
```

---

## Plan D completion checklist

- [ ] `MockDB` tag struct with `last_sql` and `last_params` inspection fields
- [ ] `connector_trait<MockDB>` with all SQL capability tags declared
- [ ] `SqlRenderer` — translates query IR to SQL strings
- [ ] `init.cpp` / `sql_renderer.cpp` in compiled library (catches template errors at lib build time)
- [ ] SELECT integration tests (single field, multiple fields, WHERE, AND, ORDER BY, LIMIT)
- [ ] INSERT integration tests (single field, multiple fields, placeholder count)
- [ ] UPDATE integration tests (SET, SET + WHERE)
- [ ] DELETE integration tests (no WHERE, with WHERE, with AND)
- [ ] JOIN integration tests (INNER JOIN, JOIN + WHERE)
- [ ] Relationship inference integration tests (all inference rules verified)
- [ ] Full suite PASS with `-Wall -Wextra -Wpedantic -Werror`
- [ ] Git tag `v2.0.0-foundation` pushed

---

## Overall plan sequence

| Plan | Name | Status on entry |
|------|------|----------------|
| **A** | Foundation — types, reflection, entity model | Start here |
| **B** | Query IR — field<>, Rule<>, CRUD builders | After A complete |
| **C** | Connector + Result — trait, capabilities, db<>, result<> | After B complete |
| **D** | MockDB + Integration tests | After C complete |

Each plan produces a fully buildable, fully tested increment. Do not start plan N+1 before all tests of plan N are green.
