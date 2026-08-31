#include "ORM/async/task.hpp"
#include "ORM/async/io_context.hpp"

namespace orm::detail {

    ORM_AWAITER_NOINLINE std::coroutine_handle<> task_final_suspend_resume(
        bool is_detached,
        std::coroutine_handle<> continuation,
        std::function<void()> on_done,
        std::coroutine_handle<> frame) noexcept
    {
        if (on_done)
        {
            on_done();
        }
        if (is_detached)
        {
            frame.destroy();
            return std::noop_coroutine();
        }
        if (continuation)
        {
            return continuation;
        }
        return std::noop_coroutine();
    }

} // namespace orm::detail

namespace orm {

    // Out-of-line so ORM_AWAITER_NOINLINE is a real call (see io_context.hpp).
    ORM_AWAITER_NOINLINE void
    PollAwaitable::await_suspend(std::coroutine_handle<> h) noexcept
    {
        op->continuation = h;
        ctx->register_poll(fd, op.get());
    }

} // namespace orm
