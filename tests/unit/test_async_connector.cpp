#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MockDB/mock_db.hpp"
#include "ORM/db/connectors/ThreadSafety/async_thread_safety.hpp"
#include "ORM/connector/async_db.hpp"
#include "ORM/async/thread_pool.hpp"
#include "ORM/async/task.hpp"
#include <atomic>
#include <string>
#include <thread>

// ── Test model ──────────────────────────────────────────────────────────────
struct AsyncUser
{
    orm::property<int, "id"> id;
    orm::property<std::u8string, "name"> name;
};

namespace orm {
    template <>
    struct table_name_trait<AsyncUser>
    {
        static constexpr std::string_view value = "async_users";
    };
} // namespace orm

// ── async_db tests ──────────────────────────────────────────────────────────

class AsyncDbTest : public ::testing::Test
{
protected:
    orm::MockDB conn;
    orm::ThreadPool pool{2};
};

TEST_F(AsyncDbTest, AsyncSelectViaOperator)
{
    orm::async_db<orm::MockDB> adb(conn, pool);
    constexpr auto q = orm::select(orm::field<&AsyncUser::id>);

    auto task = [&]() -> orm::Task<void> {
        auto result = co_await (adb << q);
        co_return;
    }();
    task.sync_wait();

    EXPECT_TRUE(conn.last_sql.starts_with("SELECT"));
}

TEST_F(AsyncDbTest, AsyncSelectRunsOnPoolThread)
{
    orm::async_db<orm::MockDB> adb(conn, pool);
    constexpr auto q = orm::select(orm::field<&AsyncUser::id>);
    auto main_id = std::this_thread::get_id();

    auto task = [&]() -> orm::Task<std::thread::id> {
        [[maybe_unused]] auto result = co_await (adb << q);
        co_return std::this_thread::get_id();
    }();
    auto exec_id = task.sync_wait();

    EXPECT_NE(exec_id, main_id);
}

TEST_F(AsyncDbTest, AsyncExecuteWithParams)
{
    orm::async_db<orm::MockDB> adb(conn, pool);
    constexpr auto q = orm::insert(orm::field<&AsyncUser::id>, orm::field<&AsyncUser::name>);

    auto task = [&]() -> orm::Task<void> {
        co_await adb.async_execute(q, 42, u8"alice");
        co_return;
    }();
    task.sync_wait();

    EXPECT_TRUE(conn.last_sql.starts_with("INSERT"));
    ASSERT_EQ(conn.last_params.size(), 2u);
    EXPECT_EQ(conn.last_params[0], "42");
    EXPECT_EQ(conn.last_params[1], "alice");
}

TEST_F(AsyncDbTest, SyncHandleAccessible)
{
    orm::async_db<orm::MockDB> adb(conn, pool);
    auto& sync = adb.sync_handle();
    constexpr auto q = orm::select(orm::field<&AsyncUser::id>);
    sync << q;
    EXPECT_TRUE(conn.last_sql.starts_with("SELECT"));
}

TEST_F(AsyncDbTest, MultipleSequentialAsyncQueries)
{
    orm::async_db<orm::MockDB> adb(conn, pool);

    auto task = [&]() -> orm::Task<void> {
        constexpr auto q1 = orm::select(orm::field<&AsyncUser::id>);
        [[maybe_unused]] auto r1 = co_await (adb << q1);

        constexpr auto q2 = orm::select(orm::field<&AsyncUser::name>);
        [[maybe_unused]] auto r2 = co_await (adb << q2);
        co_return;
    }();
    task.sync_wait();

    EXPECT_TRUE(conn.last_sql.starts_with("SELECT"));
}

// ── async_connection_pool tests ─────────────────────────────────────────────

TEST(AsyncConnectionPoolTest, AcquireReturnsGuard)
{
    orm::ThreadPool pool(2);
    orm::async_connection_pool<orm::MockDB, 2> apool(pool);

    auto task = [&]() -> orm::Task<void> {
        auto guard = co_await apool.acquire();
        auto adb = guard.get();
        co_return;
    }();
    task.sync_wait();
}

TEST(AsyncConnectionPoolTest, SyncAcquireWorks)
{
    orm::ThreadPool pool(2);
    orm::async_connection_pool<orm::MockDB, 2> apool(pool);

    auto guard = apool.acquire_sync();
    auto adb = guard.get();
    (void)adb;
}

TEST(AsyncConnectionPoolTest, GuardReleasesOnDestruction)
{
    orm::ThreadPool pool(2);
    orm::async_connection_pool<orm::MockDB, 1> apool(pool);

    auto task = [&]() -> orm::Task<void> {
        {
            auto guard = co_await apool.acquire();
            (void)guard;
        }
        // Should be able to acquire again after guard is destroyed
        {
            auto guard2 = co_await apool.acquire();
            (void)guard2;
        }
        co_return;
    }();
    task.sync_wait();
}

TEST(AsyncConnectionPoolTest, ConcurrentAcquires)
{
    orm::ThreadPool pool(4);
    orm::async_connection_pool<orm::MockDB, 2> apool(pool);
    std::atomic<int> active{0};
    std::atomic<int> max_active{0};

    auto worker = [&]() -> orm::Task<void> {
        auto guard = co_await apool.acquire();
        int cur = active.fetch_add(1) + 1;
        int prev_max = max_active.load();
        while (cur > prev_max && !max_active.compare_exchange_weak(prev_max, cur))
            ;
        // Simulate some work
        std::this_thread::yield();
        active.fetch_sub(1);
        co_return;
    };

    constexpr int n = 8;
    std::vector<orm::Task<void>> tasks;
    tasks.reserve(n);
    for (int i = 0; i < n; ++i)
    {
        tasks.push_back(worker());
    }
    for (auto& t : tasks)
    {
        t.sync_wait();
    }

    EXPECT_LE(max_active.load(), 2);
}

// ── async_transaction_guard tests ───────────────────────────────────────────

TEST(AsyncTransactionTest, BeginAndCommit)
{
    orm::ThreadPool pool(2);
    orm::MockDB conn;

    auto task = [&]() -> orm::Task<void> {
        auto txn = co_await orm::async_begin_transaction(conn, pool);
        EXPECT_EQ(conn.last_sql, "BEGIN");

        co_await txn.commit();
        EXPECT_EQ(conn.last_sql, "COMMIT");
        EXPECT_TRUE(txn.is_committed());
        co_return;
    }();
    task.sync_wait();
}

TEST(AsyncTransactionTest, RollbackOnDestruction)
{
    orm::ThreadPool pool(2);
    orm::MockDB conn;

    auto task = [&]() -> orm::Task<void> {
        {
            auto txn = co_await orm::async_begin_transaction(conn, pool);
            EXPECT_EQ(conn.last_sql, "BEGIN");
            // txn destructs without commit → ROLLBACK
        }
        EXPECT_EQ(conn.last_sql, "ROLLBACK");
        co_return;
    }();
    task.sync_wait();
}

TEST(AsyncTransactionTest, ExplicitRollback)
{
    orm::ThreadPool pool(2);
    orm::MockDB conn;

    auto task = [&]() -> orm::Task<void> {
        auto txn = co_await orm::async_begin_transaction(conn, pool);
        co_await txn.rollback();
        EXPECT_EQ(conn.last_sql, "ROLLBACK");
        EXPECT_TRUE(txn.is_committed());
        co_return;
    }();
    task.sync_wait();
}

TEST(AsyncTransactionTest, IsAsyncConnectorConceptFalseForMockDB)
{
    // MockDB does not declare supports_async, so is_async_connector should be false
    static_assert(!orm::is_async_connector<orm::MockDB>);
}
