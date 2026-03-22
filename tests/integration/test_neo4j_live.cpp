#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/Neo4jDB/neo4j_live.hpp"
#include <cstdlib>
#include <string>

// ── Test entity ───────────────────────────────────────────────────────────────

struct Person
{
    orm::property<std::string, "name"> name;
    orm::property<int, "age">          age;
};

namespace orm
{
    template <>
    struct table_name_trait<Person>
    {
        static constexpr std::string_view value = "Person";
    };
} // namespace orm

// ── Connection helper ─────────────────────────────────────────────────────────

[[nodiscard]] static orm::Neo4jLiveDB make_connection()
{
    const char* host     = std::getenv("ORM_NEO4J_HOST");
    const char* port_str = std::getenv("ORM_NEO4J_PORT");
    const char* user     = std::getenv("ORM_NEO4J_USER");
    const char* password = std::getenv("ORM_NEO4J_PASSWORD");

    const std::string url = std::string("bolt://")
        + (host ? host : "127.0.0.1")
        + ":"
        + (port_str ? port_str : "7687");

    return orm::Neo4jLiveDB::connect(
        url.c_str(),
        user     ? user     : "neo4j",
        password ? password : "orm_test_password");
}

// ── Fixture ───────────────────────────────────────────────────────────────────

class Neo4jLiveTest : public ::testing::Test
{
protected:
    orm::Neo4jLiveDB conn;
    orm::db<orm::Neo4jLiveDB> dbc{conn};

    void SetUp() override
    {
        conn = make_connection();
        dbc.rebind(conn);

        // Clear all Person nodes before each test
        neo4j_result_stream_t* stream = neo4j_run(
            conn.conn_,
            "MATCH (n:Person) DETACH DELETE n",
            neo4j_map(nullptr, 0));
        if (stream) neo4j_close_results(stream);
    }

    void TearDown() override
    {
        neo4j_result_stream_t* stream = neo4j_run(
            conn.conn_,
            "MATCH (n:Person) DETACH DELETE n",
            neo4j_map(nullptr, 0));
        if (stream) neo4j_close_results(stream);
    }
};

// ── Tests ──────────────────────────────────────────────────────────────────────

TEST_F(Neo4jLiveTest, ConnectionIsOpen)
{
    EXPECT_TRUE(conn.is_open());
}

TEST_F(Neo4jLiveTest, InsertAndSelectReturnsNode)
{
    constexpr auto ins = orm::insert(
        orm::field<&Person::name>,
        orm::field<&Person::age>);
    dbc.execute(ins, std::string("Alice"), 30);

    constexpr auto sel = orm::select(
        orm::field<&Person::name>,
        orm::field<&Person::age>);
    const auto rows = (dbc << sel).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), "Alice");
    EXPECT_EQ(std::get<1>(rows[0]), 30);
}

TEST_F(Neo4jLiveTest, InsertMultipleNodes)
{
    constexpr auto ins = orm::insert(
        orm::field<&Person::name>,
        orm::field<&Person::age>);
    dbc.execute(ins, std::string("Alice"), 30);
    dbc.execute(ins, std::string("Bob"),   25);

    constexpr auto sel = orm::select(orm::field<&Person::name>);
    const auto rows = (dbc << sel).to_vector();
    EXPECT_EQ(rows.size(), 2u);
}

TEST_F(Neo4jLiveTest, SelectWithWhereFilterByAge)
{
    constexpr auto ins = orm::insert(
        orm::field<&Person::name>,
        orm::field<&Person::age>);
    dbc.execute(ins, std::string("Alice"), 30);
    dbc.execute(ins, std::string("Bob"),   20);

    constexpr auto sel = orm::select(orm::field<&Person::name>)
        .where(orm::field<&Person::age> > orm::Placeholder<int>{});
    const auto rows = dbc.execute(sel, 25).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), "Alice");
}

TEST_F(Neo4jLiveTest, DeleteRemovesNodes)
{
    constexpr auto ins = orm::insert(
        orm::field<&Person::name>,
        orm::field<&Person::age>);
    dbc.execute(ins, std::string("Charlie"), 40);
    dbc.execute(ins, std::string("Diana"),   35);

    constexpr auto del = orm::deleteq<Person>()
        .where(orm::field<&Person::name> == orm::Placeholder<std::string>{});
    dbc.execute(del, std::string("Charlie"));

    constexpr auto sel = orm::select(orm::field<&Person::name>);
    const auto rows = (dbc << sel).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), "Diana");
}

TEST_F(Neo4jLiveTest, SelectEmptyWhenNoNodesExist)
{
    constexpr auto sel = orm::select(orm::field<&Person::name>);
    const auto rows = (dbc << sel).to_vector();
    EXPECT_TRUE(rows.empty());
}
