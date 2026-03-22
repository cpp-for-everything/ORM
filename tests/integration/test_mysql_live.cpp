#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MySQLDB/mysql_live.hpp"
#include <cstdlib>
#include <string>

// ── Test entity ───────────────────────────────────────────────────────────────

struct Product
{
    orm::property<int, "id">           id;
    orm::property<std::string, "name"> name;
    orm::property<double, "price">     price;
};

namespace orm
{
    template <>
    struct table_name_trait<Product>
    {
        static constexpr std::string_view value = "products";
    };
} // namespace orm

// ── Connection helper ─────────────────────────────────────────────────────────

[[nodiscard]] static orm::MySQLLiveDB make_connection()
{
    const char* host     = std::getenv("ORM_MYSQL_HOST");
    const char* port_str = std::getenv("ORM_MYSQL_PORT");
    const char* user     = std::getenv("ORM_MYSQL_USER");
    const char* password = std::getenv("ORM_MYSQL_PASSWORD");
    const char* db       = std::getenv("ORM_MYSQL_DATABASE");

    // MySQL C API uses the classic protocol on port 3306 by default.
    // The docker-compose environment exposes MySQL on port 3306.
    return orm::MySQLLiveDB::connect(
        host     ? host     : "127.0.0.1",
        static_cast<unsigned int>(port_str ? std::stoul(port_str) : 3306u),
        user     ? user     : "root",
        password ? password : "orm_test_password",
        db       ? db       : "orm_test");
}

// ── Fixture ───────────────────────────────────────────────────────────────────

class MySQLLiveTest : public ::testing::Test
{
protected:
    orm::MySQLLiveDB conn;
    orm::db<orm::MySQLLiveDB> dbc{conn};

    void SetUp() override
    {
        conn = make_connection();
        dbc.rebind(conn);

        // Create and populate test table using MySQL C API
        MYSQL* mysql = conn.native();
        
        if (mysql_query(mysql,
            "CREATE TABLE IF NOT EXISTS products ("
            "  id    INT PRIMARY KEY,"
            "  name  VARCHAR(255),"
            "  price DECIMAL(10,2)"
            ")"))
            throw std::runtime_error(mysql_error(mysql));
        
        if (mysql_query(mysql, "TRUNCATE TABLE products"))
            throw std::runtime_error(mysql_error(mysql));
        
        if (mysql_query(mysql, "INSERT INTO products VALUES (1, 'apple',  0.99)"))
            throw std::runtime_error(mysql_error(mysql));
        
        if (mysql_query(mysql, "INSERT INTO products VALUES (2, 'banana', 0.49)"))
            throw std::runtime_error(mysql_error(mysql));
    }

    void TearDown() override
    {
        MYSQL* mysql = conn.native();
        if (mysql_query(mysql, "DROP TABLE IF EXISTS products"))
            throw std::runtime_error(mysql_error(mysql));
    }
};

// ── Tests ──────────────────────────────────────────────────────────────────────

TEST_F(MySQLLiveTest, ConnectionIsOpen)
{
    EXPECT_TRUE(conn.is_open());
}

TEST_F(MySQLLiveTest, SelectAllReturnsAllRows)
{
    constexpr auto q = orm::select(orm::field<&Product::id>);
    const auto rows  = (dbc << q).to_vector();
    EXPECT_EQ(rows.size(), 2u);
}

TEST_F(MySQLLiveTest, SelectWithWhereFiltersById)
{
    constexpr auto q = orm::select(orm::field<&Product::id>, orm::field<&Product::name>)
        .where(orm::field<&Product::id> == orm::Placeholder<int>{});
    const auto rows = dbc.execute(q, 1).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), 1);
    EXPECT_EQ(std::get<1>(rows[0]), "apple");
}

TEST_F(MySQLLiveTest, SelectWithWhereNoMatchReturnsEmpty)
{
    constexpr auto q = orm::select(orm::field<&Product::id>)
        .where(orm::field<&Product::id> == orm::Placeholder<int>{});
    const auto rows = dbc.execute(q, 999).to_vector();
    EXPECT_TRUE(rows.empty());
}

TEST_F(MySQLLiveTest, InsertAddsRow)
{
    constexpr auto q = orm::insert(
        orm::field<&Product::id>,
        orm::field<&Product::name>,
        orm::field<&Product::price>);
    dbc.execute(q, 3, std::string("cherry"), 1.29);

    const auto rows = (dbc << orm::select(orm::field<&Product::id>)).to_vector();
    EXPECT_EQ(rows.size(), 3u);
}

TEST_F(MySQLLiveTest, UpdateChangesName)
{
    constexpr auto q = orm::update<Product>()
        .set(orm::field<&Product::name>, orm::Placeholder<std::string>{})
        .where(orm::field<&Product::id> == orm::Placeholder<int>{});
    dbc.execute(q, std::string("grape"), 1);

    constexpr auto sel = orm::select(orm::field<&Product::id>, orm::field<&Product::name>)
        .where(orm::field<&Product::id> == orm::Placeholder<int>{});
    const auto rows = dbc.execute(sel, 1).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<1>(rows[0]), "grape");
}

TEST_F(MySQLLiveTest, DeleteRemovesRow)
{
    constexpr auto q = orm::deleteq<Product>()
        .where(orm::field<&Product::id> == orm::Placeholder<int>{});
    dbc.execute(q, 1);

    const auto rows = (dbc << orm::select(orm::field<&Product::id>)).to_vector();
    EXPECT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), 2);
}

TEST_F(MySQLLiveTest, SelectPriceColumn)
{
    constexpr auto q = orm::select(orm::field<&Product::id>, orm::field<&Product::price>)
        .where(orm::field<&Product::id> == orm::Placeholder<int>{});
    const auto rows = dbc.execute(q, 1).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_NEAR(std::get<1>(rows[0]), 0.99, 0.001);
}
