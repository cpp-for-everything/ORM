// IN and NOT IN on SQLite, as named tests. The list differential covers the corpus.
// These pin the properties a list adds: the marker count follows the length, the
// positions of everything after a list still line up, and an empty list means what
// it should.
#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/MockDB/mock_db.hpp"
#include "ORM/db/connectors/SQLite/sqlite_db.hpp"
#include <algorithm>
#include <string>
#include <vector>

struct ListItem
{
    orm::property<int, "id">           id;
    orm::property<std::string, "name"> name;
    orm::property<int, "owner">        owner;
};

namespace orm
{
    template <>
    struct table_name_trait<ListItem>
    {
        static constexpr std::string_view value = "items";
    };
} // namespace orm

namespace
{
    using Names = std::vector<std::string>;
    using Ids   = std::vector<int>;

    constexpr auto kId    = orm::field<&ListItem::id>;
    constexpr auto kName  = orm::field<&ListItem::name>;
    constexpr auto kOwner = orm::field<&ListItem::owner>;

    class ListSqlite : public ::testing::Test
    {
    protected:
        orm::SQLiteDB          conn = orm::SQLiteDB::open(":memory:");
        orm::db<orm::SQLiteDB> db{conn};

        void SetUp() override
        {
            ASSERT_EQ(sqlite3_exec(conn.handle,
                "CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT, owner INTEGER);"
                "INSERT INTO items VALUES (1,'apple',10),(2,'banana',20),(3,'cherry',10);",
                nullptr, nullptr, nullptr), SQLITE_OK);
        }

        template <typename Q, typename... P>
        Ids ids(const Q& q, P&&... params)
        {
            Ids out;
            for (const auto& row : db.execute(q, std::forward<P>(params)...).to_vector())
                out.push_back(std::get<0>(row));
            std::sort(out.begin(), out.end());
            return out;
        }

        long count()
        {
            long n = 0;
            sqlite3_exec(conn.handle, "SELECT id FROM items",
                [](void* p, int, char**, char**) { ++*static_cast<long*>(p); return 0; },
                &n, nullptr);
            return n;
        }
    };
} // namespace

TEST(ListRendering, MarkerCountFollowsTheLengthAndNothingElse)
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    constexpr auto q = orm::select(kId).where(orm::in(kName, orm::PlaceholderList<std::string>{}));

    (void)db.execute(q, Names{"a"});
    EXPECT_NE(conn.last_sql.find(" IN (?)"), std::string::npos) << conn.last_sql;
    (void)db.execute(q, Names{"a", "b", "c"});
    EXPECT_NE(conn.last_sql.find(" IN (?, ?, ?)"), std::string::npos) << conn.last_sql;

    // A hostile element changes the bound values and leaves the text as it was.
    const std::string text = conn.last_sql;
    (void)db.execute(q, Names{"x') OR 1=1 --", "'; DROP TABLE items; --", "c"});
    EXPECT_EQ(conn.last_sql, text);
    EXPECT_EQ(conn.last_params.size(), 3u);
}

TEST(ListRendering, EmptyListIsAConstantNotASyntaxError)
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    (void)db.execute(orm::select(kId).where(orm::in(kName, orm::PlaceholderList<std::string>{})), Names{});
    EXPECT_NE(conn.last_sql.find("1 = 0"), std::string::npos) << conn.last_sql;
    (void)db.execute(orm::select(kId).where(orm::not_in(kName, orm::PlaceholderList<std::string>{})), Names{});
    EXPECT_NE(conn.last_sql.find("1 = 1"), std::string::npos) << conn.last_sql;
}

TEST_F(ListSqlite, InSelectsTheListedRows)
{
    constexpr auto q = orm::select(kId, kName).where(orm::in(kName, orm::PlaceholderList<std::string>{}));
    EXPECT_EQ(ids(q, Names{"apple", "cherry"}), (Ids{1, 3}));
    EXPECT_EQ(ids(q, Names{"banana"}), (Ids{2}));
}

TEST_F(ListSqlite, NotInSelectsTheOthers)
{
    constexpr auto q = orm::select(kId, kName).where(orm::not_in(kName, orm::PlaceholderList<std::string>{}));
    EXPECT_EQ(ids(q, Names{"apple", "cherry"}), (Ids{2}));
}

TEST_F(ListSqlite, NegationTurnsInIntoNotIn)
{
    constexpr auto q = orm::select(kId, kName).where(!orm::in(kName, orm::PlaceholderList<std::string>{}));
    EXPECT_EQ(ids(q, Names{"apple"}), (Ids{2, 3}));
}

TEST_F(ListSqlite, EmptyListMatchesNothingAndItsNegationEverything)
{
    constexpr auto in_q  = orm::select(kId, kName).where(orm::in(kName, orm::PlaceholderList<std::string>{}));
    constexpr auto out_q = orm::select(kId, kName).where(orm::not_in(kName, orm::PlaceholderList<std::string>{}));
    EXPECT_EQ(ids(in_q, Names{}), Ids{});
    EXPECT_EQ(ids(out_q, Names{}), (Ids{1, 2, 3}));
}

TEST_F(ListSqlite, PayloadInAListIsAValue)
{
    constexpr auto q = orm::select(kId, kName).where(orm::in(kName, orm::PlaceholderList<std::string>{}));
    EXPECT_EQ(ids(q, Names{"x') OR ('1'='1", "banana"}), (Ids{2}));
    EXPECT_EQ(ids(q, Names{"'); DROP TABLE items; --"}), Ids{});
    EXPECT_EQ(count(), 3);
}

// The positional check. A scalar after a list must land on its own marker however
// long the list was: this is the parameter confusion of the literal operand case.
TEST_F(ListSqlite, ScalarAfterAListKeepsItsPosition)
{
    constexpr auto q = orm::select(kId, kName)
                           .where(orm::in(kName, orm::PlaceholderList<std::string>{}))
                           .where(kOwner == orm::Placeholder<int>{});
    EXPECT_EQ(ids(q, Names{"apple", "banana", "cherry"}, 10), (Ids{1, 3}));
    EXPECT_EQ(ids(q, Names{"apple", "banana", "cherry"}, 20), (Ids{2}));
    EXPECT_EQ(ids(q, Names{"banana"}, 10), Ids{});
    EXPECT_EQ(ids(q, Names{}, 10), Ids{});
}

TEST_F(ListSqlite, ScalarBeforeAListKeepsItsPosition)
{
    constexpr auto q = orm::select(kId, kName)
                           .where(kOwner == orm::Placeholder<int>{})
                           .where(orm::in(kName, orm::PlaceholderList<std::string>{}));
    EXPECT_EQ(ids(q, 10, Names{"apple", "banana"}), (Ids{1}));
}

TEST_F(ListSqlite, TwoListsAndAScalarInOneCombinedRule)
{
    constexpr auto q = orm::select(kId, kName)
                           .where(orm::in(kId, orm::PlaceholderList<int>{}) &&
                                  orm::in(kName, orm::PlaceholderList<std::string>{}))
                           .where(kOwner == orm::Placeholder<int>{});
    EXPECT_EQ(ids(q, Ids{1, 2, 3}, Names{"apple", "banana"}, 20), (Ids{2}));
    EXPECT_EQ(ids(q, Ids{2}, Names{"apple", "banana", "cherry"}, 20), (Ids{2}));
}

TEST_F(ListSqlite, UpdateKeepsSetAndListPositionsApart)
{
    auto q = orm::update<ListItem>()
                 .set(kName, orm::Placeholder<std::string>{})
                 .where(orm::in(kId, orm::PlaceholderList<int>{}));
    (void)db.execute(q, std::string("renamed"), Ids{1, 3});
    constexpr auto check = orm::select(kId, kName).where(kName == orm::Placeholder<std::string>{});
    EXPECT_EQ(ids(check, std::string("renamed")), (Ids{1, 3}));
}

TEST_F(ListSqlite, DeleteTakesAList)
{
    (void)db.execute(orm::deleteq<ListItem>().where(orm::in(kId, orm::PlaceholderList<int>{})), Ids{1, 2});
    EXPECT_EQ(count(), 1);
}

TEST_F(ListSqlite, PreparedQueryAcceptsDifferentLengths)
{
    auto pq = db.prepare(orm::select(kId, kName).where(orm::in(kId, orm::PlaceholderList<int>{})));
    EXPECT_EQ(pq.execute(Ids{1}).to_vector().size(), 1u);
    EXPECT_EQ(pq.execute(Ids{1, 2, 3}).to_vector().size(), 3u);
}

// Past the engine's parameter limit the statement fails loudly. It must not run with
// some elements silently left out.
TEST_F(ListSqlite, ListBeyondTheEngineLimitFailsLoudly)
{
    constexpr auto q = orm::select(kId, kName).where(orm::in(kId, orm::PlaceholderList<int>{}));
    // The default limit differs between SQLite builds, so the test sets its own.
    sqlite3_limit(conn.handle, SQLITE_LIMIT_VARIABLE_NUMBER, 100);
    EXPECT_EQ(ids(q, Ids(100, 1)), (Ids{1}));
    EXPECT_THROW((void)db.execute(q, Ids(101, 1)), std::runtime_error);
}
