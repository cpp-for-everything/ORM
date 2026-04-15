#pragma once

#include "ORM/db/connectors/CassandraDB/cassandra_live.hpp"
#include "ORM/async/task.hpp"
#include "ORM/connector/capabilities.hpp"
#include "ORM/result/result.hpp"

#ifdef ORM_CASSANDRA_LIVE_AVAILABLE

#include <cassandra.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <format>
#include <coroutine>

namespace orm {

    // ── CassFutureAwaitable ────────────────────────────────────────────────────
    // Bridges the Cassandra driver's CassFuture callback API to C++20 coroutines.
    // The driver is already async internally; we just replace cass_future_wait()
    // with coroutine suspension via cass_future_set_callback().
    struct CassFutureAwaitable
    {
        CassFuture* future_;

        [[nodiscard]] bool await_ready() const noexcept
        {
            return cass_future_ready(future_) == cass_true;
        }

        void await_suspend(std::coroutine_handle<> h) noexcept
        {
            cass_future_set_callback(future_,
                [](CassFuture* /*f*/, void* data) {
                    auto handle = std::coroutine_handle<>::from_address(data);
                    handle.resume();
                },
                h.address());
        }

        void await_resume() const
        {
            if (cass_future_error_code(future_) != CASS_OK)
            {
                const char* msg = nullptr;
                std::size_t msg_len = 0;
                cass_future_error_message(future_, &msg, &msg_len);
                throw std::runtime_error(
                    std::format("Cassandra error: {}", std::string_view(msg, msg_len)));
            }
        }
    };

    // ── AsyncCassandraDB ───────────────────────────────────────────────────────
    // Async Cassandra connection using the callback-to-coroutine bridge.
    // The DataStax driver manages its own I/O threads internally;
    // CassFutureAwaitable suspends the calling coroutine until the driver
    // completes the operation and invokes the callback.
    struct AsyncCassandraDB
    {
        CassCluster* cluster_{nullptr};
        CassSession* session_{nullptr};

        AsyncCassandraDB() = default;
        AsyncCassandraDB(const AsyncCassandraDB&) = delete;
        AsyncCassandraDB& operator=(const AsyncCassandraDB&) = delete;

        AsyncCassandraDB(AsyncCassandraDB&& other) noexcept
            : cluster_(other.cluster_), session_(other.session_)
        {
            other.cluster_ = nullptr;
            other.session_ = nullptr;
        }

        AsyncCassandraDB& operator=(AsyncCassandraDB&& other) noexcept
        {
            if (this != &other)
            {
                if (session_)
                {
                    CassFuture* f = cass_session_close(session_);
                    cass_future_wait(f);
                    cass_future_free(f);
                    cass_session_free(session_);
                }
                if (cluster_) cass_cluster_free(cluster_);

                cluster_ = other.cluster_;
                session_ = other.session_;
                other.cluster_ = nullptr;
                other.session_ = nullptr;
            }
            return *this;
        }

        ~AsyncCassandraDB()
        {
            if (session_)
            {
                CassFuture* f = cass_session_close(session_);
                cass_future_wait(f);
                cass_future_free(f);
                cass_session_free(session_);
            }
            if (cluster_) cass_cluster_free(cluster_);
        }

        [[nodiscard]] static auto connect(
            const char* contact_points,
            const char* keyspace,
            unsigned int port = 9042) -> Task<AsyncCassandraDB>
        {
            AsyncCassandraDB db;
            db.cluster_ = cass_cluster_new();
            db.session_ = cass_session_new();

            cass_cluster_set_contact_points(db.cluster_, contact_points);
            cass_cluster_set_port(db.cluster_, static_cast<int>(port));

            CassFuture* connect_future = cass_session_connect_keyspace(
                db.session_, db.cluster_, keyspace);

            co_await CassFutureAwaitable{connect_future};
            cass_future_free(connect_future);

            co_return std::move(db);
        }

        [[nodiscard]] bool is_open() const noexcept
        {
            return session_ != nullptr;
        }
    };

    // ── connector_trait<AsyncCassandraDB> ──────────────────────────────────────
    template <>
    struct connector_trait<AsyncCassandraDB>
    {
        using supports_transactions = void;
        using supports_async = void;

        template <typename T>
        struct wire_type
        {
            using type = T;
        };

        struct cursor_type
        {
            [[nodiscard]] bool has_next() const noexcept { return false; }
        };

        // ── Async SELECT ───────────────────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders>
        static auto async_execute(
            AsyncCassandraDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
            -> Task<result<projected_type<Response>, Response>>
        {
            using Row = projected_type<Response>;

            auto cql = std::format("SELECT {} FROM {}{}{}",
                cass_live_detail::render_columns(q.selected_properties()),
                cass_live_detail::table_for_query(q),
                cass_live_detail::render_wheres(q.where_clauses()),
                cass_live_detail::render_limits(q.limit_clauses()));

            CassStatement* stmt = cass_statement_new(cql.c_str(), 0);
            CassFuture* future = cass_session_execute(db.session_, stmt);

            co_await CassFutureAwaitable{future};

            const CassResult* cass_result = cass_future_get_result(future);
            cass_future_free(future);
            cass_statement_free(stmt);

            std::vector<Row> rows;
            CassIterator* iter = cass_iterator_from_result(cass_result);
            while (cass_iterator_next(iter))
            {
                const CassRow* cass_row = cass_iterator_get_row(iter);
                rows.push_back(cass_live_detail::hydrate_row<Row>(cass_row));
            }
            cass_iterator_free(iter);
            cass_result_free(cass_result);

            co_return result<Row, Response>{std::move(rows)};
        }

        // ── Sync execute fallback ──────────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders>
        static auto execute(
            AsyncCassandraDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
            -> result<projected_type<Response>, Response>
        {
            using Row = projected_type<Response>;

            auto cql = std::format("SELECT {} FROM {}{}{}",
                cass_live_detail::render_columns(q.selected_properties()),
                cass_live_detail::table_for_query(q),
                cass_live_detail::render_wheres(q.where_clauses()),
                cass_live_detail::render_limits(q.limit_clauses()));

            CassStatement* stmt = cass_statement_new(cql.c_str(), 0);
            CassFuture* future = cass_session_execute(db.session_, stmt);
            cass_future_wait(future);

            if (cass_future_error_code(future) != CASS_OK)
            {
                const char* msg = nullptr;
                std::size_t msg_len = 0;
                cass_future_error_message(future, &msg, &msg_len);
                std::string err(msg, msg_len);
                cass_future_free(future);
                cass_statement_free(stmt);
                throw std::runtime_error("Cassandra query failed: " + err);
            }

            const CassResult* cass_result = cass_future_get_result(future);
            cass_future_free(future);
            cass_statement_free(stmt);

            std::vector<Row> rows;
            CassIterator* iter = cass_iterator_from_result(cass_result);
            while (cass_iterator_next(iter))
            {
                const CassRow* cass_row = cass_iterator_get_row(iter);
                rows.push_back(cass_live_detail::hydrate_row<Row>(cass_row));
            }
            cass_iterator_free(iter);
            cass_result_free(cass_result);

            return result<Row, Response>{std::move(rows)};
        }

        // ── Transaction control ────────────────────────────────────────────
        static void begin(AsyncCassandraDB& /*db*/) {}
        static void commit(AsyncCassandraDB& /*db*/) {}
        static void rollback(AsyncCassandraDB& /*db*/) {}
    };

} // namespace orm

#endif // ORM_CASSANDRA_LIVE_AVAILABLE
