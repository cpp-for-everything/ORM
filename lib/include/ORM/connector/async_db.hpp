#pragma once

#include "ORM/connector/db.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/async/task.hpp"
#include "ORM/async/thread_pool.hpp"
#include <memory>
#include <mutex>
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
    // One connection runs one query at a time. On the run_on_pool path the
    // connection is guarded by a mutex, so several tasks on one async_db (or on
    // copies of it) take turns. A connector that declares
    // supports_shared_connection skips the lock. supports_concurrent_execute
    // does not: it means one connection per thread. Cost: one mutex lock and
    // unlock per query, uncontended unless tasks share the connection. Two
    // async_db objects built separately on the same DB& do not share the mutex.
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
            : db_(connection), pool_(&pool), guard_(std::make_shared<std::mutex>())
        {
        }

        // ── async operator<< — primary async query dispatch ────────────────────
        // Returns a Task that, when co_awaited, offloads the query to the pool.
        template <typename Query>
        [[nodiscard]] auto operator<<(Query q) -> Task<decltype(std::declval<db<DB>>() << std::declval<Query>())>
        {
            detail::check_query_shape<Query>();
            using ResultType = decltype(std::declval<db<DB>>() << std::declval<Query>());

            if constexpr (has_capability<DB, cap::supports_async>)
            {
                co_return co_await connector_trait<DB>::async_execute(
                    db_.connection(), std::move(q));
            }
            else
            {
                co_return co_await run_on_pool(*pool_, [this, q = std::move(q)]() mutable -> ResultType {
                    [[maybe_unused]] auto lock = lock_connection();
                    return db_ << std::move(q);
                });
            }
        }

        // ── async execute with explicit parameters ─────────────────────────────
        // Params are taken by value on purpose. The task starts lazily, so a
        // reference parameter would dangle if the caller passed a temporary.
        template <typename Query, typename... Params>
        [[nodiscard]] auto async_execute(Query q, Params... params)
            -> Task<decltype(std::declval<db<DB>>().execute(std::declval<Query>(), std::declval<Params>()...))>
        {
            detail::check_query_shape<Query>();
            co_return co_await run_on_pool(*pool_,
                [this, q = std::move(q), ... p = std::move(params)]() mutable {
                    [[maybe_unused]] auto lock = lock_connection();
                    return db_.execute(std::move(q), std::move(p)...);
                });
        }

        [[nodiscard]] db<DB>& sync_handle() noexcept { return db_; }
        [[nodiscard]] DB& connection() noexcept { return db_.connection(); }
        [[nodiscard]] ThreadPool& pool() noexcept { return *pool_; }

    private:
        // Empty guard when the connector declares it is safe to share.
        [[nodiscard]] std::unique_lock<std::mutex> lock_connection()
        {
            if constexpr (has_capability<DB, cap::supports_shared_connection>)
                return {};
            else
                return std::unique_lock<std::mutex>(*guard_);
        }

        db<DB> db_;
        ThreadPool* pool_;
        std::shared_ptr<std::mutex> guard_;
    };

} // namespace orm
