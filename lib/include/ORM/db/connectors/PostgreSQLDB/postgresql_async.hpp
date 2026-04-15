#pragma once

#include "ORM/db/connectors/PostgreSQLDB/postgresql_live.hpp"
#include "ORM/async/io_context.hpp"
#include "ORM/async/task.hpp"
#include "ORM/connector/capabilities.hpp"
#include "ORM/result/result.hpp"

#ifdef ORM_POSTGRESQL_LIVE_AVAILABLE

#include <libpq-fe.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <format>
#include <memory>

namespace orm {

    // ── AsyncPostgreSQLDB ──────────────────────────────────────────────────────
    // Native async PostgreSQL connection using libpq's non-blocking API.
    // Uses IoContext::watch_readable/watch_writable for coroutine-based I/O.
    struct AsyncPostgreSQLDB
    {
        PGconn* conn_{nullptr};
        IoContext* ctx_{nullptr};

        AsyncPostgreSQLDB() = default;
        AsyncPostgreSQLDB(const AsyncPostgreSQLDB&) = delete;
        AsyncPostgreSQLDB& operator=(const AsyncPostgreSQLDB&) = delete;

        AsyncPostgreSQLDB(AsyncPostgreSQLDB&& other) noexcept
            : conn_(other.conn_), ctx_(other.ctx_)
        {
            other.conn_ = nullptr;
            other.ctx_ = nullptr;
        }

        AsyncPostgreSQLDB& operator=(AsyncPostgreSQLDB&& other) noexcept
        {
            if (this != &other)
            {
                if (conn_) PQfinish(conn_);
                conn_ = other.conn_;
                ctx_ = other.ctx_;
                other.conn_ = nullptr;
                other.ctx_ = nullptr;
            }
            return *this;
        }

        ~AsyncPostgreSQLDB()
        {
            if (conn_) PQfinish(conn_);
        }

        [[nodiscard]] static auto connect(const char* conninfo, IoContext& ctx)
            -> Task<AsyncPostgreSQLDB>
        {
            AsyncPostgreSQLDB db;
            db.ctx_ = &ctx;
            db.conn_ = PQconnectStart(conninfo);

            if (!db.conn_)
                throw std::runtime_error("PQconnectStart failed: out of memory");

            if (PQstatus(db.conn_) == CONNECTION_BAD)
            {
                std::string err = PQerrorMessage(db.conn_);
                PQfinish(db.conn_);
                db.conn_ = nullptr;
                throw std::runtime_error("PostgreSQL connection failed: " + err);
            }

            // Drive the async connection state machine
            while (true)
            {
                auto poll_status = PQconnectPoll(db.conn_);
                if (poll_status == PGRES_POLLING_OK) break;
                if (poll_status == PGRES_POLLING_FAILED)
                {
                    std::string err = PQerrorMessage(db.conn_);
                    PQfinish(db.conn_);
                    db.conn_ = nullptr;
                    throw std::runtime_error("PostgreSQL connect poll failed: " + err);
                }

                if (poll_status == PGRES_POLLING_WRITING)
                    co_await ctx.watch_writable(PQsocket(db.conn_));
                else if (poll_status == PGRES_POLLING_READING)
                    co_await ctx.watch_readable(PQsocket(db.conn_));
            }

            PQsetnonblocking(db.conn_, 1);
            co_return std::move(db);
        }

        [[nodiscard]] int fd() const noexcept { return PQsocket(conn_); }
        [[nodiscard]] bool is_open() const noexcept
        {
            return conn_ && PQstatus(conn_) == CONNECTION_OK;
        }
    };

    // ── Async query execution helpers ──────────────────────────────────────────
    namespace pg_async_detail {

        // Core: send query + wait for result, fully non-blocking
        [[nodiscard]] inline auto exec_async(
            AsyncPostgreSQLDB& db,
            const std::string& sql,
            const std::vector<std::string>& vals) -> Task<PGresult*>
        {
            std::vector<const char*> param_values;
            param_values.reserve(vals.size());
            for (const auto& v : vals)
                param_values.push_back(v.c_str());

            int ok = PQsendQueryParams(
                db.conn_, sql.c_str(),
                static_cast<int>(vals.size()),
                nullptr, param_values.data(),
                nullptr, nullptr, 0);

            if (!ok)
                throw std::runtime_error(
                    std::format("PQsendQueryParams failed: {}", PQerrorMessage(db.conn_)));

            // Flush send buffer
            while (PQflush(db.conn_) == 1)
                co_await db.ctx_->watch_writable(db.fd());

            // Wait for result
            while (PQisBusy(db.conn_))
            {
                co_await db.ctx_->watch_readable(db.fd());
                if (!PQconsumeInput(db.conn_))
                    throw std::runtime_error(
                        std::format("PQconsumeInput failed: {}", PQerrorMessage(db.conn_)));
            }

            PGresult* res = PQgetResult(db.conn_);

            // Drain remaining results
            PGresult* extra = nullptr;
            while ((extra = PQgetResult(db.conn_)) != nullptr)
                PQclear(extra);

            co_return res;
        }

    } // namespace pg_async_detail

    // ── connector_trait<AsyncPostgreSQLDB> ─────────────────────────────────────
    template <>
    struct connector_trait<AsyncPostgreSQLDB>
    {
        using supports_joins = void;
        using supports_transactions = void;
        using supports_aggregation = void;
        using supports_async = void;
        using supports_concurrent_execute = void;

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
            AsyncPostgreSQLDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
            -> Task<result<projected_type<Response>, Response>>
        {
            using Row = projected_type<Response>;
            pg_live_detail::RenderCtx ctx;

            auto sql = std::format("SELECT {} FROM {}{}{}{}{}",
                pg_live_detail::render_columns(q.selected_properties(), ctx),
                pg_live_detail::table_for_query(q),
                pg_live_detail::render_joins(q.join_clauses(), ctx),
                pg_live_detail::render_wheres(q.where_clauses(), ctx),
                pg_live_detail::render_order_by(q.order_clauses()),
                pg_live_detail::render_limits(q.limit_clauses()));

            PGresult* res = co_await pg_async_detail::exec_async(db, sql, {});

            if (PQresultStatus(res) != PGRES_TUPLES_OK)
            {
                std::string err = PQerrorMessage(db.conn_);
                PQclear(res);
                throw std::runtime_error("PostgreSQL async SELECT failed: " + err);
            }

            const int nrows = PQntuples(res);
            std::vector<Row> rows;
            rows.reserve(nrows);

            for (int i = 0; i < nrows; ++i)
                rows.push_back(pg_live_detail::hydrate_row<Row>(res, i));

            PQclear(res);
            co_return result<Row, Response>{std::move(rows)};
        }

        // ── Sync execute fallback (for orm::db<AsyncPostgreSQLDB>) ─────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders>
        static auto execute(
            AsyncPostgreSQLDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
            -> result<projected_type<Response>, Response>
        {
            using Row = projected_type<Response>;
            pg_live_detail::RenderCtx ctx;

            auto sql = std::format("SELECT {} FROM {}{}{}{}{}",
                pg_live_detail::render_columns(q.selected_properties(), ctx),
                pg_live_detail::table_for_query(q),
                pg_live_detail::render_joins(q.join_clauses(), ctx),
                pg_live_detail::render_wheres(q.where_clauses(), ctx),
                pg_live_detail::render_order_by(q.order_clauses()),
                pg_live_detail::render_limits(q.limit_clauses()));

            PGresult* res = PQexec(db.conn_, sql.c_str());

            if (PQresultStatus(res) != PGRES_TUPLES_OK)
            {
                std::string err = PQerrorMessage(db.conn_);
                PQclear(res);
                throw std::runtime_error("PostgreSQL SELECT failed: " + err);
            }

            const int nrows = PQntuples(res);
            std::vector<Row> rows;
            rows.reserve(nrows);

            for (int i = 0; i < nrows; ++i)
                rows.push_back(pg_live_detail::hydrate_row<Row>(res, i));

            PQclear(res);
            return result<Row, Response>{std::move(rows)};
        }

        // ── Transaction control ────────────────────────────────────────────
        static void begin(AsyncPostgreSQLDB& db)
        {
            PGresult* res = PQexec(db.conn_, "BEGIN");
            PQclear(res);
        }

        static void commit(AsyncPostgreSQLDB& db)
        {
            PGresult* res = PQexec(db.conn_, "COMMIT");
            PQclear(res);
        }

        static void rollback(AsyncPostgreSQLDB& db)
        {
            PGresult* res = PQexec(db.conn_, "ROLLBACK");
            PQclear(res);
        }
    };

} // namespace orm

#endif // ORM_POSTGRESQL_LIVE_AVAILABLE
