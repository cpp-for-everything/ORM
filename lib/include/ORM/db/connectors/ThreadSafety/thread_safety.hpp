#pragma once
#include "ORM/connector/capabilities.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/connector/db.hpp"
#include <array>
#include <mutex>
#include <condition_variable>
#include <cstddef>

namespace orm {

    // ── connection_state ──────────────────────────────────────────────────────
    enum class connection_state { Idle, InUse, Closed };

    // ── connection_guard<DB> ──────────────────────────────────────────────────
    // RAII guard holding exclusive ownership of one connection from a pool.
    // On destruction, transitions the connection back to Idle and notifies waiters.
    template <typename DB>
    class connection_guard
    {
    public:
        connection_guard(DB& conn, connection_state* state,
                         std::mutex* mtx, std::condition_variable* cv)
            : conn_(conn), state_(state), mtx_(mtx), cv_(cv)
        {}

        ~connection_guard()
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

        connection_guard(const connection_guard&)            = delete;
        connection_guard& operator=(const connection_guard&) = delete;

        connection_guard(connection_guard&& o) noexcept
            : conn_(o.conn_), state_(o.state_), mtx_(o.mtx_), cv_(o.cv_)
        {
            o.state_ = nullptr;
        }

        [[nodiscard]] db<DB> get() { return db<DB>{conn_}; }

    private:
        DB&                     conn_;
        connection_state*       state_;
        std::mutex*             mtx_;
        std::condition_variable* cv_;
    };

    // ── connection_pool<DB, N> ────────────────────────────────────────────────
    // Holds N independent connections of type DB.
    // acquire() blocks calling thread until a connection is Idle.
    // Enforces at instantiation that connector_trait<DB> declares
    // supports_concurrent_execute.
    template <typename DB, std::size_t N>
    class connection_pool
    {
        static_assert(
            requires { typename connector_trait<DB>::supports_concurrent_execute; },
            "connection_pool requires connector_trait<DB>::supports_concurrent_execute. "
            "Add 'using supports_concurrent_execute = void;' to connector_trait<DB>.");

    public:
        connection_pool()
        {
            states_.fill(connection_state::Idle);
        }

        ~connection_pool()
        {
            std::lock_guard<std::mutex> lk{mtx_};
            for (auto& s : states_)
                s = connection_state::Closed;
        }

        // ── acquire — blocks until a connection is Idle, returns RAII guard ──
        [[nodiscard]] connection_guard<DB> acquire()
        {
            std::unique_lock<std::mutex> lk{mtx_};
            cv_.wait(lk, [this]
            {
                for (auto& s : states_)
                    if (s == connection_state::Idle) return true;
                return false;
            });
            for (std::size_t i = 0; i < N; ++i)
            {
                if (states_[i] == connection_state::Idle)
                {
                    states_[i] = connection_state::InUse;
                    return connection_guard<DB>{connections_[i], &states_[i], &mtx_, &cv_};
                }
            }
            // Unreachable after wait — defensive
            return connection_guard<DB>{connections_[0], &states_[0], &mtx_, &cv_};
        }

    private:
        std::array<DB, N>                   connections_;
        std::array<connection_state, N>     states_{};
        mutable std::mutex                  mtx_;
        std::condition_variable             cv_;
    };

    // ── thread_local_db<DB> ───────────────────────────────────────────────────
    // One connection per calling thread. Zero synchronisation overhead.
    // Pattern: access via thread_local_db<DB>::get(), which returns a db<DB>
    // wrapping the thread-local connection instance.
    template <typename DB>
    class thread_local_db
    {
    public:
        // Returns a db<DB> wrapping this thread's dedicated connection.
        [[nodiscard]] static db<DB> get()
        {
            return db<DB>{instance()};
        }

        // Expose the underlying connection for inspection in tests.
        [[nodiscard]] static DB& connection()
        {
            return instance();
        }

    private:
        [[nodiscard]] static DB& instance()
        {
            thread_local DB conn{};
            return conn;
        }
    };

    // ── transaction_guard<DB> ────────────────────────────────────────────────
    // RAII transaction guard.
    // - Construction: issues BEGIN via connector_trait<DB>::begin(conn).
    // - commit():     issues COMMIT; marks guard as committed.
    // - Destruction:  if not committed, issues ROLLBACK.
    // - Enforces at instantiation that connector_trait<DB> declares
    //   supports_transactions.
    template <typename DB>
    class transaction_guard
    {
        static_assert(
            requires { typename connector_trait<DB>::supports_transactions; },
            "transaction_guard requires connector_trait<DB>::supports_transactions. "
            "Add 'using supports_transactions = void;' to connector_trait<DB>.");

    public:
        explicit transaction_guard(DB& conn) : conn_(conn), committed_(false)
        {
            connector_trait<DB>::begin(conn_);
        }

        ~transaction_guard()
        {
            if (!committed_)
                connector_trait<DB>::rollback(conn_);
        }

        void commit()
        {
            connector_trait<DB>::commit(conn_);
            committed_ = true;
        }

        transaction_guard(const transaction_guard&)            = delete;
        transaction_guard& operator=(const transaction_guard&) = delete;

    private:
        DB&  conn_;
        bool committed_;
    };

    // ── begin_transaction<DB>(conn) — free function convenience wrapper ───────
    // Usage: auto txn = orm::begin_transaction(db_conn);
    //        txn.commit(); // or let it destruct for auto ROLLBACK
    //
    // This is the preferred API because it avoids a circular include between
    // db.hpp (which would need thread_safety.hpp for the return type) and
    // thread_safety.hpp (which already includes db.hpp for connection_guard::get()).
    template <typename DB>
    [[nodiscard]] transaction_guard<DB> begin_transaction(DB& conn)
    {
        return transaction_guard<DB>{conn};
    }

} // namespace orm
