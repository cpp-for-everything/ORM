#pragma once

#include "ORM/connector/db.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/async/task.hpp"
#include "ORM/async/thread_pool.hpp"
#include <utility>

namespace orm {

    // ── orm::async_db<DB> ──────────────────────────────────────────────────────
    // Async wrapper around orm::db<DB>. Offloads synchronous connector calls
    // to a thread pool, returning Task<result<...>> for coroutine consumption.
    //
    // For connectors that declare supports_async and provide native async_execute,
    // the async path calls that directly. For all other connectors, the sync
    // execute is wrapped in run_on_pool.
    //
    // Usage:
    //   orm::ThreadPool pool(4);
    //   orm::async_db<MyDB> adb(connection, pool);
    //   auto result = co_await (adb << orm::select(...));
    //
    template <typename DB>
        requires is_connector<DB>
    class async_db
    {
    public:
        explicit async_db(DB& connection, ThreadPool& pool)
            : db_(connection), pool_(&pool)
        {
        }

        // ── async operator<< — primary async query dispatch ────────────────────
        // Returns a Task that, when co_awaited, offloads the query to the pool.
        template <typename Query>
        [[nodiscard]] auto operator<<(Query q) -> Task<decltype(std::declval<db<DB>>() << std::declval<Query>())>
        {
            using ResultType = decltype(std::declval<db<DB>>() << std::declval<Query>());

            if constexpr (has_capability<DB, cap::supports_async>)
            {
                co_return co_await connector_trait<DB>::async_execute(
                    db_.connection(), std::move(q));
            }
            else
            {
                co_return co_await run_on_pool(*pool_, [this, q = std::move(q)]() mutable -> ResultType {
                    return db_ << std::move(q);
                });
            }
        }

        // ── async execute with explicit parameters ─────────────────────────────
        template <typename Query, typename... Params>
        [[nodiscard]] auto async_execute(Query q, Params&&... params)
            -> Task<decltype(std::declval<db<DB>>().execute(std::declval<Query>(), std::declval<Params>()...))>
        {
            co_return co_await run_on_pool(*pool_,
                [this, q = std::move(q), ... p = std::forward<Params>(params)]() mutable {
                    return db_.execute(std::move(q), std::move(p)...);
                });
        }

        [[nodiscard]] db<DB>& sync_handle() noexcept { return db_; }
        [[nodiscard]] DB& connection() noexcept { return db_.connection(); }
        [[nodiscard]] ThreadPool& pool() noexcept { return *pool_; }

    private:
        db<DB> db_;
        ThreadPool* pool_;
    };

} // namespace orm
