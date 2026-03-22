#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MockDB/mock_db.hpp"

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

namespace orm
{
    template <>
    struct table_name_trait<User>
    {
        static constexpr std::string_view value = "users";
    };

    template <>
    struct table_name_trait<Post>
    {
        static constexpr std::string_view value = "posts";
    };
} // namespace orm

class MockDBTest : public ::testing::Test
{
protected:
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
};

// ── SELECT ────────────────────────────────────────────────────────────────────

TEST_F(MockDBTest, SelectProducesSelectStatement)
{
    constexpr auto q = orm::select(orm::field<&User::id>);
    db << q;
    EXPECT_NE(conn.last_sql.find("SELECT"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("id"), std::string::npos);
}

TEST_F(MockDBTest, SelectMultipleFields)
{
    constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&User::name>);
    db << q;
    EXPECT_NE(conn.last_sql.find("id"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("name"), std::string::npos);
}

TEST_F(MockDBTest, SelectWithWhereClause)
{
    constexpr auto q = orm::select(orm::field<&User::id>)
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    db << q;
    EXPECT_NE(conn.last_sql.find("WHERE"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("?"), std::string::npos);
}

TEST_F(MockDBTest, SelectWithAndWhere)
{
    constexpr auto q = orm::select(orm::field<&User::id>)
        .where((orm::field<&User::id> == orm::Placeholder<int>{})
            && (orm::field<&User::name> == orm::Placeholder<std::u8string>{}));
    db << q;
    EXPECT_NE(conn.last_sql.find("WHERE"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("&&"), std::string::npos);
}

TEST_F(MockDBTest, SelectWithLimit)
{
    using namespace orm::literals;
    constexpr auto q = orm::select(orm::field<&User::id>)
        .limit(10_per_page * 2_page);
    db << q;
    EXPECT_NE(conn.last_sql.find("LIMIT 10"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("OFFSET 20"), std::string::npos);
}

TEST_F(MockDBTest, SelectWithOrderBy)
{
    constexpr auto q = orm::select(orm::field<&User::id>)
        .order_by<orm::order::direction::asc, &User::id>();
    db << q;
    EXPECT_NE(conn.last_sql.find("ORDER BY"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("id"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("ASC"), std::string::npos);
}

TEST_F(MockDBTest, SelectWithOrderByDesc)
{
    constexpr auto q = orm::select(orm::field<&User::score>)
        .order_by<orm::order::direction::desc, &User::score>();
    db << q;
    EXPECT_NE(conn.last_sql.find("ORDER BY"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("score"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("DESC"), std::string::npos);
}

TEST_F(MockDBTest, SelectOrderByMultipleColumns)
{
    constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&User::score>)
        .order_by<orm::order::direction::asc,  &User::id>()
        .order_by<orm::order::direction::desc, &User::score>();
    db << q;
    EXPECT_NE(conn.last_sql.find("id ASC"),    std::string::npos);
    EXPECT_NE(conn.last_sql.find("score DESC"), std::string::npos);
}

TEST_F(MockDBTest, SelectWithInnerJoin)
{
    constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&Post::body>)
        .join<orm::join::mode::inner, Post>(
            orm::field<&User::id> == orm::field<&Post::author_id>);
    db << q;
    EXPECT_NE(conn.last_sql.find("INNER JOIN"), std::string::npos);
}

TEST_F(MockDBTest, SelectResultIsRange)
{
    constexpr auto q = orm::select(orm::field<&User::id>);
    auto res = db << q;
    EXPECT_TRUE(res.empty());
    EXPECT_EQ(res.to_vector().size(), 0u);
}

TEST_F(MockDBTest, SelectResultRowTypeIsTuple)
{
    constexpr auto q = orm::select(orm::field<&User::id>);
    using ResType = decltype(db << q);
    static_assert(std::is_same_v<ResType::value_type, std::tuple<int>>);
}

TEST_F(MockDBTest, SelectMultiFieldRowType)
{
    constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&User::name>);
    using ResType = decltype(db << q);
    static_assert(std::is_same_v<ResType::value_type, std::tuple<int, std::u8string>>);
}

TEST_F(MockDBTest, SelectGetByMemberPointer)
{
    constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&User::name>);
    using ResType = decltype(db << q);
    using Row = ResType::value_type;
    Row row{42, u8"alice"};
    EXPECT_EQ((ResType::get_field<&User::id>(row)), 42);
    EXPECT_EQ((ResType::get_field<&User::name>(row)), u8"alice");
}

TEST_F(MockDBTest, SelectGetByIndex)
{
    constexpr auto q = orm::select(orm::field<&User::id>);
    using ResType = decltype(db << q);
    using Row = ResType::value_type;
    Row row{99};
    EXPECT_EQ((ResType::get<0>(row)), 99);
}

TEST_F(MockDBTest, InsertResultIsEmptyTuple)
{
    constexpr auto q = orm::insert(orm::field<&User::name>);
    using ResType = decltype(db << q);
    static_assert(std::is_same_v<ResType::value_type, std::tuple<>>);
}

TEST_F(MockDBTest, UpdateResultIsEmptyTuple)
{
    constexpr auto q = orm::update<User>()
        .set(orm::field<&User::name>, orm::Placeholder<std::u8string>{});
    using ResType = decltype(db << q);
    static_assert(std::is_same_v<ResType::value_type, std::tuple<>>);
}

TEST_F(MockDBTest, DeleteResultIsEmptyTuple)
{
    constexpr auto q = orm::deleteq<User>();
    using ResType = decltype(db << q);
    static_assert(std::is_same_v<ResType::value_type, std::tuple<>>);
}

// ── INSERT ────────────────────────────────────────────────────────────────────

TEST_F(MockDBTest, InsertProducesInsertStatement)
{
    constexpr auto q = orm::insert(orm::field<&User::name>);
    db << q;
    EXPECT_NE(conn.last_sql.find("INSERT INTO"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("name"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("VALUES"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("?"), std::string::npos);
}

TEST_F(MockDBTest, InsertMultipleFieldsMultiplePlaceholders)
{
    constexpr auto q = orm::insert(orm::field<&User::id>, orm::field<&User::name>);
    db << q;
    const auto first  = conn.last_sql.find('?');
    const auto second = conn.last_sql.find('?', first + 1);
    EXPECT_NE(second, std::string::npos);
}

// ── UPDATE ────────────────────────────────────────────────────────────────────

TEST_F(MockDBTest, UpdateProducesUpdateStatement)
{
    constexpr auto q = orm::update<User>()
        .set(orm::field<&User::name>, orm::Placeholder<std::u8string>{});
    db << q;
    EXPECT_NE(conn.last_sql.find("UPDATE"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("users"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("SET"), std::string::npos);
}

TEST_F(MockDBTest, UpdateWithWhereClause)
{
    constexpr auto q = orm::update<User>()
        .set(orm::field<&User::name>, orm::Placeholder<std::u8string>{})
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    db << q;
    EXPECT_NE(conn.last_sql.find("WHERE"), std::string::npos);
}

// ── DELETE ────────────────────────────────────────────────────────────────────

TEST_F(MockDBTest, DeleteProducesDeleteStatement)
{
    constexpr auto q = orm::deleteq<User>();
    db << q;
    EXPECT_NE(conn.last_sql.find("DELETE FROM"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("users"), std::string::npos);
}

TEST_F(MockDBTest, DeleteWithWhereClause)
{
    constexpr auto q = orm::deleteq<User>()
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    db << q;
    EXPECT_NE(conn.last_sql.find("DELETE FROM"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("WHERE"), std::string::npos);
}

TEST_F(MockDBTest, DeleteWithAndWhere)
{
    constexpr auto q = orm::deleteq<User>()
        .where((orm::field<&User::id> == orm::Placeholder<int>{})
            && (orm::field<&User::name> == orm::Placeholder<std::u8string>{}));
    db << q;
    EXPECT_NE(conn.last_sql.find("&&"), std::string::npos);
}

// ── Capability gating ─────────────────────────────────────────────────────────
// MockDB declares supports_joins, so this should compile fine.
TEST_F(MockDBTest, GroupByRendered)
{
    constexpr auto q = orm::select(orm::field<&User::id>)
        .template group_by<&User::id>();
    db << q;
    EXPECT_NE(conn.last_sql.find("GROUP BY"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("id"), std::string::npos);
}

TEST_F(MockDBTest, GroupByMultipleFields)
{
    constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&User::name>)
        .template group_by<&User::id>()
        .template group_by<&User::name>();
    db << q;
    EXPECT_NE(conn.last_sql.find("GROUP BY"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("id"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("name"), std::string::npos);
}

TEST_F(MockDBTest, FindOneEmptyResultReturnsNullopt)
{
    constexpr auto q = orm::select(orm::field<&User::id>);
    auto opt = db.find_one(q);
    EXPECT_FALSE(opt.has_value());
}

TEST_F(MockDBTest, ExecuteWithParamsStoresParams)
{
    constexpr auto q = orm::select(orm::field<&User::id>)
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    db.execute(q, 42);
    ASSERT_EQ(conn.last_params.size(), 1u);
    EXPECT_EQ(conn.last_params[0], "42");
}

TEST_F(MockDBTest, ExecuteWithMultipleParamsStoresAll)
{
    constexpr auto q = orm::select(orm::field<&User::id>)
        .where((orm::field<&User::id> == orm::Placeholder<int>{})
            && (orm::field<&User::name> == orm::Placeholder<std::u8string>{}));
    db.execute(q, 7, u8"alice");
    ASSERT_EQ(conn.last_params.size(), 2u);
    EXPECT_EQ(conn.last_params[0], "7");
    EXPECT_EQ(conn.last_params[1], "alice");
}

TEST_F(MockDBTest, ExecuteNoParamsClearsParamList)
{
    constexpr auto q = orm::select(orm::field<&User::id>);
    conn.last_params = {"stale"};
    db << q;
    EXPECT_TRUE(conn.last_params.empty());
}

TEST_F(MockDBTest, JoinAllowedOnMockDB)
{
    static_assert(orm::has_capability<orm::MockDB, orm::cap::supports_joins>);
}

TEST_F(MockDBTest, TransactionsAllowedOnMockDB)
{
    static_assert(orm::has_capability<orm::MockDB, orm::cap::supports_transactions>);
}

TEST_F(MockDBTest, InsertStoresParamsInOrder)
{
    constexpr auto q = orm::insert(orm::field<&User::id>, orm::field<&User::name>);
    db.execute(q, 99, u8"zara");
    ASSERT_EQ(conn.last_params.size(), 2u);
    EXPECT_EQ(conn.last_params[0], "99");
    EXPECT_EQ(conn.last_params[1], "zara");
}

TEST_F(MockDBTest, InsertSqlContainsInsertKeyword)
{
    constexpr auto q = orm::insert(orm::field<&User::id>, orm::field<&User::name>);
    db.execute(q, 1, u8"bob");
    EXPECT_NE(conn.last_sql.find("INSERT"), std::string::npos);
}

TEST_F(MockDBTest, UpdateStoresSetAndWhereParams)
{
    constexpr auto q = orm::update<User>()
        .set(orm::field<&User::name>, orm::Placeholder<std::u8string>{})
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    db.execute(q, u8"carol", 5);
    ASSERT_EQ(conn.last_params.size(), 2u);
    EXPECT_EQ(conn.last_params[0], "carol");
    EXPECT_EQ(conn.last_params[1], "5");
}

TEST_F(MockDBTest, UpdateSqlContainsUpdateKeyword)
{
    constexpr auto q = orm::update<User>()
        .set(orm::field<&User::name>, orm::Placeholder<std::u8string>{})
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    db.execute(q, u8"dan", 3);
    EXPECT_NE(conn.last_sql.find("UPDATE"), std::string::npos);
}

TEST_F(MockDBTest, DeleteStoresWhereParams)
{
    constexpr auto q = orm::deleteq<User>()
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    db.execute(q, 7);
    ASSERT_EQ(conn.last_params.size(), 1u);
    EXPECT_EQ(conn.last_params[0], "7");
}

TEST_F(MockDBTest, DeleteSqlContainsDeleteKeyword)
{
    constexpr auto q = orm::deleteq<User>();
    db.execute(q);
    EXPECT_NE(conn.last_sql.find("DELETE"), std::string::npos);
}

TEST_F(MockDBTest, IndexedPlaceholderRendersQuestionMarkN)
{
    using namespace std::placeholders;
    constexpr auto q = orm::select(orm::field<&User::id>)
        .where(orm::field<&User::id> == orm::ph<int, _1>);
    db.execute(q, 42);
    EXPECT_NE(conn.last_sql.find("?1"), std::string::npos);
    ASSERT_EQ(conn.last_params.size(), 1u);
    EXPECT_EQ(conn.last_params[0], "42");
}

TEST_F(MockDBTest, IndexedPlaceholderReusedSameArgInSQL)
{
    using namespace std::placeholders;
    constexpr auto q = orm::select(orm::field<&User::id>)
        .where((orm::field<&User::id>   == orm::ph<int, _1>)
            && (orm::field<&User::score> > orm::ph<double, _1>));
    db.execute(q, 7, 3.5);
    // SQL should contain two ?1 occurrences (same argument index reused)
    const auto& sql = conn.last_sql;
    const auto pos1 = sql.find("?1");
    ASSERT_NE(pos1, std::string::npos);
    EXPECT_NE(sql.find("?1", pos1 + 1), std::string::npos);
    ASSERT_EQ(conn.last_params.size(), 2u);
    EXPECT_EQ(conn.last_params[0], "7");
}

TEST_F(MockDBTest, PrepareReturnsExecutableObject)
{
    const auto pq = db.prepare(orm::select(orm::field<&User::id>));
    auto res = pq.execute();
    EXPECT_TRUE(conn.last_sql.find("SELECT") != std::string::npos);
}

TEST_F(MockDBTest, PrepareWithParamExecutesCorrectly)
{
    using namespace std::placeholders;
    const auto pq = db.prepare(
        orm::select(orm::field<&User::id>)
            .where(orm::field<&User::id> == orm::ph<int, _1>));
    pq.execute(42);
    ASSERT_EQ(conn.last_params.size(), 1u);
    EXPECT_EQ(conn.last_params[0], "42");
}

TEST_F(MockDBTest, PrepareCanBeCalledMultipleTimesWithDifferentParams)
{
    using namespace std::placeholders;
    const auto pq = db.prepare(
        orm::select(orm::field<&User::id>)
            .where(orm::field<&User::id> == orm::ph<int, _1>));
    pq.execute(1);
    EXPECT_EQ(conn.last_params[0], "1");
    pq.execute(99);
    EXPECT_EQ(conn.last_params[0], "99");
    pq.execute(7);
    EXPECT_EQ(conn.last_params[0], "7");
}

TEST_F(MockDBTest, PrepareExposesQueryIR)
{
    constexpr auto q = orm::select(orm::field<&User::id>);
    const auto pq = db.prepare(q);
    using ExpectedQuery = decltype(q);
    static_assert(std::is_same_v<decltype(pq.query()), const ExpectedQuery&>);
}

TEST_F(MockDBTest, TwoDistinctIndexedPlaceholdersRenderCorrectSlots)
{
    using namespace std::placeholders;
    constexpr auto q = orm::select(orm::field<&User::id>)
        .where((orm::field<&User::id>   == orm::ph<int, _1>)
            && (orm::field<&User::name>  == orm::ph<std::u8string, _2>));
    db.execute(q, 5, u8"alice");
    EXPECT_NE(conn.last_sql.find("?1"), std::string::npos);
    EXPECT_NE(conn.last_sql.find("?2"), std::string::npos);
    ASSERT_EQ(conn.last_params.size(), 2u);
    EXPECT_EQ(conn.last_params[0], "5");
    EXPECT_EQ(conn.last_params[1], "alice");
}
