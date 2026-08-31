#include <gtest/gtest.h>
#include "ORM/async/io_context.hpp"
#include "ORM/async/task.hpp"
#include <atomic>
#include <thread>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
namespace {
    // Connected loopback TCP pair as a portable substitute for POSIX pipes
    // when running tests against the Windows reactor (WSAPoll only accepts
    // sockets — not anonymous pipes — so the test must use sockets too).
    inline bool make_pipe_pair(int& read_fd, int& write_fd)
    {
        SOCKET listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == INVALID_SOCKET) return false;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        if (::bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
        {
            ::closesocket(listener);
            return false;
        }
        int alen = sizeof(addr);
        if (::getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &alen) != 0)
        {
            ::closesocket(listener);
            return false;
        }
        if (::listen(listener, 1) != 0)
        {
            ::closesocket(listener);
            return false;
        }

        SOCKET writer = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (writer == INVALID_SOCKET)
        {
            ::closesocket(listener);
            return false;
        }
        if (::connect(writer, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
        {
            ::closesocket(listener);
            ::closesocket(writer);
            return false;
        }

        SOCKET reader = ::accept(listener, nullptr, nullptr);
        ::closesocket(listener);
        if (reader == INVALID_SOCKET)
        {
            ::closesocket(writer);
            return false;
        }

        read_fd = static_cast<int>(reader);
        write_fd = static_cast<int>(writer);
        return true;
    }

    inline void close_fd(int fd) { ::closesocket(static_cast<SOCKET>(fd)); }
    inline int write_one(int fd, char b)
    {
        return ::send(static_cast<SOCKET>(fd), &b, 1, 0);
    }

    struct WinsockGuard
    {
        WinsockGuard()
        {
            WSADATA d{};
            ::WSAStartup(MAKEWORD(2, 2), &d);
        }
        ~WinsockGuard() { ::WSACleanup(); }
    };
    WinsockGuard g_winsock_guard;
}
#else
#include <unistd.h>
namespace {
    inline bool make_pipe_pair(int& read_fd, int& write_fd)
    {
        int pfd[2];
        if (::pipe(pfd) != 0) return false;
        read_fd = pfd[0];
        write_fd = pfd[1];
        return true;
    }
    inline void close_fd(int fd) { ::close(fd); }
    inline int write_one(int fd, char b)
    {
        return static_cast<int>(::write(fd, &b, 1));
    }
}
#endif

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
    int read_fd = -1, write_fd = -1;
    ASSERT_TRUE(make_pipe_pair(read_fd, write_fd));

    std::atomic<bool> ready{false};

    // Keep the coroutine-lambda closure alive: IIFE [&]{...}() frees captures
    // before start_detached()/run() resume the frame (SEGFAULT under GCC 15 -O3).
    auto coro_fn = [&]() -> orm::Task<void> {
        co_await ctx->watch_readable(read_fd);
        ready.store(true, std::memory_order_release);
        ctx->stop();
        co_return;
    };
    auto coro = coro_fn();
    coro.start_detached();

    // Write to pipe after a short delay to make read end readable
    ctx->post([&] { (void)write_one(write_fd, 'x'); });

    ctx->run();

    EXPECT_TRUE(ready.load());
    close_fd(read_fd);
    close_fd(write_fd);
}

TEST(IoContextTest, WatchWritableWithPipe)
{
    auto ctx = orm::IoContext::create(1);
    int read_fd = -1, write_fd = -1;
    ASSERT_TRUE(make_pipe_pair(read_fd, write_fd));

    std::atomic<bool> ready{false};

    // Both endpoints of an empty TCP / pipe pair are immediately writable.
    auto coro_fn = [&]() -> orm::Task<void> {
        co_await ctx->watch_writable(write_fd);
        ready.store(true, std::memory_order_release);
        ctx->stop();
        co_return;
    };
    auto coro = coro_fn();
    coro.start_detached();

    ctx->run();

    EXPECT_TRUE(ready.load());
    close_fd(read_fd);
    close_fd(write_fd);
}

TEST(IoContextTest, MultipleWatchesSequential)
{
    auto ctx = orm::IoContext::create(1);
    int read_fd = -1, write_fd = -1;
    ASSERT_TRUE(make_pipe_pair(read_fd, write_fd));

    std::atomic<int> step{0};

    auto coro_fn = [&]() -> orm::Task<void> {
        // First watch: writable
        co_await ctx->watch_writable(write_fd);
        step.store(1, std::memory_order_release);

        // Write something so read end becomes readable
        (void)write_one(write_fd, 'y');

        // Second watch: readable
        co_await ctx->watch_readable(read_fd);
        step.store(2, std::memory_order_release);

        ctx->stop();
        co_return;
    };
    auto coro = coro_fn();
    coro.start_detached();

    ctx->run();

    EXPECT_EQ(step.load(), 2);
    close_fd(read_fd);
    close_fd(write_fd);
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
