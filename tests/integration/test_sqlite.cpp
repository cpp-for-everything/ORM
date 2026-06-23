#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/SQLite/sqlite_db.hpp"
#include <filesystem>
#include <map>

struct Item
{
    orm::property<int, "id">          id;
    orm::property<std::string, "name"> name;
    orm::property<double, "price">    price;
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

class SQLiteTest : public ::testing::Test
{
protected:
    orm::SQLiteDB conn;
    orm::db<orm::SQLiteDB> db{conn};

    void SetUp() override
    {
        conn = orm::SQLiteDB::open(":memory:");
        sqlite3_exec(conn.handle,
            "CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT, price REAL);",
            nullptr, nullptr, nullptr);
        sqlite3_exec(conn.handle,
            "INSERT INTO items VALUES (1, 'apple', 0.99);",
            nullptr, nullptr, nullptr);
        sqlite3_exec(conn.handle,
            "INSERT INTO items VALUES (2, 'banana', 0.49);",
            nullptr, nullptr, nullptr);
    }
};

TEST_F(SQLiteTest, ConnectionIsOpen)
{
    EXPECT_TRUE(conn.is_open());
}

TEST_F(SQLiteTest, OpenInMemoryDatabase)
{
    orm::SQLiteDB db2 = orm::SQLiteDB::open(":memory:");
    EXPECT_TRUE(db2.is_open());
}

TEST_F(SQLiteTest, MoveConstructor)
{
    orm::SQLiteDB db2 = orm::SQLiteDB::open(":memory:");
    orm::SQLiteDB db3 = std::move(db2);
    EXPECT_TRUE(db3.is_open());
    EXPECT_FALSE(db2.is_open());
}

TEST_F(SQLiteTest, CloseReleasesHandle)
{
    orm::SQLiteDB db2 = orm::SQLiteDB::open(":memory:");
    db2.close();
    EXPECT_FALSE(db2.is_open());
}

TEST_F(SQLiteTest, SelectReturnsTypedResult)
{
    constexpr auto q = orm::select(orm::field<&Item::id>);
    auto res = db << q;
    using ResType = decltype(res);
    static_assert(std::is_same_v<ResType::value_type, std::tuple<int>>);
}

TEST_F(SQLiteTest, SelectHydratesRows)
{
    constexpr auto q = orm::select(orm::field<&Item::id>);
    auto res = db << q;
    EXPECT_EQ(res.size(), 2u);
}

TEST_F(SQLiteTest, SelectHydratesRowValues)
{
    constexpr auto q = orm::select(orm::field<&Item::id>);
    auto rows = (db << q).to_vector();
    EXPECT_EQ(std::get<0>(rows[0]), 1);
    EXPECT_EQ(std::get<0>(rows[1]), 2);
}

TEST_F(SQLiteTest, ConnectorHasJoinCapability)
{
    static_assert(orm::has_capability<orm::SQLiteDB, orm::cap::supports_joins>);
}

TEST_F(SQLiteTest, ConnectorHasTransactionCapability)
{
    static_assert(orm::has_capability<orm::SQLiteDB, orm::cap::supports_transactions>);
}

TEST_F(SQLiteTest, FindOneEmptyReturnsNullopt)
{
    sqlite3_exec(conn.handle, "DELETE FROM items;", nullptr, nullptr, nullptr);
    constexpr auto q = orm::select(orm::field<&Item::id>);
    auto opt = db.find_one(q);
    EXPECT_FALSE(opt.has_value());
}

TEST_F(SQLiteTest, SelectWithWhereFiltersByParam)
{
    constexpr auto q = orm::select(orm::field<&Item::id>)
        .where(orm::field<&Item::id> == orm::Placeholder<int>{});
    auto rows = db.execute(q, 1).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), 1);
}

TEST_F(SQLiteTest, SelectWithWhereNoMatchReturnsEmpty)
{
    constexpr auto q = orm::select(orm::field<&Item::id>)
        .where(orm::field<&Item::id> == orm::Placeholder<int>{});
    auto rows = db.execute(q, 999).to_vector();
    EXPECT_TRUE(rows.empty());
}

TEST_F(SQLiteTest, FindOneWithParamReturnsFirst)
{
    auto opt = db.find_one(orm::select(orm::field<&Item::id>));
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(std::get<0>(*opt), 1);
}

TEST_F(SQLiteTest, SelectAllRowsHydratesName)
{
    constexpr auto q = orm::select(orm::field<&Item::id>, orm::field<&Item::name>);
    auto rows = (db << q).to_vector();
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(std::get<1>(rows[0]), "apple");
    EXPECT_EQ(std::get<1>(rows[1]), "banana");
}

TEST_F(SQLiteTest, InsertAddsRow)
{
    constexpr auto q = orm::insert(orm::field<&Item::id>, orm::field<&Item::name>, orm::field<&Item::price>);
    db.execute(q, 3, std::string("cherry"), 1.29);
    constexpr auto sel = orm::select(orm::field<&Item::id>);
    auto rows = (db << sel).to_vector();
    EXPECT_EQ(rows.size(), 3u);
}

TEST_F(SQLiteTest, UpdateChangesRow)
{
    constexpr auto q = orm::update<Item>()
        .set(orm::field<&Item::name>, orm::Placeholder<std::string>{})
        .where(orm::field<&Item::id> == orm::Placeholder<int>{});
    db.execute(q, std::string("grape"), 1);
    constexpr auto sel = orm::select(orm::field<&Item::id>, orm::field<&Item::name>)
        .where(orm::field<&Item::id> == orm::Placeholder<int>{});
    auto rows = db.execute(sel, 1).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<1>(rows[0]), "grape");
}

TEST_F(SQLiteTest, DeleteRemovesRow)
{
    constexpr auto q = orm::deleteq<Item>()
        .where(orm::field<&Item::id> == orm::Placeholder<int>{});
    db.execute(q, 1);
    constexpr auto sel = orm::select(orm::field<&Item::id>);
    auto rows = (db << sel).to_vector();
    EXPECT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), 2);
}

TEST_F(SQLiteTest, DeleteAllRemovesAllRows)
{
    constexpr auto q = orm::deleteq<Item>();
    db.execute(q);
    constexpr auto sel = orm::select(orm::field<&Item::id>);
    EXPECT_TRUE((db << sel).empty());
}

TEST_F(SQLiteTest, GetFieldByMemberPointerReturnsCorrectColumn)
{
    constexpr auto q = orm::select(orm::field<&Item::id>, orm::field<&Item::name>);
    auto res = db << q;
    auto rows = res.to_vector();
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ((decltype(res)::get_field<&Item::id>(rows[0])), 1);
    EXPECT_EQ((decltype(res)::get_field<&Item::name>(rows[0])), "apple");
    EXPECT_EQ((decltype(res)::get_field<&Item::id>(rows[1])), 2);
    EXPECT_EQ((decltype(res)::get_field<&Item::name>(rows[1])), "banana");
}

TEST_F(SQLiteTest, GetFieldPriceColumn)
{
    constexpr auto q = orm::select(orm::field<&Item::id>, orm::field<&Item::price>);
    auto res = db << q;
    auto rows = res.to_vector();
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_DOUBLE_EQ((decltype(res)::get_field<&Item::price>(rows[0])), 0.99);
    EXPECT_DOUBLE_EQ((decltype(res)::get_field<&Item::price>(rows[1])), 0.49);
}

TEST_F(SQLiteTest, IndexedPlaceholderFiltersCorrectly)
{
    using namespace std::placeholders;
    constexpr auto q = orm::select(orm::field<&Item::id>, orm::field<&Item::name>)
        .where(orm::field<&Item::id> == orm::ph<int, _1>);
    auto rows = db.execute(q, 2).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), 2);
    EXPECT_EQ(std::get<1>(rows[0]), "banana");
}

TEST_F(SQLiteTest, IndexedPlaceholderReusedMatchesBothConditions)
{
    using namespace std::placeholders;
    // WHERE id = ?1 AND id = ?1  -- reuse same arg: should match row with id==1
    constexpr auto q = orm::select(orm::field<&Item::id>)
        .where((orm::field<&Item::id> == orm::ph<int, _1>)
            && (orm::field<&Item::id> == orm::ph<int, _1>));
    auto rows = db.execute(q, 1).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), 1);
}

TEST_F(SQLiteTest, PrepareSelectAndExecuteReturnsRows)
{
    const auto pq = db.prepare(orm::select(orm::field<&Item::id>));
    auto rows = pq.execute().to_vector();
    EXPECT_EQ(rows.size(), 2u);
}

TEST_F(SQLiteTest, PrepareSelectWithParamFiltersCorrectly)
{
    using namespace std::placeholders;
    const auto pq = db.prepare(
        orm::select(orm::field<&Item::id>, orm::field<&Item::name>)
            .where(orm::field<&Item::id> == orm::ph<int, _1>));
    auto rows = pq.execute(1).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), 1);
    EXPECT_EQ(std::get<1>(rows[0]), "apple");
}

TEST_F(SQLiteTest, PrepareCanBeReusedWithDifferentParams)
{
    using namespace std::placeholders;
    const auto pq = db.prepare(
        orm::select(orm::field<&Item::id>, orm::field<&Item::name>)
            .where(orm::field<&Item::id> == orm::ph<int, _1>));

    auto r1 = pq.execute(1).to_vector();
    ASSERT_EQ(r1.size(), 1u);
    EXPECT_EQ(std::get<1>(r1[0]), "apple");

    auto r2 = pq.execute(2).to_vector();
    ASSERT_EQ(r2.size(), 1u);
    EXPECT_EQ(std::get<1>(r2[0]), "banana");
}

TEST_F(SQLiteTest, PrepareInsertExecutesWithParams)
{
    const auto pq = db.prepare(
        orm::insert(orm::field<&Item::id>, orm::field<&Item::name>, orm::field<&Item::price>));
    pq.execute(3, std::string("cherry"), 2.49);

    auto rows = (db << orm::select(orm::field<&Item::id>)).to_vector();
    EXPECT_EQ(rows.size(), 3u);
}

TEST_F(SQLiteTest, IndexedPlaceholderTwoDistinctArgsFilter)
{
    using namespace std::placeholders;
    // WHERE id = ?1 AND price > ?2
    constexpr auto q = orm::select(orm::field<&Item::id>, orm::field<&Item::price>)
        .where((orm::field<&Item::id>    == orm::ph<int, _1>)
            && (orm::field<&Item::price>  > orm::ph<double, _2>));
    auto rows = db.execute(q, 1, 0.5).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), 1);
    EXPECT_DOUBLE_EQ(std::get<1>(rows[0]), 0.99);
}

// ── relationship-aware queries (auto-inferred JOIN → joined_row) ───────────────

class SQLiteJoinTest : public ::testing::Test
{
protected:
    orm::SQLiteDB conn;
    orm::db<orm::SQLiteDB> db{conn};

    void exec(const char* sql) { sqlite3_exec(conn.handle, sql, nullptr, nullptr, nullptr); }

    void SetUp() override
    {
        conn = orm::SQLiteDB::open(":memory:");
        exec("CREATE TABLE authors (id INTEGER PRIMARY KEY, name TEXT);");
        exec("CREATE TABLE books (id INTEGER PRIMARY KEY, title TEXT, author_id INTEGER);");
        exec("INSERT INTO authors VALUES (1,'Tolkien'),(2,'Le Guin');");
        exec("INSERT INTO books   VALUES (10,'The Hobbit',1),(11,'A Wizard of Earthsea',2);");
    }
};

TEST_F(SQLiteJoinTest, AutoJoinResultTypeIsJoinedRow)
{
    constexpr auto q = orm::select(orm::field<&Book::title>, orm::field<&Author::name>);
    auto res = db << q;
    static_assert(std::is_same_v<decltype(res)::value_type, orm::joined_row<Book, Author>>);
    EXPECT_EQ(res.size(), 2u);
}

TEST_F(SQLiteJoinTest, AutoJoinHydratesPartialEntities)
{
    // select a book column + an author column → INNER JOIN books→authors is inferred,
    // each row is a joined_row with partial Book and Author entities.
    constexpr auto q = orm::select(orm::field<&Book::title>, orm::field<&Author::name>);
    auto rows = (db << q).to_vector();
    ASSERT_EQ(rows.size(), 2u);

    std::map<std::string, std::string> title_to_author;
    for (const auto& r : rows)
    {
        EXPECT_TRUE(r.get<Book>().title.has_value());
        EXPECT_TRUE(r.get<Author>().name.has_value());
        EXPECT_FALSE(r.get<Book>().id.has_value());   // id not selected → unset
        EXPECT_FALSE(r.get<Author>().id.has_value());
        title_to_author[r.get<Book>().title.value] = r.get<Author>().name.value;
    }
    EXPECT_EQ(title_to_author["The Hobbit"],           "Tolkien");
    EXPECT_EQ(title_to_author["A Wizard of Earthsea"], "Le Guin");
}

TEST_F(SQLiteJoinTest, SingleTableStillReturnsTuple)
{
    // a single-table select over a related entity keeps the flat-tuple behaviour
    constexpr auto q = orm::select(orm::field<&Book::id>, orm::field<&Book::title>);
    auto res = db << q;
    static_assert(std::is_same_v<decltype(res)::value_type, std::tuple<int, std::string>>);
    auto rows = res.to_vector();
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(std::get<0>(rows[0]), 10);
}
