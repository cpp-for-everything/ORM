#include <gtest/gtest.h>
#include "ORM/async/thread_pool.hpp"
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

TEST(ThreadPoolTest, CreateAndShutdown)
{
    orm::ThreadPool pool(2);
    EXPECT_EQ(pool.thread_count(), 2u);
    EXPECT_FALSE(pool.is_stopped());
    pool.shutdown();
    EXPECT_TRUE(pool.is_stopped());
}

TEST(ThreadPoolTest, PostExecutes)
{
    orm::ThreadPool pool(1);
    std::atomic<bool> called{false};
    pool.post([&] { called = true; });
    pool.shutdown();
    EXPECT_TRUE(called.load());
}

TEST(ThreadPoolTest, PostMultipleTasks)
{
    orm::ThreadPool pool(4);
    std::atomic<int> count{0};
    constexpr int n = 100;
    for (int i = 0; i < n; ++i)
    {
        pool.post([&] { count.fetch_add(1, std::memory_order_relaxed); });
    }
    pool.shutdown();
    EXPECT_EQ(count.load(), n);
}

TEST(ThreadPoolTest, TasksRunOnDifferentThreads)
{
    orm::ThreadPool pool(4);
    std::mutex mtx;
    std::set<std::thread::id> ids;

    constexpr int n = 20;
    std::atomic<int> done{0};
    for (int i = 0; i < n; ++i)
    {
        pool.post([&] {
            auto id = std::this_thread::get_id();
            {
                std::lock_guard<std::mutex> lock(mtx);
                ids.insert(id);
            }
            // Sleep to prevent one thread from draining the entire queue
            // before other pool threads wake up (flaky under heavy load)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            done.fetch_add(1);
        });
    }
    pool.shutdown();
    EXPECT_EQ(done.load(), n);
    EXPECT_GT(ids.size(), 1u);
}

TEST(ThreadPoolTest, DoubleShutdownIsSafe)
{
    orm::ThreadPool pool(1);
    pool.shutdown();
    pool.shutdown();
    EXPECT_TRUE(pool.is_stopped());
}

// ── run_on_pool coroutine tests ─────────────────────────────────────────────

TEST(RunOnPoolTest, NonVoidReturn)
{
    orm::ThreadPool pool(2);
    // Keep the coroutine-lambda closure alive across sync_wait() — see
    // RunOnPoolTest.ExceptionPropagates / TaskTest.VoidTaskCompletes.
    auto coro = [&]() -> orm::Task<int> {
        co_return co_await orm::run_on_pool(pool, [] { return 42; });
    };
    auto task = coro();
    EXPECT_EQ(task.sync_wait(), 42);
    pool.shutdown();
}

TEST(RunOnPoolTest, StringReturn)
{
    orm::ThreadPool pool(2);
    auto coro = [&]() -> orm::Task<std::string> {
        co_return co_await orm::run_on_pool(pool, [] {
            return std::string("hello from pool");
        });
    };
    auto task = coro();
    EXPECT_EQ(task.sync_wait(), "hello from pool");
    pool.shutdown();
}

TEST(RunOnPoolTest, VoidReturn)
{
    orm::ThreadPool pool(2);
    std::atomic<bool> called{false};
    auto coro = [&]() -> orm::Task<void> {
        co_await orm::run_on_pool(pool, [&] { called = true; });
        co_return;
    };
    auto task = coro();
    task.sync_wait();
    EXPECT_TRUE(called.load());
    pool.shutdown();
}

TEST(RunOnPoolTest, ExceptionPropagates)
{
    orm::ThreadPool pool(2);
    // Hold the coroutine-lambda closure alive across sync_wait(): an immediately-
    // invoked [&]{...}() destroys its closure at the end of the full expression, but
    // the suspended coroutine still reads its captures when resumed → stack-use-after-
    // scope (crashes on libc++/macOS in the exception-unwind path; latent on glibc).
    auto coro = [&]() -> orm::Task<int> {
        co_return co_await orm::run_on_pool(pool, []() -> int {
            throw std::runtime_error("pool boom");
        });
    };
    auto task = coro();
    EXPECT_THROW(task.sync_wait(), std::runtime_error);
    pool.shutdown();
}

TEST(RunOnPoolTest, VoidExceptionPropagates)
{
    orm::ThreadPool pool(2);
    auto coro = [&]() -> orm::Task<void> {        // keep closure alive — see ExceptionPropagates
        co_await orm::run_on_pool(pool, [] {
            throw std::logic_error("void boom");
        });
        co_return;
    };
    auto task = coro();
    EXPECT_THROW(task.sync_wait(), std::logic_error);
    pool.shutdown();
}

TEST(RunOnPoolTest, RunsOnPoolThread)
{
    orm::ThreadPool pool(2);
    auto main_id = std::this_thread::get_id();
    auto coro = [&]() -> orm::Task<std::thread::id> {
        co_return co_await orm::run_on_pool(pool, [] {
            return std::this_thread::get_id();
        });
    };
    auto task = coro();
    auto pool_id = task.sync_wait();
    EXPECT_NE(pool_id, main_id);
    pool.shutdown();
}

TEST(RunOnPoolTest, SequentialOffloads)
{
    orm::ThreadPool pool(2);
    auto coro = [&]() -> orm::Task<int> {
        int a = co_await orm::run_on_pool(pool, [] { return 10; });
        int b = co_await orm::run_on_pool(pool, [] { return 20; });
        int c = co_await orm::run_on_pool(pool, [] { return 12; });
        co_return a + b + c;
    };
    auto task = coro();
    EXPECT_EQ(task.sync_wait(), 42);
    pool.shutdown();
}
