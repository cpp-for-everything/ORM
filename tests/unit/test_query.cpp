#include <gtest/gtest.h>
#include "ORM/ORM.hpp"

struct Post
{
    orm::property<int, "id">             id;
    orm::property<int, "author_id">      author_id;
    orm::property<std::u8string, "body"> body;
};

struct User
{
    orm::property<int, "id">            id;
    orm::property<std::u8string, "name"> name;
    orm::property<double, "score">      score;
};

// ── field<> ───────────────────────────────────────────────────────────────────

TEST(Field, ColumnNameFromProperty)
{
    constexpr auto f = orm::field<&User::id>;
    EXPECT_EQ(decltype(f)::column_name(), "id");
}

TEST(Field, TableType)
{
    static_assert(std::is_same_v<decltype(orm::field<&User::id>)::table_type, User>);
}

TEST(Field, IsFieldConcept)
{
    static_assert(orm::is_field<decltype(orm::field<&User::id>)>);
    static_assert(!orm::is_field<int>);
}

TEST(Field, SameMemberSameType)
{
    static_assert(std::is_same_v<
        decltype(orm::field<&User::id>),
        decltype(orm::field<&User::id>)>);
}

TEST(Field, DifferentMembersDifferentTypes)
{
    static_assert(!std::is_same_v<
        decltype(orm::field<&User::id>),
        decltype(orm::field<&User::name>)>);
}

// ── Placeholder ───────────────────────────────────────────────────────────────

TEST(Placeholder, IsPlaceholderConcept)
{
    static_assert(orm::is_placeholder<orm::Placeholder<int>>);
    static_assert(!orm::is_placeholder<int>);
}

TEST(Placeholder, DifferentTypesDistinct)
{
    static_assert(!std::is_same_v<orm::Placeholder<int>, orm::Placeholder<double>>);
}

// ── Rule ─────────────────────────────────────────────────────────────────────

TEST(Rule, EqualityFieldVsPlaceholder)
{
    constexpr auto rule = orm::field<&User::id> == orm::Placeholder<int>{};
    static_assert(orm::is_rule<decltype(rule)>);
    EXPECT_EQ(decltype(rule)::operation, "==");
}

TEST(Rule, GreaterThanField)
{
    constexpr auto rule = orm::field<&User::score> > 3.14;
    static_assert(orm::is_rule<decltype(rule)>);
    EXPECT_EQ(decltype(rule)::operation, ">");
}

TEST(Rule, LogicalAnd)
{
    constexpr auto r1 = orm::field<&User::id> == orm::Placeholder<int>{};
    constexpr auto r2 = orm::field<&User::score> > 0.0;
    constexpr auto combined = r1 && r2;
    static_assert(orm::is_rule<decltype(combined)>);
    EXPECT_EQ(decltype(combined)::operation, "&&");
}

TEST(Rule, Negation)
{
    constexpr auto rule = orm::field<&User::id> == orm::Placeholder<int>{};
    constexpr auto neg  = !rule;
    EXPECT_EQ(decltype(neg)::operation, "!=");
}

TEST(Rule, NullComparison)
{
    constexpr auto rule = orm::field<&User::id> == nullptr;
    static_assert(orm::is_rule<decltype(rule)>);
    EXPECT_EQ(decltype(rule)::operation, "==");
}

// ── Pagification / UDLs ───────────────────────────────────────────────────────

TEST(Pagification, UdlConstruction)
{
    using namespace orm::literals;
    constexpr auto p = 10_per_page * 2_page;
    static_assert(p.get_elements_per_page() == 10);
    static_assert(p.get_number_of_page() == 2);
}

// ── select_query ──────────────────────────────────────────────────────────────

TEST(SelectQuery, IsSelectQuery)
{
    constexpr auto q = orm::select(orm::field<&User::id>);
    static_assert(orm::is_select_query<decltype(q)>);
}

TEST(SelectQuery, ResponseSize)
{
    constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&User::name>);
    static_assert(decltype(q)::response_type::size == 2);
}

TEST(SelectQuery, WithWhereAddsClause)
{
    constexpr auto q = orm::select(orm::field<&User::id>)
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    static_assert(decltype(q.where_clauses())::size == 1);
}

TEST(SelectQuery, WithJoinAddsClause)
{
    constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&Post::body>)
        .join<orm::join::mode::inner, Post>(
            orm::field<&User::id> == orm::field<&Post::author_id>);
    static_assert(decltype(q.join_clauses())::size == 1);
}

TEST(SelectQuery, WithOrderByAddsClause)
{
    constexpr auto q = orm::select(orm::field<&User::id>)
        .order_by<orm::order::direction::asc, &User::id>();
    static_assert(decltype(q.order_clauses())::size == 1);
}

TEST(SelectQuery, WithLimitAddsClause)
{
    using namespace orm::literals;
    constexpr auto q = orm::select(orm::field<&User::id>)
        .limit(10_per_page * 1_page);
    static_assert(decltype(q.limit_clauses())::size == 1);
}

// ── insert_query ──────────────────────────────────────────────────────────────

TEST(InsertQuery, IsInsertQuery)
{
    constexpr auto q = orm::insert(orm::field<&User::id>);
    static_assert(orm::is_insert_query<decltype(q)>);
}

TEST(InsertQuery, PropertiesSize)
{
    constexpr auto q = orm::insert(orm::field<&User::name>, orm::field<&User::score>);
    static_assert(decltype(q)::properties::size == 2);
}

// ── update_query ──────────────────────────────────────────────────────────────

TEST(UpdateQuery, IsUpdateQuery)
{
    constexpr auto q = orm::update<User>();
    static_assert(orm::is_update_query<decltype(q)>);
}

TEST(UpdateQuery, WithSetAddsStatement)
{
    constexpr auto q = orm::update<User>()
        .set(orm::field<&User::name>, orm::Placeholder<std::u8string>{});
    static_assert(decltype(q.updates())::size == 1);
}

TEST(UpdateQuery, WithWhereAddsClause)
{
    constexpr auto q = orm::update<User>()
        .set(orm::field<&User::name>, orm::Placeholder<std::u8string>{})
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    static_assert(decltype(q.wheres())::size == 1);
}

// ── delete_query ──────────────────────────────────────────────────────────────

TEST(DeleteQuery, IsDeleteQuery)
{
    constexpr auto q = orm::deleteq<User>();
    static_assert(orm::is_delete_query<decltype(q)>);
}

TEST(DeleteQuery, TableType)
{
    using Q = decltype(orm::deleteq<User>());
    static_assert(std::is_same_v<Q::table, User>);
}

TEST(DeleteQuery, WithWhereAddsClause)
{
    constexpr auto q = orm::deleteq<User>()
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    static_assert(decltype(q.wheres())::size == 1);
}

// ── result ────────────────────────────────────────────────────────────────────

using IntRow = std::tuple<int>;
using StrRow = std::tuple<std::u8string>;

TEST(Result, DefaultEmpty)
{
    orm::result<IntRow> r;
    EXPECT_TRUE(r.empty());
}

TEST(Result, IterableFromVector)
{
    orm::result<IntRow> r(std::vector<IntRow>{{1}, {2}, {3}});
    int sum = 0;
    for (const auto& row : r) sum += std::get<0>(row);
    EXPECT_EQ(sum, 6);
}

TEST(Result, ToVectorRoundtrip)
{
    std::vector<IntRow> data{{10}, {20}};
    orm::result<IntRow> r(data);
    EXPECT_EQ(r.to_vector(), data);
}

TEST(Result, GetByIndex)
{
    IntRow row{42};
    EXPECT_EQ((orm::result<IntRow>::get<0>(row)), 42);
}

TEST(Result, GetByMemberPointer)
{
    using Row = std::tuple<int, std::u8string>;
    using Fields = orm::detail::orm_tuple<
        decltype(orm::field<&User::id>),
        decltype(orm::field<&User::name>)>;
    orm::result<Row, Fields> res;
    Row row{7, u8"alice"};
    EXPECT_EQ((orm::result<Row, Fields>::get_field<&User::id>(row)), 7);
    EXPECT_EQ((orm::result<Row, Fields>::get_field<&User::name>(row)), u8"alice");
}

TEST(Result, SelectQueryHasRowType)
{
    constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&User::name>);
    using Q = decltype(q);
    static_assert(std::is_same_v<Q::row_type, std::tuple<int, std::u8string>>);
}

TEST(OptionalResult, EmptyHasNoValue)
{
    orm::optional_result<int> r;
    EXPECT_FALSE(r.has_value());
}

TEST(OptionalResult, WithValueAccessible)
{
    orm::optional_result<int> r(42);
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(*r, 42);
}
