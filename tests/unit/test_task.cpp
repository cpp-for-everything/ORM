#include <gtest/gtest.h>
#include "ORM/async/task.hpp"
#include <atomic>
#include <stdexcept>
#include <string>

TEST(TaskTest, VoidTaskCompletes)
{
    bool executed = false;
    // Keep the coroutine-lambda closure alive across sync_wait(): an immediately-
    // invoked [&]{...}() destroys its closure at the end of the full expression, but
    // the coroutine frame still holds a reference to that closure (captures live there).
    auto coro = [&]() -> orm::Task<void> {
        executed = true;
        co_return;
    };
    auto task = coro();
    task.sync_wait();
    EXPECT_TRUE(executed);
}

TEST(TaskTest, IntTaskReturnsValue)
{
    auto task = []() -> orm::Task<int> {
        co_return 42;
    }();
    EXPECT_EQ(task.sync_wait(), 42);
}

TEST(TaskTest, StringTaskReturnsValue)
{
    auto task = []() -> orm::Task<std::string> {
        co_return std::string("hello");
    }();
    EXPECT_EQ(task.sync_wait(), "hello");
}

TEST(TaskTest, NestedAwait)
{
    auto inner = []() -> orm::Task<int> { co_return 7; };
    auto outer_fn = [&]() -> orm::Task<int> {
        int v = co_await inner();
        co_return v * 6;
    };
    auto outer = outer_fn();
    EXPECT_EQ(outer.sync_wait(), 42);
}

TEST(TaskTest, DeepNesting)
{
    auto level3 = []() -> orm::Task<int> { co_return 1; };
    auto level2 = [&]() -> orm::Task<int> {
        int v = co_await level3();
        co_return v + 1;
    };
    auto level1_fn = [&]() -> orm::Task<int> {
        int v = co_await level2();
        co_return v + 1;
    };
    auto level1 = level1_fn();
    EXPECT_EQ(level1.sync_wait(), 3);
}

TEST(TaskTest, ExceptionPropagates)
{
    auto task = []() -> orm::Task<int> {
        throw std::runtime_error("boom");
        co_return 0;
    }();
    EXPECT_THROW(task.sync_wait(), std::runtime_error);
}

TEST(TaskTest, VoidExceptionPropagates)
{
    auto task = []() -> orm::Task<void> {
        throw std::logic_error("oops");
        co_return;
    }();
    EXPECT_THROW(task.sync_wait(), std::logic_error);
}

TEST(TaskTest, MoveOnlyNoDiscard)
{
    static_assert(!std::is_copy_constructible_v<orm::Task<int>>);
    static_assert(std::is_move_constructible_v<orm::Task<int>>);
    static_assert(!std::is_copy_constructible_v<orm::Task<void>>);
    static_assert(std::is_move_constructible_v<orm::Task<void>>);
}

TEST(TaskTest, MoveAssignment)
{
    auto task1 = []() -> orm::Task<int> { co_return 1; }();
    auto task2 = []() -> orm::Task<int> { co_return 2; }();
    task1 = std::move(task2);
    EXPECT_EQ(task1.sync_wait(), 2);
}

TEST(TaskTest, StartDetached)
{
    std::atomic<bool> done{false};
    // Hold the coroutine lambda's closure alive: an immediately-invoked coroutine
    // lambda destroys its closure at the end of the full expression, but a
    // suspended coroutine still references that closure (its captures live there)
    // when start_detached() later resumes the body. Storing it keeps captures valid.
    auto coro = [&]() -> orm::Task<void> {
        done.store(true, std::memory_order_release);
        co_return;
    };
    auto task = coro();
    task.start_detached();
    EXPECT_TRUE(done.load(std::memory_order_acquire));
}

TEST(TaskTest, ValidAndDoneState)
{
    orm::Task<int> empty;
    EXPECT_FALSE(empty.valid());
    EXPECT_FALSE(empty.done());

    auto task = []() -> orm::Task<int> { co_return 99; }();
    EXPECT_TRUE(task.valid());
    EXPECT_FALSE(task.done());

    EXPECT_EQ(task.sync_wait(), 99);
    EXPECT_TRUE(task.done());
}

TEST(TaskTest, DetachReleasesOwnership)
{
    auto task = []() -> orm::Task<void> { co_return; }();
    EXPECT_TRUE(task.valid());
    task.detach();
    EXPECT_FALSE(task.valid());
}

TEST(TaskTest, YieldAwaiter)
{
    auto coro = []() -> orm::Task<int> {
        co_await orm::yield();
        co_return 42;
    };
    auto task = coro();
    EXPECT_EQ(task.sync_wait(), 42);
}

// ── Cancellation Tests ──────────────────────────────────────────────────────

#include "ORM/async/cancellation.hpp"

TEST(CancellationTest, TokenStartsNotCancelled)
{
    orm::CancellationSource source;
    auto token = source.token();
    EXPECT_FALSE(token.is_cancelled());
    EXPECT_TRUE(static_cast<bool>(token));
}

TEST(CancellationTest, CancelPropagates)
{
    orm::CancellationSource source;
    auto token = source.token();
    EXPECT_TRUE(source.cancel());
    EXPECT_TRUE(token.is_cancelled());
    EXPECT_FALSE(static_cast<bool>(token));
}

TEST(CancellationTest, DoubleCancelReturnsFalse)
{
    orm::CancellationSource source;
    EXPECT_TRUE(source.cancel());
    EXPECT_FALSE(source.cancel());
}

TEST(CancellationTest, CallbackInvokedOnCancel)
{
    orm::CancellationSource source;
    auto token = source.token();
    bool called = false;
    token.on_cancel([&] { called = true; });
    EXPECT_FALSE(called);
    source.cancel();
    EXPECT_TRUE(called);
}

TEST(CancellationTest, CallbackInvokedImmediatelyIfAlreadyCancelled)
{
    orm::CancellationSource source;
    source.cancel();
    auto token = source.token();
    bool called = false;
    token.on_cancel([&] { called = true; });
    EXPECT_TRUE(called);
}

TEST(CancellationTest, MultipleCallbacks)
{
    orm::CancellationSource source;
    auto token = source.token();
    int count = 0;
    token.on_cancel([&] { count++; });
    token.on_cancel([&] { count++; });
    token.on_cancel([&] { count++; });
    source.cancel();
    EXPECT_EQ(count, 3);
}

TEST(CancellationTest, GuardCancelsOnDestruction)
{
    orm::CancellationSource source;
    auto token = source.token();
    {
        orm::CancellationGuard guard(source);
    }
    EXPECT_TRUE(token.is_cancelled());
}

TEST(CancellationTest, GuardReleasePreventsCancellation)
{
    orm::CancellationSource source;
    auto token = source.token();
    {
        orm::CancellationGuard guard(source);
        guard.release();
    }
    EXPECT_FALSE(token.is_cancelled());
}

TEST(CancellationTest, GuardMoveTransfersOwnership)
{
    orm::CancellationSource source;
    auto token = source.token();
    {
        orm::CancellationGuard guard1(source);
        orm::CancellationGuard guard2(std::move(guard1));
    }
    EXPECT_TRUE(token.is_cancelled());
}

TEST(CancellationTest, DefaultTokenNotValid)
{
    orm::CancellationToken token;
    EXPECT_FALSE(token.valid());
    EXPECT_FALSE(token.is_cancelled());
}

TEST(CancellationTest, MultipleTokensFromSameSource)
{
    orm::CancellationSource source;
    auto token1 = source.token();
    auto token2 = source.token();
    EXPECT_FALSE(token1.is_cancelled());
    EXPECT_FALSE(token2.is_cancelled());
    source.cancel();
    EXPECT_TRUE(token1.is_cancelled());
    EXPECT_TRUE(token2.is_cancelled());
}

TEST(CancellationTest, NoneTokenNeverCancels)
{
    auto token = orm::CancellationToken::none();
    EXPECT_FALSE(token.valid());
    EXPECT_FALSE(token.is_cancelled());
}
