// Tests for the HIGH findings of the code review that SQLite can show for real:
// clause rendering (LIMIT, ORDER BY, GROUP BY, JOIN), step and bind errors,
// parentheses around compound rules, and negation of compound rules.
#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/SQLite/sqlite_db.hpp"
#include "ORM/db/connectors/WireProtocol/wire_protocol.hpp"
#include "ORM/connector/async_db.hpp"
#include "ORM/async/thread_pool.hpp"
#include <atomic>
#include <set>
#include <string>
#include <thread>
#include <vector>

struct RvItem
{
    orm::property<int, "id">             id;
    orm::property<std::string, "name">   name;
    orm::property<int, "grp">            grp;
    orm::property<int, "owner_id">       owner_id;
};

struct RvOwner
{
    orm::property<int, "oid">            oid;
    orm::property<std::string, "oname">  oname;
};

namespace orm
{
    template <> struct table_name_trait<RvItem>  { static constexpr std::string_view value = "rv_items"; };
    template <> struct table_name_trait<RvOwner> { static constexpr std::string_view value = "rv_owners"; };
} // namespace orm

class SQLiteReviewTest : public ::testing::Test
{
protected:
    orm::SQLiteDB conn;
    orm::db<orm::SQLiteDB> db{conn};

    void exec(const char* sql)
    {
        char* err = nullptr;
        ASSERT_EQ(sqlite3_exec(conn.handle, sql, nullptr, nullptr, &err), SQLITE_OK)
            << (err ? err : "");
        sqlite3_free(err);
    }

    void SetUp() override
    {
        conn = orm::SQLiteDB::open(":memory:");
        exec("CREATE TABLE rv_items (id INTEGER PRIMARY KEY, name TEXT UNIQUE, grp INT, owner_id INT);");
        exec("CREATE TABLE rv_owners (oid INTEGER PRIMARY KEY, oname TEXT);");
        exec("INSERT INTO rv_items VALUES (1, 'a', 1, 10), (2, 'b', 1, 10), (3, 'c', 2, 20),"
             " (4, 'd', 2, 30), (5, 'e', 3, 20);");
        exec("INSERT INTO rv_owners VALUES (10, 'ann'), (20, 'bob');");
    }

    template <typename Rows>
    static std::set<int> ids(const Rows& rows)
    {
        std::set<int> out;
        for (const auto& r : rows) out.insert(std::get<0>(r));
        return out;
    }
};

// ── finding 1: LIMIT, ORDER BY, GROUP BY, JOIN ────────────────────────────────

TEST_F(SQLiteReviewTest, LimitRestrictsRows)
{
    constexpr auto q = orm::select(orm::field<&RvItem::id>)
        .order_by<orm::order::direction::asc>(orm::field<&RvItem::id>)
        .limit(orm::Pagification{2, 0});
    auto rows = (db << q).to_vector();
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(std::get<0>(rows[0]), 1);
    EXPECT_EQ(std::get<0>(rows[1]), 2);
}

TEST_F(SQLiteReviewTest, LimitPageSkipsRows)
{
    constexpr auto q = orm::select(orm::field<&RvItem::id>)
        .order_by<orm::order::direction::asc>(orm::field<&RvItem::id>)
        .limit(orm::Pagification{2, 1});
    auto rows = (db << q).to_vector();
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(std::get<0>(rows[0]), 3);
    EXPECT_EQ(std::get<0>(rows[1]), 4);
}

TEST_F(SQLiteReviewTest, OrderByDescSortsRows)
{
    constexpr auto q = orm::select(orm::field<&RvItem::id>)
        .order_by<orm::order::direction::desc>(orm::field<&RvItem::id>);
    auto rows = (db << q).to_vector();
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(std::get<0>(rows[0]), 5);
    EXPECT_EQ(std::get<0>(rows[4]), 1);
}

TEST_F(SQLiteReviewTest, GroupByCollapsesRows)
{
    constexpr auto q = orm::select(orm::field<&RvItem::grp>)
        .group_by(orm::field<&RvItem::grp>);
    auto rows = (db << q).to_vector();
    EXPECT_EQ(rows.size(), 3u);
}

TEST_F(SQLiteReviewTest, ExplicitJoinFiltersRows)
{
    constexpr auto q = orm::select(orm::field<&RvItem::id>)
        .join<orm::join::mode::inner, RvOwner>(
            orm::field<&RvItem::owner_id> == orm::field<&RvOwner::oid>);
    auto rows = (db << q).to_vector();
    // Item 4 has owner 30, which does not exist, so the inner join drops it.
    EXPECT_EQ(ids(rows), (std::set<int>{1, 2, 3, 5}));
}

// ── finding 2: INSERT, UPDATE, DELETE results ─────────────────────────────────

TEST_F(SQLiteReviewTest, InsertConstraintViolationThrows)
{
    constexpr auto q = orm::insert(orm::field<&RvItem::id>, orm::field<&RvItem::name>);
    EXPECT_THROW(db.execute(q, 1, std::string("zzz")), std::runtime_error);
}

TEST_F(SQLiteReviewTest, UpdateConstraintViolationThrows)
{
    constexpr auto q = orm::update<RvItem>()
        .set(orm::field<&RvItem::name>, orm::Placeholder<std::string>{})
        .where(orm::field<&RvItem::id> == orm::Placeholder<int>{});
    EXPECT_THROW(db.execute(q, std::string("a"), 2), std::runtime_error);
}

TEST_F(SQLiteReviewTest, DeleteFailureThrows)
{
    exec("CREATE TRIGGER no_delete BEFORE DELETE ON rv_items "
         "BEGIN SELECT RAISE(ABORT, 'no deletes'); END;");
    constexpr auto q = orm::deleteq<RvItem>();
    EXPECT_THROW(db.execute(q), std::runtime_error);
}

// ── finding 3: SELECT must end with SQLITE_DONE ───────────────────────────────

TEST_F(SQLiteReviewTest, ErrorMidScanThrowsInsteadOfPartialResult)
{
    exec("DROP TABLE rv_items;");
    // abs() of the smallest integer raises an error at row 3, after two good rows.
    exec("CREATE TABLE rv_base (id INTEGER, name TEXT);");
    exec("INSERT INTO rv_base VALUES (1, 'a'), (2, 'b'), (3, 'c'), (4, 'd');");
    exec("CREATE VIEW rv_items AS SELECT id, "
         "CASE WHEN id = 3 THEN abs(-9223372036854775807 - 1) ELSE name END AS name "
         "FROM rv_base;");
    constexpr auto q = orm::select(orm::field<&RvItem::id>, orm::field<&RvItem::name>);
    EXPECT_THROW((void)(db << q), std::runtime_error);
}

// ── findings 4 and 5: parentheses and negation ────────────────────────────────

TEST_F(SQLiteReviewTest, OrClauseThenAndClauseKeepsGrouping)
{
    using namespace std::placeholders;
    // (grp = 1 OR grp = 3) AND id > 1  ->  ids 2 and 5.
    // Without parentheses: grp = 1 OR (grp = 3 AND id > 1)  ->  ids 1, 2 and 5.
    constexpr auto q = orm::select(orm::field<&RvItem::id>)
        .where(orm::field<&RvItem::grp> == orm::ph<int, _1>
               || orm::field<&RvItem::grp> == orm::ph<int, _2>)
        .where(orm::field<&RvItem::id> > orm::ph<int, _3>);
    auto rows = db.execute(q, 1, 3, 1).to_vector();
    EXPECT_EQ(ids(rows), (std::set<int>{2, 5}));
}

TEST_F(SQLiteReviewTest, NestedOrInsideAndKeepsGrouping)
{
    using namespace std::placeholders;
    constexpr auto q = orm::select(orm::field<&RvItem::id>)
        .where((orm::field<&RvItem::grp> == orm::ph<int, _1>
                || orm::field<&RvItem::grp> == orm::ph<int, _2>)
               && orm::field<&RvItem::id> > orm::ph<int, _3>);
    auto rows = db.execute(q, 1, 3, 1).to_vector();
    EXPECT_EQ(ids(rows), (std::set<int>{2, 5}));
}

TEST_F(SQLiteReviewTest, NotOfAndIsDeMorgan)
{
    using namespace std::placeholders;
    // !(grp = 1 AND id = 2): every row except id 2.
    constexpr auto q = orm::select(orm::field<&RvItem::id>)
        .where(!(orm::field<&RvItem::grp> == orm::ph<int, _1>
                 && orm::field<&RvItem::id> == orm::ph<int, _2>));
    auto rows = db.execute(q, 1, 2).to_vector();
    EXPECT_EQ(ids(rows), (std::set<int>{1, 3, 4, 5}));
}

TEST_F(SQLiteReviewTest, NotOfOrIsDeMorgan)
{
    using namespace std::placeholders;
    // !(grp = 1 OR grp = 2): only grp 3, which is id 5.
    constexpr auto q = orm::select(orm::field<&RvItem::id>)
        .where(!(orm::field<&RvItem::grp> == orm::ph<int, _1>
                 || orm::field<&RvItem::grp> == orm::ph<int, _2>));
    auto rows = db.execute(q, 1, 2).to_vector();
    EXPECT_EQ(ids(rows), (std::set<int>{5}));
}

// ── batch_insert: every stored row reaches the table in one call ──────────────

TEST_F(SQLiteReviewTest, BatchInsertStoresEveryRow)
{
    const auto q = orm::insert(orm::field<&RvItem::id>, orm::field<&RvItem::name>,
                               orm::field<&RvItem::grp>, orm::field<&RvItem::owner_id>);
    orm::batch_insert<orm::SQLiteDB, decltype(q.signature())> bi;
    bi.add_row({"101", "x", "7", "10"});
    bi.add_row({"102", "y", "8", "20"});
    bi.add_row({"103", "z", "9", "30"});
    bi.execute(conn, q);

    EXPECT_EQ(bi.execute_count(), 1);

    using namespace std::placeholders;
    constexpr auto sel = orm::select(orm::field<&RvItem::id>, orm::field<&RvItem::name>,
                                     orm::field<&RvItem::grp>)
        .where(orm::field<&RvItem::id> > orm::ph<int, _1>);
    auto rows = db.execute(sel, 100).to_vector();
    ASSERT_EQ(rows.size(), 3u);
    std::set<std::tuple<int, std::string, int>> got;
    for (const auto& r : rows)
        got.emplace(std::get<0>(r), std::get<1>(r), std::get<2>(r));
    EXPECT_EQ(got, (std::set<std::tuple<int, std::string, int>>{
        {101, "x", 7}, {102, "y", 8}, {103, "z", 9}}));
}

// ── finding 12: several tasks on one connection ───────────────────────────────
// Eight threads each start queries through one async_db. Every query must
// return the row it asked for.

TEST_F(SQLiteReviewTest, ConcurrentAsyncQueriesOnOneConnectionReturnTheirOwnRows)
{
    using namespace std::placeholders;
    orm::ThreadPool pool{4};
    orm::async_db<orm::SQLiteDB> adb(conn, pool);
    constexpr auto q = orm::select(orm::field<&RvItem::id>, orm::field<&RvItem::name>)
        .where(orm::field<&RvItem::id> == orm::ph<int, _1>);

    constexpr int kThreads = 8;
    constexpr int kRounds = 100;
    std::atomic<int> wrong{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&, t] {
            const int want = 1 + t % 5;
            for (int i = 0; i < kRounds; ++i)
            {
                auto rows = adb.async_execute(q, want).sync_wait().to_vector();
                if (rows.size() != 1 || std::get<0>(rows[0]) != want)
                    ++wrong;
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(wrong.load(), 0);
}
