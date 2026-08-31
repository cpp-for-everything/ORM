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

    class IoContext;

    // ── Awaitable returned by watch_readable / watch_writable ───────────────
    //
    // The PollOperation is heap-allocated and owned by the awaitable, which
    // is itself owned by the caller's coroutine frame for the duration of
    // the co_await expression. The reason for the heap allocation is subtle
    // and worth recording: an earlier design embedded PollOperation directly
    // as a value member, and GCC at -O2 and above could speculate the
    // awaitable's address into a register rather than into the coroutine
    // frame, leaving `&op` (which we pass into register_poll) dangling once
    // await_suspend returned. The heap allocation forces a stable address
    // that survives any optimisation of the awaitable's storage.
    //
    // Ordering inside await_suspend: continuation is assigned BEFORE the
    // reactor sees the registration. A reactor that fires immediately (e.g.
    // a socket already writable) and races on another thread must always
    // observe a valid continuation handle, otherwise the wake-up would be
    // dropped.
    class PollAwaitable
    {
    public:
        IoContext* ctx;
        int fd;
        std::unique_ptr<PollOperation> op;

        PollAwaitable(IoContext* c, int f, PollInterest interest)
            : ctx(c), fd(f), op(std::make_unique<PollOperation>(interest))
        {
        }

        PollAwaitable(const PollAwaitable&) = delete;
        PollAwaitable& operator=(const PollAwaitable&) = delete;
        PollAwaitable(PollAwaitable&&) = default;
        PollAwaitable& operator=(PollAwaitable&&) = default;

        bool await_ready() const noexcept { return false; }

        ORM_AWAITER_NOINLINE void await_suspend(std::coroutine_handle<> h) noexcept;

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

        // Awaitable wrappers. Returning PollAwaitable by value keeps the
        // awaiter object in the caller's coroutine frame for the duration
        // of the co_await expression — exactly the lifetime semantics the
        // language already guarantees, with no inner coroutine.
        [[nodiscard]] PollAwaitable watch_readable(int fd) noexcept
        {
            return PollAwaitable{this, fd, PollInterest::Read};
        }

        [[nodiscard]] PollAwaitable watch_writable(int fd) noexcept
        {
            return PollAwaitable{this, fd, PollInterest::Write};
        }

        // Factory — creates platform-appropriate context
        [[nodiscard]] static auto create(size_t thread_count = 1)
            -> std::unique_ptr<IoContext>;
    };

} // namespace orm
