// Level 1 on SQLite, as named unit tests. The differential test covers the
// corpus. These pin the properties that matter one by one, so that a regression
// is reported by name rather than as a changed count.
#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/SQLite/sqlite_db.hpp"
#include <string>

struct SecItem
{
    orm::property<int, "id">           id;
    orm::property<std::string, "name"> name;
};

namespace orm
{
    template <>
    struct table_name_trait<SecItem>
    {
        static constexpr std::string_view value = "items";
    };
} // namespace orm

class PlaceholderPath : public ::testing::Test
{
protected:
    orm::SQLiteDB conn = orm::SQLiteDB::open(":memory:");
    orm::db<orm::SQLiteDB> db{conn};

    void SetUp() override
    {
        sqlite3_exec(conn.handle,
            "CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT);"
            "INSERT INTO items VALUES (1,'apple'),(2,'banana'),(3,'cherry');",
            nullptr, nullptr, nullptr);
    }

    int item_count()
    {
        sqlite3_stmt* s = nullptr;
        if (sqlite3_prepare_v2(conn.handle, "SELECT COUNT(*) FROM items", -1, &s, nullptr) != SQLITE_OK)
            return -1;   // the table is gone
        int n = (sqlite3_step(s) == SQLITE_ROW) ? sqlite3_column_int(s, 0) : -1;
        sqlite3_finalize(s);
        return n;
    }

    auto by_name(const std::string& v)
    {
        constexpr auto q = orm::select(orm::field<&SecItem::id>)
                               .where(orm::field<&SecItem::name> == orm::Placeholder<std::string>{});
        return db.execute(q, v).to_vector();
    }
};

// The control: the documented path finds a row that exists.
TEST_F(PlaceholderPath, BindsTheValue)
{
    EXPECT_EQ(by_name("banana").size(), 1u);
}

// A tautology, the oldest injection there is, matches nothing as data.
TEST_F(PlaceholderPath, TautologyIsData)
{
    EXPECT_EQ(by_name("' OR '1'='1").size(), 0u);
    EXPECT_EQ(item_count(), 3);
}

// A stacked statement does not execute, and the table survives.
TEST_F(PlaceholderPath, StackedStatementIsData)
{
    EXPECT_EQ(by_name("x'; DROP TABLE items; --").size(), 0u);
    EXPECT_EQ(item_count(), 3);
}

// A string that is literally a payload is stored and found as a string. This is
// what distinguishes binding from escaping: nothing is stripped or mangled.
TEST_F(PlaceholderPath, PayloadRoundTripsUnchanged)
{
    const std::string payload = "x' OR '1'='1";
    sqlite3_stmt* s = nullptr;
    sqlite3_prepare_v2(conn.handle, "INSERT INTO items VALUES (4, ?)", -1, &s, nullptr);
    sqlite3_bind_text(s, 1, payload.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(s);
    sqlite3_finalize(s);
    EXPECT_EQ(by_name(payload).size(), 1u);
}
