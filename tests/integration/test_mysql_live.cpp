#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MySQLDB/mysql_live.hpp"
#include <cstdlib>
#include <string>
#include <map>

// ── Test entity ───────────────────────────────────────────────────────────────

struct Product
{
    orm::property<int, "id">           id;
    orm::property<std::string, "name"> name;
    orm::property<double, "price">     price;
};

// ── related entities for the relationship-aware (auto-JOIN) tests ──────────────
struct Author
{
    orm::property<int, "id">           id;
    orm::property<std::string, "name"> name;
};
struct Book
{
    orm::property<int, "id">            id;
    orm::property<std::string, "title"> title;
    orm::relationship<orm::store_as::reference<&Author::id>, "author_id"> author_id;
};

namespace orm
{
    template <>
    struct table_name_trait<Product>
    {
        static constexpr std::string_view value = "products";
    };
    template <> struct table_name_trait<Author> { static constexpr std::string_view value = "authors"; };
    template <> struct table_name_trait<Book>   { static constexpr std::string_view value = "books"; };
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

// ── relationship-aware queries (auto-inferred JOIN → joined_row) ───────────────

class MySQLJoinTest : public ::testing::Test
{
protected:
    orm::MySQLLiveDB conn;
    orm::db<orm::MySQLLiveDB> dbc{conn};

    void exec(const char* sql) { if (mysql_query(conn.native(), sql)) throw std::runtime_error(mysql_error(conn.native())); }

    void SetUp() override
    {
        conn = make_connection();
        dbc.rebind(conn);
        exec("DROP TABLE IF EXISTS books");
        exec("DROP TABLE IF EXISTS authors");
        exec("CREATE TABLE authors (id INT PRIMARY KEY, name VARCHAR(255))");
        exec("CREATE TABLE books (id INT PRIMARY KEY, title VARCHAR(255), author_id INT)");
        exec("INSERT INTO authors VALUES (1,'Tolkien'),(2,'Le Guin')");
        exec("INSERT INTO books   VALUES (10,'The Hobbit',1),(11,'A Wizard of Earthsea',2)");
    }

    void TearDown() override
    {
        exec("DROP TABLE IF EXISTS books");
        exec("DROP TABLE IF EXISTS authors");
    }
};

TEST_F(MySQLJoinTest, AutoJoinResultTypeIsJoinedRow)
{
    constexpr auto q = orm::select(orm::field<&Book::title>, orm::field<&Author::name>);
    auto res = dbc << q;
    static_assert(std::is_same_v<decltype(res)::value_type, orm::joined_row<Book, Author>>);
    EXPECT_EQ(res.size(), 2u);
}

TEST_F(MySQLJoinTest, AutoJoinHydratesPartialEntities)
{
    // book column + author column → INNER JOIN books→authors inferred; each row is a
    // joined_row with partial Book and Author entities.
    constexpr auto q = orm::select(orm::field<&Book::title>, orm::field<&Author::name>);
    auto rows = (dbc << q).to_vector();
    ASSERT_EQ(rows.size(), 2u);

    std::map<std::string, std::string> title_to_author;
    for (const auto& r : rows)
    {
        EXPECT_TRUE(r.get<Book>().title.has_value());
        EXPECT_TRUE(r.get<Author>().name.has_value());
        EXPECT_FALSE(r.get<Book>().id.has_value());   // id not selected → unset
        title_to_author[r.get<Book>().title.value] = r.get<Author>().name.value;
    }
    EXPECT_EQ(title_to_author["The Hobbit"],           "Tolkien");
    EXPECT_EQ(title_to_author["A Wizard of Earthsea"], "Le Guin");
}
