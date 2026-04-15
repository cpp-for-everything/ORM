#pragma once

#include "ORM/async/task.hpp"
#include <chrono>
#include <coroutine>
#include <functional>
#include <memory>

namespace orm {

    // ── Poll interest for reactor-mode fd watching ──────────────────────────
    enum class PollInterest
    {
        Read,
        Write
    };

    // ── Reactor operation: just tracks coroutine continuation ───────────────
    struct PollOperation
    {
        PollInterest interest;
        std::coroutine_handle<> continuation;

        explicit PollOperation(PollInterest pi) noexcept : interest(pi) {}
    };

    // ── Awaiter for watch_readable / watch_writable ─────────────────────────
    struct PollAwaiter
    {
        PollOperation& op;

        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) noexcept
        {
            op.continuation = h;
        }

        void await_resume() const noexcept {}
    };

    // ── Abstract IoContext ──────────────────────────────────────────────────
    //
    // Minimal event loop for ORM async connectors.
    // Supports:
    //   - run() / run_one() / stop() lifecycle
    //   - post(callback) for cross-thread dispatch
    //   - schedule(delay, callback) for timeouts
    //   - watch_readable(fd) / watch_writable(fd) — reactor awaitables
    //     that resume a coroutine when the fd is ready WITHOUT doing I/O
    //
    class IoContext
    {
    public:
        virtual ~IoContext() = default;

        virtual void run() = 0;
        virtual void run_one() = 0;
        virtual void stop() = 0;
        [[nodiscard]] virtual bool stopped() const noexcept = 0;

        virtual void post(std::function<void()> callback) = 0;
        virtual void schedule(
            std::chrono::milliseconds delay,
            std::function<void()> callback) = 0;

        // Reactor: register fd for readiness notification.
        // The awaiter suspends the coroutine; the event loop resumes it
        // when the fd becomes readable/writable. No I/O is performed —
        // the caller's database library does its own read/write after
        // resumption.
        virtual void register_poll(int fd, PollOperation* op) = 0;

        // Convenience coroutine wrappers
        [[nodiscard]] auto watch_readable(int fd) -> Task<void>
        {
            PollOperation op{PollInterest::Read};
            register_poll(fd, &op);
            co_await PollAwaiter{op};
            co_return;
        }

        [[nodiscard]] auto watch_writable(int fd) -> Task<void>
        {
            PollOperation op{PollInterest::Write};
            register_poll(fd, &op);
            co_await PollAwaiter{op};
            co_return;
        }

        // Factory — creates platform-appropriate context
        [[nodiscard]] static auto create(size_t thread_count = 1)
            -> std::unique_ptr<IoContext>;
    };

} // namespace orm
