#pragma once

#include "ORM/db/connectors/ThreadSafety/thread_safety.hpp"
#include "ORM/connector/async_db.hpp"
#include "ORM/async/task.hpp"
#include "ORM/async/thread_pool.hpp"
#include <array>
#include <mutex>
#include <condition_variable>
#include <cstddef>

namespace orm {

    // ── async_connection_guard<DB> ─────────────────────────────────────────────
    // RAII guard holding exclusive ownership of one connection from an async pool.
    // Provides an async_db handle for coroutine-based query dispatch.
    template <typename DB>
    class async_connection_guard
    {
    public:
        async_connection_guard(DB& conn, connection_state* state,
                               std::mutex* mtx, std::condition_variable* cv,
                               ThreadPool* pool)
            : conn_(conn), state_(state), mtx_(mtx), cv_(cv), pool_(pool)
        {
        }

        ~async_connection_guard()
        {
            if (state_)
            {
                {
                    std::lock_guard<std::mutex> lk{*mtx_};
                    *state_ = connection_state::Idle;
                }
                cv_->notify_one();
            }
        }

        async_connection_guard(const async_connection_guard&) = delete;
        async_connection_guard& operator=(const async_connection_guard&) = delete;

        async_connection_guard(async_connection_guard&& o) noexcept
            : conn_(o.conn_), state_(o.state_), mtx_(o.mtx_),
              cv_(o.cv_), pool_(o.pool_)
        {
            o.state_ = nullptr;
        }

        async_connection_guard& operator=(async_connection_guard&&) = delete;

        [[nodiscard]] auto get() -> async_db<DB>
        {
            return async_db<DB>{conn_, *pool_};
        }

        [[nodiscard]] auto sync_get() -> db<DB>
        {
            return db<DB>{conn_};
        }

    private:
        DB& conn_;
        connection_state* state_;
        std::mutex* mtx_;
        std::condition_variable* cv_;
        ThreadPool* pool_;
    };

    // ── async_connection_pool<DB, N> ───────────────────────────────────────────
    // Like connection_pool<DB, N> but acquire() is a coroutine that suspends
    // instead of blocking, and returns an async_connection_guard.
    template <typename DB, std::size_t N>
    class async_connection_pool
    {
        static_assert(
            requires { typename connector_trait<DB>::supports_concurrent_execute; },
            "async_connection_pool requires connector_trait<DB>::supports_concurrent_execute.");

    public:
        explicit async_connection_pool(ThreadPool& pool)
            : pool_(&pool)
        {
            states_.fill(connection_state::Idle);
        }

        ~async_connection_pool()
        {
            std::lock_guard<std::mutex> lk{mtx_};
            for (auto& s : states_)
                s = connection_state::Closed;
        }

        // ── acquire — coroutine that suspends until a connection is Idle ────
        [[nodiscard]] auto acquire() -> Task<async_connection_guard<DB>>
        {
            co_return co_await run_on_pool(*pool_, [this]() -> async_connection_guard<DB> {
                std::unique_lock<std::mutex> lk{mtx_};
                cv_.wait(lk, [this]
                {
                    for (const auto& s : states_)
                        if (s == connection_state::Idle) return true;
                    return false;
                });
                for (std::size_t i = 0; i < N; ++i)
                {
                    if (states_[i] == connection_state::Idle)
                    {
                        states_[i] = connection_state::InUse;
                        return async_connection_guard<DB>{
                            connections_[i], &states_[i], &mtx_, &cv_, pool_};
                    }
                }
                // Unreachable after wait — defensive
                return async_connection_guard<DB>{
                    connections_[0], &states_[0], &mtx_, &cv_, pool_};
            });
        }

        // Synchronous acquire for non-coroutine contexts
        [[nodiscard]] auto acquire_sync() -> async_connection_guard<DB>
        {
            std::unique_lock<std::mutex> lk{mtx_};
            cv_.wait(lk, [this]
            {
                for (const auto& s : states_)
                    if (s == connection_state::Idle) return true;
                return false;
            });
            for (std::size_t i = 0; i < N; ++i)
            {
                if (states_[i] == connection_state::Idle)
                {
                    states_[i] = connection_state::InUse;
                    return async_connection_guard<DB>{
                        connections_[i], &states_[i], &mtx_, &cv_, pool_};
                }
            }
            return async_connection_guard<DB>{
                connections_[0], &states_[0], &mtx_, &cv_, pool_};
        }

    private:
        std::array<DB, N> connections_;
        std::array<connection_state, N> states_{};
        mutable std::mutex mtx_;
        std::condition_variable cv_;
        ThreadPool* pool_;
    };

    // ── async_transaction_guard<DB> ────────────────────────────────────────────
    // Async RAII transaction guard.
    // - begin():   issues BEGIN via run_on_pool.
    // - commit():  issues COMMIT via run_on_pool.
    // - rollback_if_needed(): issues ROLLBACK if not committed, via run_on_pool.
    //
    // Usage:
    //   auto txn = co_await orm::async_begin_transaction(conn, pool);
    //   ... use conn ...
    //   co_await txn.commit();
    //   // or let it destruct for sync ROLLBACK (safe for cleanup)
    //
    template <typename DB>
    class async_transaction_guard
    {
        static_assert(
            requires { typename connector_trait<DB>::supports_transactions; },
            "async_transaction_guard requires connector_trait<DB>::supports_transactions.");

    public:
        async_transaction_guard(DB& conn, ThreadPool& pool)
            : conn_(conn), pool_(&pool), committed_(false)
        {
        }

        ~async_transaction_guard()
        {
            if (!committed_)
            {
                connector_trait<DB>::rollback(conn_);
            }
        }

        async_transaction_guard(const async_transaction_guard&) = delete;
        async_transaction_guard& operator=(const async_transaction_guard&) = delete;

        async_transaction_guard(async_transaction_guard&& o) noexcept
            : conn_(o.conn_), pool_(o.pool_), committed_(o.committed_)
        {
            o.committed_ = true;
        }

        async_transaction_guard& operator=(async_transaction_guard&&) = delete;

        [[nodiscard]] auto commit() -> Task<void>
        {
            co_await run_on_pool(*pool_, [this] {
                connector_trait<DB>::commit(conn_);
            });
            committed_ = true;
            co_return;
        }

        [[nodiscard]] auto rollback() -> Task<void>
        {
            co_await run_on_pool(*pool_, [this] {
                connector_trait<DB>::rollback(conn_);
            });
            committed_ = true;
            co_return;
        }

        [[nodiscard]] bool is_committed() const noexcept { return committed_; }

    private:
        DB& conn_;
        ThreadPool* pool_;
        bool committed_;
    };

    // ── async_begin_transaction — coroutine factory ────────────────────────────
    template <typename DB>
    [[nodiscard]] auto async_begin_transaction(DB& conn, ThreadPool& pool)
        -> Task<async_transaction_guard<DB>>
    {
        co_await run_on_pool(pool, [&conn] {
            connector_trait<DB>::begin(conn);
        });
        co_return async_transaction_guard<DB>{conn, pool};
    }

} // namespace orm
