#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/PostgreSQLDB/postgresql_live.hpp"
#include <cstdlib>
#include <string>
#include <format>
#include <map>

// ── Test entity ───────────────────────────────────────────────────────────────

struct Item
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
    struct table_name_trait<Item>
    {
        static constexpr std::string_view value = "items";
    };
    template <> struct table_name_trait<Author> { static constexpr std::string_view value = "authors"; };
    template <> struct table_name_trait<Book>   { static constexpr std::string_view value = "books"; };
} // namespace orm

// ── Connection helper ─────────────────────────────────────────────────────────

[[nodiscard]] static orm::PostgreSQLLiveDB make_connection()
{
    const char* host     = std::getenv("ORM_POSTGRESQL_HOST");
    const char* port_str = std::getenv("ORM_POSTGRESQL_PORT");
    const char* user     = std::getenv("ORM_POSTGRESQL_USER");
    const char* password = std::getenv("ORM_POSTGRESQL_PASSWORD");
    const char* db       = std::getenv("ORM_POSTGRESQL_DATABASE");

    const std::string conninfo = std::format(
        "host={} port={} user={} password={} dbname={}",
        host     ? host : "127.0.0.1",
        port_str ? port_str : "5432",
        user     ? user : "postgres",
        password ? password : "",
        db       ? db : "postgres");

    return orm::PostgreSQLLiveDB::connect(conninfo.c_str());
}

// ── Fixture ───────────────────────────────────────────────────────────────────

class PostgreSQLLiveTest : public ::testing::Test
{
protected:
    orm::PostgreSQLLiveDB conn;
    orm::db<orm::PostgreSQLLiveDB> dbc{conn};

    void SetUp() override
    {
        conn = make_connection();
        dbc.rebind(conn);

        // Use libpq C API directly
        PGresult* res;
        
        res = PQexec(conn.native(), "BEGIN");
        PQclear(res);
        
        res = PQexec(conn.native(),
            "CREATE TABLE IF NOT EXISTS items ("
            "  id    INTEGER PRIMARY KEY,"
            "  name  TEXT    NOT NULL,"
            "  price FLOAT8  NOT NULL"
            ")");
        PQclear(res);
        
        res = PQexec(conn.native(), "TRUNCATE TABLE items");
        PQclear(res);
        
        res = PQexec(conn.native(), "INSERT INTO items VALUES (1, 'apple',  0.99)");
        PQclear(res);
        
        res = PQexec(conn.native(), "INSERT INTO items VALUES (2, 'banana', 0.49)");
        PQclear(res);
        
        res = PQexec(conn.native(), "COMMIT");
        PQclear(res);
    }

    void TearDown() override
    {
        PGresult* res;
        res = PQexec(conn.native(), "DROP TABLE IF EXISTS items");
        PQclear(res);
    }
};

// ── Tests ──────────────────────────────────────────────────────────────────────

TEST_F(PostgreSQLLiveTest, ConnectionIsOpen)
{
    EXPECT_TRUE(conn.is_open());
}

TEST_F(PostgreSQLLiveTest, SelectAllReturnsAllRows)
{
    constexpr auto q = orm::select(orm::field<&Item::id>);
    const auto rows  = (dbc << q).to_vector();
    EXPECT_EQ(rows.size(), 2u);
}

TEST_F(PostgreSQLLiveTest, SelectWithWhereFiltersById)
{
    constexpr auto q = orm::select(orm::field<&Item::id>, orm::field<&Item::name>)
        .where(orm::field<&Item::id> == orm::Placeholder<int>{});
    const auto rows = dbc.execute(q, 1).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), 1);
    EXPECT_EQ(std::get<1>(rows[0]), "apple");
}

TEST_F(PostgreSQLLiveTest, SelectWithWhereNoMatchReturnsEmpty)
{
    constexpr auto q = orm::select(orm::field<&Item::id>)
        .where(orm::field<&Item::id> == orm::Placeholder<int>{});
    const auto rows = dbc.execute(q, 999).to_vector();
    EXPECT_TRUE(rows.empty());
}

TEST_F(PostgreSQLLiveTest, InsertAddsRow)
{
    constexpr auto q = orm::insert(
        orm::field<&Item::id>,
        orm::field<&Item::name>,
        orm::field<&Item::price>);
    dbc.execute(q, 3, std::string("cherry"), 1.29);

    const auto rows = (dbc << orm::select(orm::field<&Item::id>)).to_vector();
    EXPECT_EQ(rows.size(), 3u);
}

TEST_F(PostgreSQLLiveTest, UpdateChangesName)
{
    constexpr auto q = orm::update<Item>()
        .set(orm::field<&Item::name>, orm::Placeholder<std::string>{})
        .where(orm::field<&Item::id> == orm::Placeholder<int>{});
    dbc.execute(q, std::string("grape"), 1);

    constexpr auto sel = orm::select(orm::field<&Item::id>, orm::field<&Item::name>)
        .where(orm::field<&Item::id> == orm::Placeholder<int>{});
    const auto rows = dbc.execute(sel, 1).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<1>(rows[0]), "grape");
}

TEST_F(PostgreSQLLiveTest, DeleteRemovesRow)
{
    constexpr auto q = orm::deleteq<Item>()
        .where(orm::field<&Item::id> == orm::Placeholder<int>{});
    dbc.execute(q, 1);

    const auto rows = (dbc << orm::select(orm::field<&Item::id>)).to_vector();
    EXPECT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), 2);
}

TEST_F(PostgreSQLLiveTest, SelectPriceColumn)
{
    constexpr auto q = orm::select(orm::field<&Item::id>, orm::field<&Item::price>)
        .where(orm::field<&Item::id> == orm::Placeholder<int>{});
    const auto rows = dbc.execute(q, 1).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_NEAR(std::get<1>(rows[0]), 0.99, 0.001);
}

// ── relationship-aware queries (auto-inferred JOIN → joined_row) ───────────────

class PostgreSQLJoinTest : public ::testing::Test
{
protected:
    orm::PostgreSQLLiveDB conn;
    orm::db<orm::PostgreSQLLiveDB> dbc{conn};

    void exec(const char* sql) { PGresult* r = PQexec(conn.native(), sql); PQclear(r); }

    void SetUp() override
    {
        conn = make_connection();
        dbc.rebind(conn);
        exec("DROP TABLE IF EXISTS books");
        exec("DROP TABLE IF EXISTS authors");
        exec("CREATE TABLE authors (id INTEGER PRIMARY KEY, name TEXT NOT NULL)");
        exec("CREATE TABLE books (id INTEGER PRIMARY KEY, title TEXT NOT NULL, author_id INTEGER)");
        exec("INSERT INTO authors VALUES (1,'Tolkien'),(2,'Le Guin')");
        exec("INSERT INTO books   VALUES (10,'The Hobbit',1),(11,'A Wizard of Earthsea',2)");
    }

    void TearDown() override
    {
        exec("DROP TABLE IF EXISTS books");
        exec("DROP TABLE IF EXISTS authors");
    }
};

TEST_F(PostgreSQLJoinTest, AutoJoinResultTypeIsJoinedRow)
{
    constexpr auto q = orm::select(orm::field<&Book::title>, orm::field<&Author::name>);
    auto res = dbc << q;
    static_assert(std::is_same_v<decltype(res)::value_type, orm::joined_row<Book, Author>>);
    EXPECT_EQ(res.size(), 2u);
}

TEST_F(PostgreSQLJoinTest, AutoJoinHydratesPartialEntities)
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
