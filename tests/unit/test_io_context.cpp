#include <gtest/gtest.h>
#include "ORM/async/io_context.hpp"
#include "ORM/async/task.hpp"
#include <atomic>
#include <thread>
#include <unistd.h>

TEST(IoContextTest, CreateSucceeds)
{
    auto ctx = orm::IoContext::create(1);
    ASSERT_NE(ctx, nullptr);
}

TEST(IoContextTest, PostCallbackExecutes)
{
    auto ctx = orm::IoContext::create(1);
    std::atomic<bool> called{false};
    ctx->post([&] {
        called = true;
        ctx->stop();
    });

    std::thread t([&] { ctx->run(); });
    t.join();

    EXPECT_TRUE(called.load());
}

TEST(IoContextTest, StopBreaksRunLoop)
{
    auto ctx = orm::IoContext::create(1);
    ctx->post([&] { ctx->stop(); });
    ctx->run();
    EXPECT_TRUE(ctx->stopped());
}

TEST(IoContextTest, RunOneProcessesSingleCallback)
{
    auto ctx = orm::IoContext::create(1);
    std::atomic<int> count{0};
    ctx->post([&] { count++; });
    ctx->post([&] { count++; });
    ctx->run_one();
    // run_one processes events + one callback
    EXPECT_GE(count.load(), 1);
}

TEST(IoContextTest, WatchReadableWithPipe)
{
    auto ctx = orm::IoContext::create(1);
    int pipefd[2];
    ASSERT_EQ(pipe(pipefd), 0);

    std::atomic<bool> ready{false};

    auto coro = [&]() -> orm::Task<void> {
        co_await ctx->watch_readable(pipefd[0]);
        ready.store(true, std::memory_order_release);
        ctx->stop();
        co_return;
    }();
    coro.start_detached();

    // Write to pipe after a short delay to make read end readable
    ctx->post([&] {
        char buf = 'x';
        [[maybe_unused]] auto written = write(pipefd[1], &buf, 1);
    });

    ctx->run();

    EXPECT_TRUE(ready.load());
    close(pipefd[0]);
    close(pipefd[1]);
}

TEST(IoContextTest, WatchWritableWithPipe)
{
    auto ctx = orm::IoContext::create(1);
    int pipefd[2];
    ASSERT_EQ(pipe(pipefd), 0);

    std::atomic<bool> ready{false};

    // Pipes are typically writable immediately
    auto coro = [&]() -> orm::Task<void> {
        co_await ctx->watch_writable(pipefd[1]);
        ready.store(true, std::memory_order_release);
        ctx->stop();
        co_return;
    }();
    coro.start_detached();

    ctx->run();

    EXPECT_TRUE(ready.load());
    close(pipefd[0]);
    close(pipefd[1]);
}

TEST(IoContextTest, MultipleWatchesSequential)
{
    auto ctx = orm::IoContext::create(1);
    int pipefd[2];
    ASSERT_EQ(pipe(pipefd), 0);

    std::atomic<int> step{0};

    auto coro = [&]() -> orm::Task<void> {
        // First watch: writable (pipe write end is immediately writable)
        co_await ctx->watch_writable(pipefd[1]);
        step.store(1, std::memory_order_release);

        // Write something so read end becomes readable
        char buf = 'y';
        [[maybe_unused]] auto written = write(pipefd[1], &buf, 1);

        // Second watch: readable
        co_await ctx->watch_readable(pipefd[0]);
        step.store(2, std::memory_order_release);

        ctx->stop();
        co_return;
    }();
    coro.start_detached();

    ctx->run();

    EXPECT_EQ(step.load(), 2);
    close(pipefd[0]);
    close(pipefd[1]);
}

TEST(IoContextTest, ScheduleDelayedCallback)
{
    auto ctx = orm::IoContext::create(1);
    std::atomic<bool> called{false};

    ctx->schedule(std::chrono::milliseconds(50), [&] {
        called = true;
        ctx->stop();
    });

    ctx->run();

    EXPECT_TRUE(called.load());
}
