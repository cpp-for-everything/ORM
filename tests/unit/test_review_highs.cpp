// Tests for HIGH findings of the code review that need no database server:
// rule parentheses and negation, number formatting, the transaction guard
// destructor, and coroutine parameter lifetime.
#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MockDB/mock_db.hpp"
#include "ORM/db/connectors/WireProtocol/wire_protocol.hpp"
#include "ORM/db/connectors/MongoDB/mongodb_db.hpp"
#include "ORM/db/connectors/ThreadSafety/thread_safety.hpp"
#include "ORM/details/number_format.hpp"
#include "ORM/connector/async_db.hpp"
#include "ORM/async/thread_pool.hpp"
#include "ORM/async/task.hpp"
#include "ORM/async/io_context.hpp"
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

struct RvRow
{
    orm::property<int, "a"> a;
    orm::property<int, "b"> b;
    orm::property<int, "c"> c;
};

namespace orm
{
    template <> struct table_name_trait<RvRow> { static constexpr std::string_view value = "rv_rows"; };
} // namespace orm

// ── rules: parentheses ────────────────────────────────────────────────────────
// MockDB prints every operand as ?, so the text shows the grouping only.

class RvMockTest : public ::testing::Test
{
protected:
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
};

TEST_F(RvMockTest, OrClauseFollowedByAndClauseIsParenthesised)
{
    constexpr auto q = orm::select(orm::field<&RvRow::a>)
        .where(orm::field<&RvRow::a> == orm::Placeholder<int>{}
               || orm::field<&RvRow::b> == orm::Placeholder<int>{})
        .where(orm::field<&RvRow::c> == orm::Placeholder<int>{});
    db.execute(q, 1, 2, 3);
    EXPECT_NE(conn.last_sql.find("(? == ? || ? == ?) AND ? == ?"), std::string::npos)
        << conn.last_sql;
}

TEST_F(RvMockTest, SingleOrClauseHasNoParentheses)
{
    constexpr auto q = orm::select(orm::field<&RvRow::a>)
        .where(orm::field<&RvRow::a> == orm::Placeholder<int>{}
               || orm::field<&RvRow::b> == orm::Placeholder<int>{});
    db.execute(q, 1, 2);
    EXPECT_NE(conn.last_sql.find("WHERE ? == ? || ? == ?"), std::string::npos)
        << conn.last_sql;
}

TEST_F(RvMockTest, NestedOrInsideAndIsParenthesised)
{
    constexpr auto q = orm::select(orm::field<&RvRow::a>)
        .where((orm::field<&RvRow::a> == orm::Placeholder<int>{}
                || orm::field<&RvRow::b> == orm::Placeholder<int>{})
               && orm::field<&RvRow::c> == orm::Placeholder<int>{});
    db.execute(q, 1, 2, 3);
    EXPECT_NE(conn.last_sql.find("(? == ? || ? == ?) && ? == ?"), std::string::npos)
        << conn.last_sql;
}

// ── rules: negation ───────────────────────────────────────────────────────────

TEST(RvRules, NotOfAndNegatesBothOperandsAndFlipsOperator)
{
    constexpr auto r = orm::field<&RvRow::a> == 1 && orm::field<&RvRow::b> > 2;
    constexpr auto n = !r;
    static_assert(decltype(n)::operation == "||");
    static_assert(decltype(n.lhs_)::operation == "!=");
    static_assert(decltype(n.rhs_)::operation == "<=");
    SUCCEED();
}

TEST(RvRules, NotOfOrNegatesBothOperandsAndFlipsOperator)
{
    constexpr auto r = orm::field<&RvRow::a> == 1 || orm::field<&RvRow::b> > 2;
    constexpr auto n = !r;
    static_assert(decltype(n)::operation == "&&");
    static_assert(decltype(n.lhs_)::operation == "!=");
    static_assert(decltype(n.rhs_)::operation == "<=");
    SUCCEED();
}

TEST(RvRules, NotNotIsTheOriginalShape)
{
    constexpr auto r = orm::field<&RvRow::a> == 1 && orm::field<&RvRow::b> > 2;
    constexpr auto nn = !!r;
    static_assert(decltype(nn)::operation == "&&");
    static_assert(decltype(nn.lhs_)::operation == "==");
    static_assert(decltype(nn.rhs_)::operation == ">");
    SUCCEED();
}

TEST(RvRules, NotOfNestedCompoundRecurses)
{
    constexpr auto r = (orm::field<&RvRow::a> == 1 || orm::field<&RvRow::b> == 2)
                       && orm::field<&RvRow::c> == 3;
    constexpr auto n = !r;
    static_assert(decltype(n)::operation == "||");
    static_assert(decltype(n.lhs_)::operation == "&&");
    static_assert(decltype(n.lhs_.lhs_)::operation == "!=");
    SUCCEED();
}

// ── number formatting ─────────────────────────────────────────────────────────

TEST(RvNumberFormat, DoubleIsShortestRoundTrip)
{
    EXPECT_EQ(orm::detail::arithmetic_to_string(0.1), "0.1");
    EXPECT_EQ(orm::detail::arithmetic_to_string(1.5), "1.5");
}

TEST(RvNumberFormat, SmallValueKeepsItsDigits)
{
    // std::to_string would give "0.000000".
    const std::string s = orm::detail::arithmetic_to_string(1.0e-7);
    double back = 0.0;
    std::from_chars(s.data(), s.data() + s.size(), back);
    EXPECT_EQ(back, 1.0e-7);
    EXPECT_NE(s, "0.000000");
}

TEST(RvNumberFormat, DoubleRoundTripsExactly)
{
    const double v = 1.0 / 3.0;
    const std::string s = orm::detail::arithmetic_to_string(v);
    double back = 0.0;
    std::from_chars(s.data(), s.data() + s.size(), back);
    EXPECT_EQ(back, v);
}

TEST(RvNumberFormat, FloatRoundTripsExactly)
{
    const float v = 0.1f;
    const std::string s = orm::detail::arithmetic_to_string(v);
    float back = 0.0f;
    std::from_chars(s.data(), s.data() + s.size(), back);
    EXPECT_EQ(back, v);
}

TEST(RvNumberFormat, NanAndInfinityAreRejected)
{
    EXPECT_THROW(orm::detail::arithmetic_to_string(std::numeric_limits<double>::quiet_NaN()),
                 std::invalid_argument);
    EXPECT_THROW(orm::detail::arithmetic_to_string(std::numeric_limits<double>::infinity()),
                 std::invalid_argument);
    EXPECT_THROW(orm::detail::arithmetic_to_string(-std::numeric_limits<float>::infinity()),
                 std::invalid_argument);
}

TEST(RvNumberFormat, IntegersAreUnchanged)
{
    EXPECT_EQ(orm::detail::arithmetic_to_string(42), "42");
    EXPECT_EQ(orm::detail::arithmetic_to_string(-7LL), "-7");
}

// ── transaction guard destructor ──────────────────────────────────────────────

struct RvThrowingDB {};

namespace orm
{
    template <>
    struct connector_trait<RvThrowingDB>
    {
        using supports_transactions = void;
        static inline int rollbacks = 0;
        static void begin(RvThrowingDB&) {}
        static void commit(RvThrowingDB&) {}
        static void rollback(RvThrowingDB&)
        {
            ++rollbacks;
            throw std::runtime_error("connection lost");
        }
    };
} // namespace orm

TEST(RvTransactionGuard, FailedRollbackInDestructorDoesNotTerminate)
{
    RvThrowingDB conn;
    orm::connector_trait<RvThrowingDB>::rollbacks = 0;
    {
        orm::transaction_guard<RvThrowingDB> guard(conn);
    }
    EXPECT_EQ(orm::connector_trait<RvThrowingDB>::rollbacks, 1);
}

TEST(RvTransactionGuard, FailedRollbackWhileUnwindingKeepsTheFirstError)
{
    RvThrowingDB conn;
    try
    {
        orm::transaction_guard<RvThrowingDB> guard(conn);
        throw std::logic_error("first error");
    }
    catch (const std::logic_error& e)
    {
        EXPECT_STREQ(e.what(), "first error");
        return;
    }
    FAIL() << "the first error must reach the caller";
}

// ── coroutine parameter lifetime ──────────────────────────────────────────────

namespace
{
    // The destructor poisons the value, so a read through a dangling reference
    // to a destroyed temporary shows -1.
    struct PoisonOnDestroy
    {
        volatile int value;
        explicit PoisonOnDestroy(int v) : value(v) {}
        PoisonOnDestroy(const PoisonOnDestroy& o) : value(o.value) {}
        PoisonOnDestroy(PoisonOnDestroy&& o) noexcept : value(o.value) {}
        ~PoisonOnDestroy() { value = -1; }
        int operator()() { return value; }
    };
}

TEST(RvAsyncLifetime, RunOnPoolCopiesATemporaryCallable)
{
    orm::ThreadPool pool{1};
    auto task = orm::run_on_pool(pool, PoisonOnDestroy{42});
    EXPECT_EQ(task.sync_wait(), 42);
}

TEST(RvAsyncLifetime, AsyncExecuteCopiesTemporaryParams)
{
    orm::MockDB conn;
    orm::ThreadPool pool{1};
    orm::async_db<orm::MockDB> adb(conn, pool);
    constexpr auto q = orm::insert(orm::field<&RvRow::a>, orm::field<&RvRow::b>);
    auto task = adb.async_execute(q, 1234567, std::string(64, 'x'));
    task.sync_wait();
    ASSERT_EQ(conn.last_params.size(), 2u);
    EXPECT_EQ(conn.last_params[0], "1234567");
    EXPECT_EQ(conn.last_params[1], std::string(64, 'x'));
}

// ── batch_insert: the stored rows are sent ────────────────────────────────────

TEST(RvBatchInsert, OneCallCarriesEveryRowAndValue)
{
    orm::MockDB conn;
    const auto q = orm::insert(orm::field<&RvRow::a>, orm::field<&RvRow::b>);
    orm::batch_insert<orm::MockDB, decltype(q.signature())> bi;
    bi.add_row({"1", "2"});
    bi.add_row({"3", "4"});
    bi.add_row({"5", "6"});
    bi.execute(conn, q);

    EXPECT_EQ(bi.execute_count(), 1);
    EXPECT_EQ(conn.last_sql, "INSERT INTO ? (a, b) VALUES (?, ?), (?, ?), (?, ?)");
    EXPECT_EQ(conn.last_params,
              (std::vector<std::string>{"1", "2", "3", "4", "5", "6"}));
}

TEST(RvBatchInsert, RowWithWrongWidthThrowsAndSendsNothing)
{
    orm::MockDB conn;
    const auto q = orm::insert(orm::field<&RvRow::a>, orm::field<&RvRow::b>);
    orm::batch_insert<orm::MockDB, decltype(q.signature())> bi;
    bi.add_row({"1", "2"});
    bi.add_row({"3"});
    EXPECT_THROW(bi.execute(conn, q), std::invalid_argument);
    EXPECT_EQ(bi.execute_count(), 0);
    EXPECT_EQ(conn.last_sql, "");
}

// A connector without supports_multi_row_insert would drop all rows but the
// first in a multi-row call. batch_insert sends one call per row instead.
struct RvOneRowDB { std::vector<std::vector<std::string>> calls; };

namespace orm
{
    template <>
    struct connector_trait<RvOneRowDB>
    {
        template <typename T> struct wire_type { using type = T; };
        struct cursor_type {};

        template <typename Properties>
        static auto execute(RvOneRowDB& db, insert_query<Properties> q,
                            const std::vector<std::string>& row)
            -> result<std::tuple<>>
        {
            EXPECT_EQ(q.rows_, 1u);
            db.calls.push_back(row);
            return result<std::tuple<>>{};
        }
    };
} // namespace orm

TEST(RvBatchInsert, ConnectorWithoutMultiRowGetsOneCallPerRow)
{
    RvOneRowDB conn;
    const auto q = orm::insert(orm::field<&RvRow::a>, orm::field<&RvRow::b>);
    orm::batch_insert<RvOneRowDB, decltype(q.signature())> bi;
    bi.add_row({"1", "2"});
    bi.add_row({"3", "4"});
    bi.execute(conn, q);

    EXPECT_EQ(bi.execute_count(), 2);
    ASSERT_EQ(conn.calls.size(), 2u);
    EXPECT_EQ(conn.calls[1], (std::vector<std::string>{"3", "4"}));
}

// ── async_db: one connection runs one query at a time ─────────────────────────
// The connector counts queries that run at the same time. It declares
// supports_concurrent_execute (one connection per thread), which does not make
// a single connection safe to share.

struct RvOverlapDB
{
    std::atomic<int> running{0};
    std::atomic<int> most{0};
};

struct RvSharedDB : RvOverlapDB {};

namespace orm
{
    namespace rv_detail
    {
        template <typename DB, typename Properties>
        auto overlap_execute(DB& db, insert_query<Properties>) -> result<std::tuple<>>
        {
            const int now = ++db.running;
            int seen = db.most.load();
            while (now > seen && !db.most.compare_exchange_weak(seen, now)) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            --db.running;
            return result<std::tuple<>>{};
        }
    } // namespace rv_detail

    template <>
    struct connector_trait<RvOverlapDB>
    {
        using supports_concurrent_execute = void;
        template <typename T> struct wire_type { using type = T; };
        struct cursor_type {};

        template <typename Properties, typename... Params>
        static auto execute(RvOverlapDB& db, insert_query<Properties> q, Params&&...)
            -> result<std::tuple<>>
        {
            return rv_detail::overlap_execute(db, q);
        }
    };

    template <>
    struct connector_trait<RvSharedDB>
    {
        using supports_shared_connection = void;
        template <typename T> struct wire_type { using type = T; };
        struct cursor_type {};

        template <typename Properties, typename... Params>
        static auto execute(RvSharedDB& db, insert_query<Properties> q, Params&&...)
            -> result<std::tuple<>>
        {
            return rv_detail::overlap_execute(db, q);
        }
    };
} // namespace orm

template <typename DB>
static int rv_most_overlap(DB& conn)
{
    orm::ThreadPool pool{4};
    orm::async_db<DB> adb(conn, pool);
    constexpr auto q = orm::insert(orm::field<&RvRow::a>);
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t)
        threads.emplace_back([&] {
            for (int i = 0; i < 10; ++i)
                adb.async_execute(q, 1).sync_wait();
        });
    for (auto& th : threads) th.join();
    return conn.most.load();
}

TEST(RvAsyncSerialisation, ConnectorWithoutSharedTagRunsOneQueryAtATime)
{
    RvOverlapDB conn;
    EXPECT_EQ(rv_most_overlap(conn), 1);
}

TEST(RvAsyncSerialisation, ConnectorWithSharedTagIsNotLocked)
{
    RvSharedDB conn;
    EXPECT_GT(rv_most_overlap(conn), 1);
}

// ── timers: one queue per context, no thread per timer ────────────────────────

TEST(RvTimers, ManyTimersFireInDeadlineOrder)
{
    constexpr int kCount = 200;
    auto ctx = orm::IoContext::create(1);

    // Added out of order. The deadline of timer i is i milliseconds.
    std::vector<int> add_order(kCount);
    std::iota(add_order.begin(), add_order.end(), 0);
    std::reverse(add_order.begin(), add_order.end());
    for (std::size_t i = 0; i + 1 < add_order.size(); i += 2)
        std::swap(add_order[i], add_order[i + 1]);

    std::vector<int> fired;
    for (int i : add_order)
        ctx->schedule(std::chrono::milliseconds(i), [&fired, i, &ctx] {
            fired.push_back(i);
            if (i == kCount - 1) ctx->stop();
        });

    std::thread loop([&] { ctx->run(); });
    loop.join();

    std::vector<int> expected(kCount);
    std::iota(expected.begin(), expected.end(), 0);
    EXPECT_EQ(fired, expected);
}

TEST(RvTimers, TimerMayScheduleAnotherTimer)
{
    auto ctx = orm::IoContext::create(1);
    int stage = 0;
    ctx->schedule(std::chrono::milliseconds(1), [&] {
        stage = 1;
        ctx->schedule(std::chrono::milliseconds(1), [&] {
            stage = 2;
            ctx->stop();
        });
    });
    std::thread loop([&] { ctx->run(); });
    loop.join();
    EXPECT_EQ(stage, 2);
}

TEST(RvTimers, DestroyingContextWithPendingTimersCancelsThem)
{
    auto witness = std::make_shared<int>(0);
    std::weak_ptr<int> watch = witness;
    bool ran = false;
    {
        auto ctx = orm::IoContext::create(1);
        for (int i = 0; i < 50; ++i)
            ctx->schedule(std::chrono::seconds(60), [witness, &ran] { ran = true; });
    }
    witness.reset();
    // The context dropped its copies of the callback when it was destroyed.
    EXPECT_TRUE(watch.expired());
    EXPECT_FALSE(ran);
}

TEST(RvTimers, DestroyingRunningContextWithPendingTimersIsClean)
{
    auto ctx = orm::IoContext::create(1);
    ctx->schedule(std::chrono::seconds(60), [] {});
    std::thread loop([&] { ctx->run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ctx->stop();
    loop.join();
    ctx.reset();
    // Destroying a context that just stopped must not leave a thread behind.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    SUCCEED();
}

// ── MongoDB mock renderer: && is $and, || is $or ──────────────────────────────

TEST(RvMongoRules, AndRuleRendersAsAnd)
{
    using namespace std::placeholders;
    orm::MongoDB conn;
    orm::db<orm::MongoDB> mdb{conn};
    constexpr auto q = orm::select(orm::field<&RvRow::a>)
        .where(orm::field<&RvRow::a> == orm::ph<int, _1>
               && orm::field<&RvRow::b> > orm::ph<int, _2>);
    mdb.execute(q, 1, 2);
    EXPECT_EQ(conn.coll.last_filter,
              "{\"$and\":[{\"a\":{\"$eq\":\"?\"}},{\"b\":{\"$gt\":\"?\"}}]}");
}

TEST(RvMongoRules, OrRuleRendersAsOr)
{
    using namespace std::placeholders;
    orm::MongoDB conn;
    orm::db<orm::MongoDB> mdb{conn};
    constexpr auto q = orm::select(orm::field<&RvRow::a>)
        .where(orm::field<&RvRow::a> == orm::ph<int, _1>
               || orm::field<&RvRow::b> > orm::ph<int, _2>);
    mdb.execute(q, 1, 2);
    EXPECT_EQ(conn.coll.last_filter,
              "{\"$or\":[{\"a\":{\"$eq\":\"?\"}},{\"b\":{\"$gt\":\"?\"}}]}");
}
