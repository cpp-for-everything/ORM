# ORM v2 — Plan B: Query IR (field<>, Rule<>, Placeholder, CRUD Builders)

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the complete compile-time query intermediate representation: `field<>` wrapper, `Rule<>` expression tree, `Placeholder<T>`, and all four CRUD fluent builders (`select`, `insert`, `update`, `deleteq`).

**Architecture:** Entirely type-level — all query structure lives in template parameters. Every query object is `constexpr`-constructible. The query IR is SQL-superset shaped; NoSQL connectors translate it. No database I/O in this plan.

**Tech Stack:** C++20; GoogleTest; depends on Plan A (foundation types must be installed).

**Spec:** `docs/superpowers/specs/2026-03-28-orm-v2-architecture.md` §5

**Depends on:** Plan A complete  
**Required by:** Plan C (Connector + Result), Plan D (MockDB)

---

## Chunk 1: `field<>` and `Rule<>` expression system

### File map

| Action | Path | Responsibility |
|--------|------|---------------|
| Create | `lib/include/ORM/query/field.hpp` | `field<&T::m>` mem_ptr wrapper; C++26 `^^` path note |
| Create | `lib/include/ORM/query/rules.hpp` | `IRule`, `Rule<T1,op,T2>`, all operator overloads |
| Create | `lib/include/ORM/query/placeholders.hpp` | `Placeholder<T>`, `is_placeholder` concept |
| Create | `lib/include/ORM/details/string_literal.hpp` | `string_literal<N>` NTTP helper |
| Create | `lib/include/ORM/details/orm_tuple.hpp` | Extended tuple utilities |
| Create | `lib/include/ORM/details/member_pointer.hpp` | `mem_ptr`, `i_mem_ptr`, `is_mem_ptr` concepts |
| Create | `tests/query/CMakeLists.txt` | Query IR test suite |
| Create | `tests/query/test_field.cpp` | field<> and Rule<> tests |
| Create | `tests/query/test_placeholder.cpp` | Placeholder tests |

---

### Task 1: `string_literal` NTTP helper

**Files:**
- Create: `lib/include/ORM/details/string_literal.hpp`

- [ ] **Step 1: Create string_literal**

```cpp
// lib/include/ORM/details/string_literal.hpp
#pragma once
#include <algorithm>
#include <string_view>
#include <cstddef>

namespace orm::detail {

template <std::size_t N>
struct string_literal {
    char data[N]{};

    constexpr string_literal(const char (&s)[N]) {
        std::copy_n(s, N, data);
    }

    constexpr operator std::string_view() const noexcept {
        return {data, N - 1};
    }

    constexpr bool operator==(const string_literal&) const noexcept = default;

    template <std::size_t M>
    constexpr bool operator==(const string_literal<M>&) const noexcept {
        return false;
    }
};

} // namespace orm::detail
```

- [ ] **Step 2: Commit**

```bash
git add lib/include/ORM/details/string_literal.hpp
git commit -m "feat: string_literal<N> NTTP helper for compile-time operator strings"
```

---

### Task 2: `orm_tuple` extended tuple

**Files:**
- Create: `lib/include/ORM/details/orm_tuple.hpp`
- Create: `tests/query/test_orm_tuple.cpp`

- [ ] **Step 1: Write failing tests**

Create `tests/query/test_orm_tuple.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/details/orm_tuple.hpp"

TEST(OrmTuple, GetByIndex) {
    orm::detail::orm_tuple<int, double, float> t(1, 2.0, 3.0f);
    EXPECT_EQ(t.get<0>(), 1);
    EXPECT_DOUBLE_EQ(t.get<1>(), 2.0);
    EXPECT_FLOAT_EQ(t.get<2>(), 3.0f);
}

TEST(OrmTuple, SizeConstant) {
    static_assert(orm::detail::orm_tuple<int, double>::size == 2);
    static_assert(orm::detail::orm_tuple<>::size == 0);
}

TEST(OrmTuple, TypeAccess) {
    static_assert(std::is_same_v<
        orm::detail::orm_tuple<int, double>::orm_type<0>, int>);
    static_assert(std::is_same_v<
        orm::detail::orm_tuple<int, double>::orm_type<1>, double>);
}

TEST(OrmTuple, ConcatTuples) {
    orm::detail::orm_tuple<int> a(1);
    orm::detail::orm_tuple<double> b(2.0);
    auto c = orm::detail::tuple_cat(a, b);
    static_assert(decltype(c)::size == 2);
    EXPECT_EQ(c.get<0>(), 1);
}
```

- [ ] **Step 2: Confirm compile error**

```bash
cmake --build build 2>&1 | head -20
```

- [ ] **Step 3: Implement orm_tuple**

Create `lib/include/ORM/details/orm_tuple.hpp`:

```cpp
#pragma once
#include <tuple>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace orm::detail {

template <typename... Ts>
struct orm_tuple {
    static constexpr std::size_t size = sizeof...(Ts);

    template <std::size_t I>
    using orm_type = std::tuple_element_t<I, std::tuple<Ts...>>;

    std::tuple<Ts...> _storage;

    constexpr orm_tuple() = default;
    explicit constexpr orm_tuple(Ts... vals) : _storage(std::move(vals)...) {}
    explicit constexpr orm_tuple(std::tuple<Ts...> t) : _storage(std::move(t)) {}

    template <std::size_t I>
    constexpr decltype(auto) get() & { return std::get<I>(_storage); }

    template <std::size_t I>
    constexpr decltype(auto) get() const& { return std::get<I>(_storage); }

    constexpr auto to_tuple() const& { return _storage; }
};

// ── Deduction guide ───────────────────────────────────────────────────────────
template <typename... Ts>
orm_tuple(Ts...) -> orm_tuple<Ts...>;

// ── tuple_cat for orm_tuple ───────────────────────────────────────────────────
template <typename... As, typename... Bs>
constexpr auto tuple_cat(const orm_tuple<As...>& a, const orm_tuple<Bs...>& b) {
    return orm_tuple<As..., Bs...>(
        orm_tuple<As..., Bs...>(std::tuple_cat(a._storage, b._storage)));
}

// ── append_type: orm_tuple<Ts...> + T -> orm_tuple<Ts..., T> ─────────────────
template <typename Tuple, typename T>
struct append_type;

template <typename... Ts, typename T>
struct append_type<orm_tuple<Ts...>, T> {
    using type = orm_tuple<Ts..., T>;
};

template <typename Tuple, typename T>
using append_type_t = typename append_type<Tuple, T>::type;

} // namespace orm::detail
```

- [ ] **Step 4: Add query test CMakeLists and run**

Create `tests/query/CMakeLists.txt`:

```cmake
add_executable(test_query
    test_orm_tuple.cpp
    test_field.cpp
    test_placeholder.cpp
    test_select.cpp
    test_insert.cpp
    test_update.cpp
    test_delete.cpp
)

target_link_libraries(test_query PRIVATE
    orm::orm
    GTest::gtest_main
)

gtest_discover_tests(test_query)
```

Add to `tests/CMakeLists.txt`:

```cmake
add_subdirectory(query)
```

Create empty stub files for tests not yet written:

```bash
touch tests/query/test_field.cpp tests/query/test_placeholder.cpp
touch tests/query/test_select.cpp tests/query/test_insert.cpp
touch tests/query/test_update.cpp tests/query/test_delete.cpp
```

Each stub contains:
```cpp
#include <gtest/gtest.h>
// Tests added in subsequent tasks
```

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_query
```

Expected: `OrmTuple.*` PASS.

- [ ] **Step 5: Commit**

```bash
git add lib/include/ORM/details/orm_tuple.hpp tests/query/
git commit -m "feat: orm_tuple<> extended tuple utility with size, type access, concat"
```

---

### Task 3: `mem_ptr` and `field<>` wrapper

**Files:**
- Create: `lib/include/ORM/details/member_pointer.hpp`
- Create: `lib/include/ORM/query/field.hpp`
- Modify: `tests/query/test_field.cpp`

- [ ] **Step 1: Write failing tests**

Replace contents of `tests/query/test_field.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/query/field.hpp"
#include "ORM/entity/property.hpp"

struct User {
    orm::property<int, "id">           id;
    orm::property<std::u8string, "name"> name;
    orm::property<double, "score">     score;
};

// field<&T::m> wraps a member pointer
TEST(Field, WrapsCorrectMemberPointer) {
    constexpr auto f = orm::field<&User::id>;
    static_assert(std::is_same_v<
        typename decltype(f)::table_type, User>);
    static_assert(std::is_same_v<
        typename decltype(f)::value_type, orm::property<int, "id">>);
}

// field<> column name matches property column_name
TEST(Field, ColumnName) {
    constexpr auto f = orm::field<&User::id>;
    EXPECT_EQ(decltype(f)::column_name(), "id");
}

// Two field<> of same member are same type
TEST(Field, SameMemberSameType) {
    static_assert(std::is_same_v<
        decltype(orm::field<&User::id>),
        decltype(orm::field<&User::id>)>);
}

// Different members are different types
TEST(Field, DifferentMembersDifferentTypes) {
    static_assert(!std::is_same_v<
        decltype(orm::field<&User::id>),
        decltype(orm::field<&User::name>)>);
}

// is_field concept
TEST(Field, IsFieldConcept) {
    static_assert(orm::is_field<decltype(orm::field<&User::id>)>);
    static_assert(!orm::is_field<int>);
}
```

- [ ] **Step 2: Confirm compile error**

```bash
cmake --build build 2>&1 | head -20
```

- [ ] **Step 3: Implement member_pointer.hpp**

Create `lib/include/ORM/details/member_pointer.hpp`:

```cpp
#pragma once
#include <type_traits>

namespace orm::detail {

// ── i_mem_ptr: extract Table and value type from a member pointer ─────────────
template <auto MemberPtr>
struct i_mem_ptr;

template <typename T, typename Class>
struct i_mem_ptr_base {
    using table_type = Class;
    using value_type = T;
    using ptr_t = T Class::*;
};

template <typename T, typename Class, T Class::* Ptr>
struct i_mem_ptr<Ptr> : i_mem_ptr_base<T, Class> {};

// ── is_mem_ptr concept ────────────────────────────────────────────────────────
template <typename T>
concept is_mem_ptr_t = requires {
    typename T::table_type;
    typename T::value_type;
    typename T::ptr_t;
};

} // namespace orm::detail
```

- [ ] **Step 4: Implement field.hpp**

Create `lib/include/ORM/query/field.hpp`:

```cpp
#pragma once
#include "ORM/details/member_pointer.hpp"
#include "ORM/entity/property.hpp"
#include <string_view>
#include <type_traits>

namespace orm {

// ── mem_ptr<Ptr>: zero-overhead wrapper enabling operator overloads ────────────
template <auto Ptr>
struct mem_ptr {
    using table_type = typename detail::i_mem_ptr<Ptr>::table_type;
    using value_type = typename detail::i_mem_ptr<Ptr>::value_type;
    using ptr_t      = typename detail::i_mem_ptr<Ptr>::ptr_t;

    static constexpr ptr_t get() noexcept { return Ptr; }

    static constexpr std::string_view column_name() noexcept {
        if constexpr (is_property_v<value_type>)
            return value_type::column_name();
        else
            return "";
    }
};

// ── field<&T::m> factory variable template ───────────────────────────────────
template <auto Ptr>
inline constexpr mem_ptr<Ptr> field{};

// ── is_field concept ──────────────────────────────────────────────────────────
template <typename T>
concept is_field = detail::is_mem_ptr_t<T>;

} // namespace orm
```

- [ ] **Step 5: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_query
```

Expected: `Field.*` PASS.

- [ ] **Step 6: Commit**

```bash
git add lib/include/ORM/details/member_pointer.hpp lib/include/ORM/query/field.hpp \
        tests/query/test_field.cpp
git commit -m "feat: orm::field<&T::m> mem_ptr wrapper with table/value type extraction"
```

---

### Task 4: `Placeholder<T>` and `IRule` / `Rule<>` expression tree

**Files:**
- Create: `lib/include/ORM/query/placeholders.hpp`
- Create: `lib/include/ORM/query/rules.hpp`
- Modify: `tests/query/test_placeholder.cpp`
- Modify: `tests/query/test_field.cpp` (append Rule tests)

- [ ] **Step 1: Write Placeholder tests**

Replace `tests/query/test_placeholder.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/query/placeholders.hpp"

TEST(Placeholder, IsPlaceholderConcept) {
    static_assert(orm::is_placeholder<orm::Placeholder<int>>);
    static_assert(!orm::is_placeholder<int>);
}

TEST(Placeholder, DifferentTypesAreDifferent) {
    static_assert(!std::is_same_v<
        orm::Placeholder<int>, orm::Placeholder<double>>);
}

TEST(Placeholder, NullptrPlaceholder) {
    static_assert(!orm::is_placeholder<std::nullptr_t>);
}
```

- [ ] **Step 2: Append Rule tests to test_field.cpp**

Append to `tests/query/test_field.cpp`:

```cpp
#include "ORM/query/rules.hpp"
#include "ORM/query/placeholders.hpp"

TEST(Rule, EqualityBetweenFields) {
    constexpr auto rule = orm::field<&User::id> == orm::field<&User::score>;
    static_assert(orm::is_rule<decltype(rule)>);
    EXPECT_EQ(std::string_view(decltype(rule)::operation), "==");
}

TEST(Rule, FieldVsPlaceholder) {
    constexpr auto rule = orm::field<&User::id> == orm::Placeholder<int>{};
    static_assert(orm::is_rule<decltype(rule)>);
}

TEST(Rule, GreaterThan) {
    constexpr auto rule = orm::field<&User::score> > 3.14;
    static_assert(orm::is_rule<decltype(rule)>);
    EXPECT_EQ(std::string_view(decltype(rule)::operation), ">");
}

TEST(Rule, LogicalAnd) {
    constexpr auto r1 = orm::field<&User::id> == orm::Placeholder<int>{};
    constexpr auto r2 = orm::field<&User::score> > 0.0;
    constexpr auto combined = r1 && r2;
    static_assert(orm::is_rule<decltype(combined)>);
    EXPECT_EQ(std::string_view(decltype(combined)::operation), "&&");
}

TEST(Rule, Negation) {
    constexpr auto rule = orm::field<&User::id> == orm::Placeholder<int>{};
    constexpr auto neg  = !rule;
    static_assert(orm::is_rule<decltype(neg)>);
    EXPECT_EQ(std::string_view(decltype(neg)::operation), "!=");
}

TEST(Rule, NullComparison) {
    constexpr auto rule = orm::field<&User::id> == nullptr;
    static_assert(orm::is_rule<decltype(rule)>);
}
```

- [ ] **Step 3: Implement Placeholder**

Create `lib/include/ORM/query/placeholders.hpp`:

```cpp
#pragma once
#include <type_traits>

namespace orm {

template <typename T>
struct Placeholder {
    using value_type = T;
};

template <typename T>
struct is_placeholder_trait : std::false_type {};

template <typename T>
struct is_placeholder_trait<Placeholder<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_placeholder_v = is_placeholder_trait<T>::value;

template <typename T>
concept is_placeholder = is_placeholder_v<T>;

} // namespace orm
```

- [ ] **Step 4: Implement Rule<>**

Create `lib/include/ORM/query/rules.hpp`:

```cpp
#pragma once
#include "ORM/details/string_literal.hpp"
#include "ORM/query/field.hpp"
#include "ORM/query/placeholders.hpp"
#include <type_traits>
#include <string_view>

namespace orm {

// ── IRule base (for derived-from checks) ─────────────────────────────────────
struct IRule {};

// ── Forward declaration ───────────────────────────────────────────────────────
template <typename T1, detail::string_literal op, typename T2>
struct Rule;

// ── is_rule concept ───────────────────────────────────────────────────────────
template <typename T>
concept is_rule = std::derived_from<T, IRule>;

// ── Primary Rule<T1, op, T2> specialization ──────────────────────────────────
template <typename T1, detail::string_literal op, typename T2>
struct Rule : IRule {
    static constexpr std::string_view operation{op};
    T1 _1;
    T2 _2;

    constexpr Rule() = default;
    constexpr Rule(T1 a, T2 b) : _1(std::move(a)), _2(std::move(b)) {}
};

// ── not_op: compile-time logical negation of an operator string ───────────────
template <detail::string_literal op>
consteval auto not_op() {
    using sv = std::string_view;
    constexpr sv s = op;
    if constexpr (s == "==")  return detail::string_literal("!=");
    if constexpr (s == "!=")  return detail::string_literal("==");
    if constexpr (s == ">")   return detail::string_literal("<=");
    if constexpr (s == "<")   return detail::string_literal(">=");
    if constexpr (s == ">=")  return detail::string_literal("<");
    if constexpr (s == "<=")  return detail::string_literal(">");
    if constexpr (s == "&&")  return detail::string_literal("||");
    if constexpr (s == "||")  return detail::string_literal("&&");
}

// ── operator! ─────────────────────────────────────────────────────────────────
template <typename T1, detail::string_literal op, typename T2>
constexpr auto operator!(const Rule<T1, op, T2>& r) {
    return Rule<T1, not_op<op>(), T2>(r._1, r._2);
}

// ── Operator overloads for mem_ptr ────────────────────────────────────────────
#define ORM_FIELD_OP(sym, str)                                              \
    template <auto P, typename T>                                           \
        requires(!std::is_same_v<T, std::nullptr_t>)                       \
    constexpr auto operator sym(mem_ptr<P> /*a*/, T x) {                   \
        if constexpr (is_field<T>)                                          \
            return Rule<typename mem_ptr<P>::ptr_t, str,                    \
                        typename T::ptr_t>(P, T::get());                   \
        else                                                                \
            return Rule<typename mem_ptr<P>::ptr_t, str, T>(P, x);         \
    }                                                                       \
    template <auto P, typename T>                                           \
        requires(!is_field<T> && !std::is_same_v<T, std::nullptr_t>)       \
    constexpr auto operator sym(T x, mem_ptr<P> /*a*/) {                   \
        return Rule<T, str, typename mem_ptr<P>::ptr_t>(x, P);             \
    }

ORM_FIELD_OP(==, "==")
ORM_FIELD_OP(!=, "!=")
ORM_FIELD_OP(< , "<" )
ORM_FIELD_OP(> , ">" )
ORM_FIELD_OP(<=, "<=")
ORM_FIELD_OP(>=, ">=")
#undef ORM_FIELD_OP

// nullptr overloads
template <auto P>
constexpr auto operator==(mem_ptr<P>, std::nullptr_t) {
    return Rule<typename mem_ptr<P>::ptr_t, "==", std::nullptr_t>(P, nullptr);
}
template <auto P>
constexpr auto operator!=(mem_ptr<P>, std::nullptr_t) {
    return Rule<typename mem_ptr<P>::ptr_t, "!=", std::nullptr_t>(P, nullptr);
}

// ── Rule && Rule, Rule || Rule ────────────────────────────────────────────────
template <typename T1, detail::string_literal op1, typename T2,
          typename U1, detail::string_literal op2, typename U2>
constexpr auto operator&&(Rule<T1, op1, T2> a, Rule<U1, op2, U2> b) {
    return Rule<Rule<T1, op1, T2>, "&&", Rule<U1, op2, U2>>(a, b);
}

template <typename T1, detail::string_literal op1, typename T2,
          typename U1, detail::string_literal op2, typename U2>
constexpr auto operator||(Rule<T1, op1, T2> a, Rule<U1, op2, U2> b) {
    return Rule<Rule<T1, op1, T2>, "||", Rule<U1, op2, U2>>(a, b);
}

} // namespace orm
```

- [ ] **Step 5: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_query
```

Expected: `Placeholder.*`, `Rule.*` PASS. All previous tests still PASS.

- [ ] **Step 6: Commit**

```bash
git add lib/include/ORM/query/ lib/include/ORM/details/member_pointer.hpp \
        tests/query/test_placeholder.cpp tests/query/test_field.cpp
git commit -m "feat: Placeholder<T>, Rule<T1,op,T2> expression tree, field operator overloads"
```

---

## Chunk 2: CRUD query builders

### File map

| Action | Path | Responsibility |
|--------|------|---------------|
| Create | `lib/include/ORM/query/select.hpp` | `select_query<...>`, `.join()`, `.where()`, `.order_by()`, `.limit()`, `.group_by()` |
| Create | `lib/include/ORM/query/insert.hpp` | `insert_query<Properties...>` |
| Create | `lib/include/ORM/query/update.hpp` | `update_query<...>`, `.where()`, `.order_by()`, `.limit()` |
| Create | `lib/include/ORM/query/delete.hpp` | `delete_query<Table, ...>`, `.where()`, `.order_by()`, `.limit()` |
| Create | `lib/include/ORM/query/limits.hpp` | `Pagification`, `_per_page`, `_page` UDLs |
| Create | `lib/include/ORM/query/join_rule.hpp` | `JoinRule<mode,Table,Rule>`, `GroupByRule`, `OrderBy` |
| Create | `lib/include/ORM/query/utils.hpp` | `is_select_query`, `is_insert_query`, type extraction utilities |

---

### Task 5: Pagination limits

**Files:**
- Create: `lib/include/ORM/query/limits.hpp`

- [ ] **Step 1: Create limits.hpp**

```cpp
// lib/include/ORM/query/limits.hpp
#pragma once
#include <cstddef>

namespace orm {

struct Pagification {
    std::size_t elements_per_page{0};
    std::size_t page_number{0};

    constexpr std::size_t get_elements_per_page() const noexcept { return elements_per_page; }
    constexpr std::size_t get_number_of_page()    const noexcept { return page_number; }
};

namespace literals {
    struct per_page_helper { std::size_t n; };
    struct page_helper     { std::size_t n; };

    constexpr per_page_helper operator""_per_page(unsigned long long n) { return {n}; }
    constexpr page_helper     operator""_page(unsigned long long n)     { return {n}; }

    constexpr Pagification operator*(per_page_helper p, page_helper g) {
        return {p.n, g.n};
    }
} // namespace literals

} // namespace orm
```

- [ ] **Step 2: Commit**

```bash
git add lib/include/ORM/query/limits.hpp
git commit -m "feat: Pagification and _per_page/_page UDLs"
```

---

### Task 6: JoinRule, GroupByRule, OrderBy

**Files:**
- Create: `lib/include/ORM/query/join_rule.hpp`

- [ ] **Step 1: Create join_rule.hpp**

```cpp
// lib/include/ORM/query/join_rule.hpp
#pragma once
#include "ORM/query/rules.hpp"
#include "ORM/details/orm_tuple.hpp"

namespace orm {

namespace join { enum class mode { inner, left, right, full }; }
namespace order { enum class direction { asc, desc }; }

// ── JoinRule<mode, Table, Rule> ───────────────────────────────────────────────
template <join::mode Mode, typename Table, typename RuleType>
struct JoinRule {
    static constexpr join::mode mode = Mode;
    using table_type = Table;
    RuleType _rule;

    constexpr explicit JoinRule(RuleType r) : _rule(std::move(r)) {}
    constexpr const RuleType& to_rule() const noexcept { return _rule; }
};

template <typename T>
struct is_join_rule_trait : std::false_type {};
template <join::mode M, typename T, typename R>
struct is_join_rule_trait<JoinRule<M, T, R>> : std::true_type {};
template <typename T>
inline constexpr bool is_join_rule_v = is_join_rule_trait<T>::value;

// ── OrderBy<direction, MemberPtr> ────────────────────────────────────────────
template <order::direction Dir, auto MemberPtr>
struct OrderBy {
    static constexpr order::direction sort = Dir;
    static constexpr auto property = MemberPtr;
};

// ── GroupByRule<Properties, optional Rule> ────────────────────────────────────
template <typename PropertiesTuple, typename RuleType = void>
struct GroupByRule {
    using Properties = PropertiesTuple;
    PropertiesTuple properties;
    RuleType _rule;

    static constexpr bool has_rule() { return !std::is_void_v<RuleType>; }
    constexpr const RuleType& to_rule() const noexcept { return _rule; }
};

template <typename PropertiesTuple>
struct GroupByRule<PropertiesTuple, void> {
    using Properties = PropertiesTuple;
    PropertiesTuple properties;

    static constexpr bool has_rule() { return false; }
};

} // namespace orm
```

- [ ] **Step 2: Commit**

```bash
git add lib/include/ORM/query/join_rule.hpp
git commit -m "feat: JoinRule, OrderBy, GroupByRule for query IR"
```

---

### Task 7: `select` builder

**Files:**
- Create: `lib/include/ORM/query/select.hpp`
- Modify: `tests/query/test_select.cpp`

- [ ] **Step 1: Write failing tests**

Replace `tests/query/test_select.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/query/select.hpp"
#include "ORM/entity/property.hpp"
#include "ORM/query/field.hpp"
#include "ORM/query/placeholders.hpp"

struct Post {
    orm::property<int, "id">            id;
    orm::property<std::u8string, "body"> body;
};
struct User {
    orm::property<int, "id">            id;
    orm::property<std::u8string, "name"> name;
};

TEST(Select, BasicConstruction) {
    constexpr auto q = orm::select(orm::field<&User::id>, orm::field<User::name>);
    static_assert(orm::is_select_query<decltype(q)>);
}

TEST(Select, ResponseSizeMatchesSelectedFields) {
    constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&User::name>);
    static_assert(decltype(q)::Response::size == 2);
}

TEST(Select, WithWhere) {
    constexpr auto q = orm::select(orm::field<&User::id>)
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    static_assert(decltype(q)::Wheres::size == 1);
}

TEST(Select, WithJoin) {
    constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&Post::body>)
        .join<orm::join::mode::inner, Post>(
            orm::field<&User::id> == orm::field<&Post::id>);
    static_assert(decltype(q)::Joins::size == 1);
}

TEST(Select, WithOrderBy) {
    constexpr auto q = orm::select(orm::field<&User::id>)
        .order_by<orm::order::direction::asc>(orm::field<&User::id>);
    static_assert(decltype(q)::OrderBy::size == 1);
}

TEST(Select, WithLimit) {
    using namespace orm::literals;
    constexpr auto q = orm::select(orm::field<&User::id>)
        .limit(10_per_page * 1_page);
    static_assert(decltype(q)::Limitations::size == 1);
}

TEST(Select, TablesExtractedFromFields) {
    constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&Post::body>);
    static_assert(decltype(q)::tables::size == 2);
}
```

- [ ] **Step 2: Confirm compile error**

```bash
cmake --build build 2>&1 | head -20
```

- [ ] **Step 3: Implement select.hpp**

Create `lib/include/ORM/query/select.hpp`:

```cpp
#pragma once
#include "ORM/query/field.hpp"
#include "ORM/query/rules.hpp"
#include "ORM/query/join_rule.hpp"
#include "ORM/query/limits.hpp"
#include "ORM/details/orm_tuple.hpp"
#include <type_traits>

namespace orm {

namespace modes::select { enum class mode { normal, all, distinct, distinctrow }; }

// Tag for is_select_query check
struct select_query_tag {};

template <typename T>
concept is_select_query = std::derived_from<T, select_query_tag>;

// ── select_query<Response, Joins, Wheres, Limitations, Groupings, OrderBy> ────
template <
    typename Response,
    typename Joins         = detail::orm_tuple<>,
    typename Wheres        = detail::orm_tuple<>,
    typename Limitations   = detail::orm_tuple<>,
    typename Groupings     = detail::orm_tuple<>,
    typename OrderByTuple  = detail::orm_tuple<>,
    modes::select::mode Mode = modes::select::mode::normal
>
struct select_query : select_query_tag {
    static constexpr modes::select::mode mode = Mode;

    Response    _response;
    Joins       _joins;
    Wheres      _wheres;
    Limitations _limits;
    Groupings   _groups;
    OrderByTuple _orders;

    // Tables are extracted from the Response fields
    // (first occurrence of each table type)
    using tables = Response; // simplified — full deduplication added in Plan D

    // .where(rule)
    template <typename RuleType>
        requires is_rule<RuleType>
    constexpr auto where(RuleType r) const {
        using NewWheres = detail::append_type_t<Wheres, RuleType>;
        return select_query<Response, Joins, NewWheres, Limitations, Groupings, OrderByTuple, Mode>{
            _response, _joins,
            detail::tuple_cat(_wheres, detail::orm_tuple<RuleType>(r)),
            _limits, _groups, _orders};
    }

    // .join<mode, Table>(rule)
    template <join::mode JMode, typename Table, typename RuleType>
        requires is_rule<RuleType>
    constexpr auto join(RuleType r) const {
        using JR = JoinRule<JMode, Table, RuleType>;
        using NewJoins = detail::append_type_t<Joins, JR>;
        return select_query<Response, NewJoins, Wheres, Limitations, Groupings, OrderByTuple, Mode>{
            _response,
            detail::tuple_cat(_joins, detail::orm_tuple<JR>(JR{r})),
            _wheres, _limits, _groups, _orders};
    }

    // .order_by<dir, MemberPtr>()
    template <order::direction Dir, auto Ptr>
    constexpr auto order_by() const {
        using OB = OrderBy<Dir, Ptr>;
        using NewOrders = detail::append_type_t<OrderByTuple, OB>;
        return select_query<Response, Joins, Wheres, Limitations, Groupings, NewOrders, Mode>{
            _response, _joins, _wheres, _limits, _groups,
            detail::tuple_cat(_orders, detail::orm_tuple<OB>(OB{}))};
    }

    // .limit(Pagification)
    constexpr auto limit(Pagification p) const {
        using NewLimits = detail::append_type_t<Limitations, Pagification>;
        return select_query<Response, Joins, Wheres, NewLimits, Groupings, OrderByTuple, Mode>{
            _response, _joins, _wheres,
            detail::tuple_cat(_limits, detail::orm_tuple<Pagification>(p)),
            _groups, _orders};
    }

    constexpr Response    selected_properties() const { return _response; }
    constexpr Joins       join_clauses()        const { return _joins; }
    constexpr Wheres      where_clauses()       const { return _wheres; }
    constexpr Limitations limit_clauses()       const { return _limits; }
    constexpr Groupings   group_clauses()       const { return _groups; }
    constexpr OrderByTuple order_clauses()      const { return _orders; }
};

// ── select(...) factory ────────────────────────────────────────────────────────
template <typename... Fields>
    requires (is_field<Fields> && ...)
constexpr auto select(Fields... fields) {
    using Response = detail::orm_tuple<Fields...>;
    return select_query<Response>{Response{fields...}, {}, {}, {}, {}, {}};
}

} // namespace orm
```

- [ ] **Step 4: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_query
```

Expected: `Select.*` PASS.

- [ ] **Step 5: Commit**

```bash
git add lib/include/ORM/query/select.hpp tests/query/test_select.cpp
git commit -m "feat: select_query<> fluent builder with join/where/order_by/limit"
```

---

### Task 8: `insert`, `update`, `deleteq` builders

**Files:**
- Create: `lib/include/ORM/query/insert.hpp`
- Create: `lib/include/ORM/query/update.hpp`
- Create: `lib/include/ORM/query/delete.hpp`
- Modify: `tests/query/test_insert.cpp`, `test_update.cpp`, `test_delete.cpp`

- [ ] **Step 1: Write failing tests**

Replace `tests/query/test_insert.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/query/insert.hpp"
#include "ORM/entity/property.hpp"
#include "ORM/query/field.hpp"

struct User {
    orm::property<int, "id">            id;
    orm::property<std::u8string, "name"> name;
};

TEST(Insert, BasicConstruction) {
    constexpr auto q = orm::insert(orm::field<&User::id>, orm::field<&User::name>);
    static_assert(orm::is_insert_query<decltype(q)>);
}

TEST(Insert, PropertiesSize) {
    constexpr auto q = orm::insert(orm::field<&User::name>);
    static_assert(decltype(q)::properties::size == 1);
}

TEST(Insert, TableExtracted) {
    constexpr auto q = orm::insert(orm::field<&User::id>, orm::field<&User::name>);
    static_assert(decltype(q)::tables::size == 1);
}
```

Replace `tests/query/test_update.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/query/update.hpp"
#include "ORM/query/field.hpp"
#include "ORM/query/placeholders.hpp"

struct User {
    orm::property<int, "id">            id;
    orm::property<std::u8string, "name"> name;
};

TEST(Update, BasicConstruction) {
    constexpr auto q = orm::update<User>();
    static_assert(orm::is_update_query<decltype(q)>);
}

TEST(Update, WithSetClause) {
    constexpr auto q = orm::update<User>()
        .set(orm::field<&User::name>, orm::Placeholder<std::u8string>{});
    static_assert(decltype(q)::size == 1);
}

TEST(Update, WithWhere) {
    constexpr auto q = orm::update<User>()
        .set(orm::field<&User::name>, orm::Placeholder<std::u8string>{})
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    static_assert(decltype(q)::where_size == 1);
}
```

Replace `tests/query/test_delete.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/query/delete.hpp"
#include "ORM/query/field.hpp"
#include "ORM/query/placeholders.hpp"

struct User {
    orm::property<int, "id">            id;
    orm::property<std::u8string, "name"> name;
};

TEST(Delete, BasicConstruction) {
    constexpr auto q = orm::deleteq<User>();
    static_assert(orm::is_delete_query<decltype(q)>);
}

TEST(Delete, WithWhere) {
    constexpr auto q = orm::deleteq<User>()
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    static_assert(decltype(q)::where_size == 1);
}

TEST(Delete, TableType) {
    constexpr auto q = orm::deleteq<User>();
    static_assert(std::is_same_v<typename decltype(q)::table, User>);
}
```

- [ ] **Step 2: Implement insert.hpp**

Create `lib/include/ORM/query/insert.hpp`:

```cpp
#pragma once
#include "ORM/query/field.hpp"
#include "ORM/details/orm_tuple.hpp"

namespace orm {

struct insert_query_tag {};
template <typename T>
concept is_insert_query = std::derived_from<T, insert_query_tag>;

template <typename Properties>
struct insert_query : insert_query_tag {
    using properties = Properties;
    // tables: extract unique table types from properties (simplified)
    struct tables { static constexpr std::size_t size = 1; };

    Properties signature;
    constexpr explicit insert_query(Properties p) : signature(std::move(p)) {}
};

template <typename... Fields>
    requires (is_field<Fields> && ...)
constexpr auto insert(Fields... fields) {
    using Properties = detail::orm_tuple<Fields...>;
    return insert_query<Properties>{Properties{fields...}};
}

} // namespace orm
```

- [ ] **Step 3: Implement update.hpp**

Create `lib/include/ORM/query/update.hpp`:

```cpp
#pragma once
#include "ORM/query/field.hpp"
#include "ORM/query/rules.hpp"
#include "ORM/query/placeholders.hpp"
#include "ORM/details/orm_tuple.hpp"

namespace orm {

struct update_query_tag {};
template <typename T>
concept is_update_query = std::derived_from<T, update_query_tag>;

// Statement: field = value/placeholder pair
template <typename FieldT, typename ValueT>
struct UpdateStatement {
    FieldT  _1;
    ValueT  _2;
};

template <
    typename Table,
    typename Statements  = detail::orm_tuple<>,
    typename Wheres      = detail::orm_tuple<>,
    typename OrderByT    = detail::orm_tuple<>,
    typename Limits      = detail::orm_tuple<>
>
struct update_query : update_query_tag {
    static constexpr std::size_t size       = Statements::size;
    static constexpr std::size_t where_size = Wheres::size;
    static constexpr std::size_t order_size = OrderByT::size;
    static constexpr std::size_t limit_size = Limits::size;
    using tables = detail::orm_tuple<Table>;

    Statements _updates;
    Wheres     _wheres;
    OrderByT   _orders;
    Limits     _limits;

    template <auto FieldPtr, typename ValueT>
    constexpr auto set(mem_ptr<FieldPtr>, ValueT v) const {
        using Stmt = UpdateStatement<decltype(FieldPtr), ValueT>;
        using NewStmts = detail::append_type_t<Statements, Stmt>;
        return update_query<Table, NewStmts, Wheres, OrderByT, Limits>{
            detail::tuple_cat(_updates, detail::orm_tuple<Stmt>(Stmt{FieldPtr, v})),
            _wheres, _orders, _limits};
    }

    template <typename RuleType>
        requires is_rule<RuleType>
    constexpr auto where(RuleType r) const {
        using NewWheres = detail::append_type_t<Wheres, RuleType>;
        return update_query<Table, Statements, NewWheres, OrderByT, Limits>{
            _updates,
            detail::tuple_cat(_wheres, detail::orm_tuple<RuleType>(r)),
            _orders, _limits};
    }

    constexpr Statements updates()       const { return _updates; }
    constexpr Wheres     wheres()        const { return _wheres; }
    constexpr OrderByT   order_clauses() const { return _orders; }
    constexpr Limits     limit_clauses() const { return _limits; }
};

template <typename Table>
constexpr auto update() {
    return update_query<Table>{};
}

} // namespace orm
```

- [ ] **Step 4: Implement delete.hpp**

Create `lib/include/ORM/query/delete.hpp`:

```cpp
#pragma once
#include "ORM/query/rules.hpp"
#include "ORM/query/join_rule.hpp"
#include "ORM/query/limits.hpp"
#include "ORM/details/orm_tuple.hpp"

namespace orm {

struct delete_query_tag {};
template <typename T>
concept is_delete_query = std::derived_from<T, delete_query_tag>;

template <
    typename Table,
    typename Wheres     = detail::orm_tuple<>,
    typename OrderByT   = detail::orm_tuple<>,
    typename Limits     = detail::orm_tuple<>
>
struct delete_query : delete_query_tag {
    using table = Table;
    static constexpr std::size_t where_size = Wheres::size;
    static constexpr std::size_t order_size = OrderByT::size;
    static constexpr std::size_t limit_size = Limits::size;

    Wheres   _wheres;
    OrderByT _orders;
    Limits   _limits;

    template <typename RuleType>
        requires is_rule<RuleType>
    constexpr auto where(RuleType r) const {
        using NewWheres = detail::append_type_t<Wheres, RuleType>;
        return delete_query<Table, NewWheres, OrderByT, Limits>{
            detail::tuple_cat(_wheres, detail::orm_tuple<RuleType>(r)),
            _orders, _limits};
    }

    constexpr Wheres   wheres()        const { return _wheres; }
    constexpr OrderByT order_clauses() const { return _orders; }
    constexpr Limits   limit_clauses() const { return _limits; }
};

template <typename Table>
constexpr auto deleteq() {
    return delete_query<Table>{};
}

} // namespace orm
```

- [ ] **Step 5: Run all query tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_query
```

Expected: all `Select.*`, `Insert.*`, `Update.*`, `Delete.*` PASS.

- [ ] **Step 6: Commit**

```bash
git add lib/include/ORM/query/ tests/query/
git commit -m "feat: insert_query, update_query, delete_query fluent builders"
```

---

### Task 9: Update ORM.hpp and run full suite

- [ ] **Step 1: Update ORM.hpp**

Add to `lib/include/ORM/ORM.hpp`:

```cpp
#include "ORM/details/string_literal.hpp"
#include "ORM/details/orm_tuple.hpp"
#include "ORM/details/member_pointer.hpp"
#include "ORM/query/placeholders.hpp"
#include "ORM/query/field.hpp"
#include "ORM/query/rules.hpp"
#include "ORM/query/limits.hpp"
#include "ORM/query/join_rule.hpp"
#include "ORM/query/select.hpp"
#include "ORM/query/insert.hpp"
#include "ORM/query/update.hpp"
#include "ORM/query/delete.hpp"
```

- [ ] **Step 2: Build and run full suite**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: ALL tests PASS, zero failures, zero warnings at `-Wall -Wextra`.

- [ ] **Step 3: Final commit**

```bash
git add lib/include/ORM/ORM.hpp
git commit -m "feat: complete Plan B — full compile-time query IR"
```

---

## Plan B completion checklist

- [ ] `detail::string_literal<N>` NTTP helper
- [ ] `detail::orm_tuple<Ts...>` with size, type access, concat
- [ ] `detail::mem_ptr<Ptr>` / `i_mem_ptr<Ptr>` member pointer introspection
- [ ] `orm::field<&T::m>` variable template wrapper
- [ ] `orm::Placeholder<T>` and `is_placeholder` concept
- [ ] `orm::IRule` / `orm::Rule<T1,op,T2>` with all operator overloads and `!` negation
- [ ] `orm::JoinRule<mode,Table,Rule>`, `orm::OrderBy<dir,Ptr>`, `orm::GroupByRule<>`
- [ ] `orm::Pagification` and `_per_page`/`_page` UDLs
- [ ] `orm::select(fields...).where().join().order_by().limit()`
- [ ] `orm::insert(fields...)`
- [ ] `orm::update<T>().set().where()`
- [ ] `orm::deleteq<T>().where()`
- [ ] All tests pass, zero warnings

**Next:** Plan C — Connector trait, capability gating, `orm::db<>`, `orm::result<>`
