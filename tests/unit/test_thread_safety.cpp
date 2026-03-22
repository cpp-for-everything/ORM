#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MockDB/mock_db.hpp"
#include "ORM/db/connectors/ThreadSafety/thread_safety.hpp"
#include <thread>
#include <atomic>
#include <chrono>

// ── connection_pool static_assert gate ────────────────────────────────────────
// Verify that instantiation of connection_pool<MockDB, 1> compiles
// (MockDB now declares supports_concurrent_execute).

TEST(ThreadSafety, PoolInstantiatesWithSupportedConnector)
{
    orm::connection_pool<orm::MockDB, 2> pool;
    (void)pool;
    SUCCEED();
}

// ── Pool acquire / release ────────────────────────────────────────────────────

TEST(ThreadSafety, PoolAcquireReturnsGuard)
{
    orm::connection_pool<orm::MockDB, 1> pool;
    auto guard = pool.acquire();
    auto dbc   = guard.get();
    (void)dbc;
    SUCCEED(); // guard destructor releases automatically
}

// ── Pool N=1: second thread blocks until first guard is destroyed ─────────────

TEST(ThreadSafety, PoolN1SecondThreadBlocksUntilRelease)
{
    orm::connection_pool<orm::MockDB, 1> pool;

    std::atomic<bool> second_acquired{false};

    // Thread A holds the guard for a short while
    auto guard_a = pool.acquire();

    std::thread thread_b([&]()
    {
        auto guard_b = pool.acquire(); // blocks until A releases
        second_acquired.store(true);
    });

    // Give thread B time to start and block on acquire()
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(second_acquired.load())
        << "Thread B must not have acquired the guard yet — pool size is 1";

    // Release guard A — thread B should now unblock
    {
        auto released = std::move(guard_a);
        (void)released;
    } // guard_a destructor called here

    thread_b.join();
    EXPECT_TRUE(second_acquired.load())
        << "Thread B must have acquired the guard after A released it";
}

// ── transaction_guard auto-rollback ──────────────────────────────────────────

TEST(ThreadSafety, TransactionAutoRollback)
{
    orm::MockDB db;
    {
        auto txn = orm::begin_transaction(db);
        // Do not call commit() — destructor should issue ROLLBACK
        EXPECT_EQ(db.last_sql, "BEGIN");
    } // txn destructor → ROLLBACK
    EXPECT_EQ(db.last_sql, "ROLLBACK")
        << "transaction_guard destructor must issue ROLLBACK when commit() not called";
}

// ── transaction_guard explicit commit ────────────────────────────────────────

TEST(ThreadSafety, TransactionCommit)
{
    orm::MockDB db;
    {
        auto txn = orm::begin_transaction(db);
        EXPECT_EQ(db.last_sql, "BEGIN");
        txn.commit();
        EXPECT_EQ(db.last_sql, "COMMIT");
    } // destructor — should NOT issue ROLLBACK since committed
    EXPECT_EQ(db.last_sql, "COMMIT")
        << "last_sql must remain COMMIT after committed transaction_guard destruction";
}

// ── thread_local_db independence ─────────────────────────────────────────────

TEST(ThreadSafety, ThreadLocalIndependentPerThread)
{
    orm::MockDB* addr_main = &orm::thread_local_db<orm::MockDB>::connection();

    orm::MockDB* addr_other = nullptr;
    std::thread t([&]()
    {
        addr_other = &orm::thread_local_db<orm::MockDB>::connection();
    });
    t.join();

    EXPECT_NE(addr_main, addr_other)
        << "thread_local_db must return a distinct connection per thread";
}

// ── capability_check static_assert ───────────────────────────────────────────

TEST(ThreadSafety, SupportsConcurrentExecutePresent)
{
    static_assert(
        requires { typename orm::connector_trait<orm::MockDB>::supports_concurrent_execute; },
        "MockDB must declare supports_concurrent_execute");
}
