#pragma once

#include "ORM/db/connectors/MySQLDB/mysql_live.hpp"
#include "ORM/async/io_context.hpp"
#include "ORM/async/task.hpp"
#include "ORM/connector/capabilities.hpp"
#include "ORM/result/result.hpp"

#ifdef ORM_MYSQL_LIVE_AVAILABLE

#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <format>

namespace orm {

    // ── AsyncMySQLDB ───────────────────────────────────────────────────────────
    // Native async MySQL connection using MariaDB's _start/_cont non-blocking API.
    // Uses IoContext::watch_readable/watch_writable for coroutine-based I/O.
    struct AsyncMySQLDB
    {
        MYSQL* conn_{nullptr};
        IoContext* ctx_{nullptr};

        AsyncMySQLDB() = default;
        AsyncMySQLDB(const AsyncMySQLDB&) = delete;
        AsyncMySQLDB& operator=(const AsyncMySQLDB&) = delete;

        AsyncMySQLDB(AsyncMySQLDB&& other) noexcept
            : conn_(other.conn_), ctx_(other.ctx_)
        {
            other.conn_ = nullptr;
            other.ctx_ = nullptr;
        }

        AsyncMySQLDB& operator=(AsyncMySQLDB&& other) noexcept
        {
            if (this != &other)
            {
                if (conn_) mysql_close(conn_);
                conn_ = other.conn_;
                ctx_ = other.ctx_;
                other.conn_ = nullptr;
                other.ctx_ = nullptr;
            }
            return *this;
        }

        ~AsyncMySQLDB()
        {
            if (conn_) mysql_close(conn_);
        }

        [[nodiscard]] static auto connect(
            const char* host, unsigned int port,
            const char* user, const char* password,
            const char* database, IoContext& ctx) -> Task<AsyncMySQLDB>
        {
            AsyncMySQLDB db;
            db.ctx_ = &ctx;
            db.conn_ = mysql_init(nullptr);
            if (!db.conn_)
                throw std::runtime_error("mysql_init failed");

            mysql_options(db.conn_, MYSQL_OPT_NONBLOCK, nullptr);

            MYSQL* ret = nullptr;
            int status = mysql_real_connect_start(
                &ret, db.conn_, host, user, password, database, port, nullptr, 0);

            while (status)
            {
                if (status & MYSQL_WAIT_WRITE)
                    co_await ctx.watch_writable(mysql_get_socket(db.conn_));
                else
                    co_await ctx.watch_readable(mysql_get_socket(db.conn_));
                status = mysql_real_connect_cont(&ret, db.conn_, status);
            }

            if (!ret)
            {
                std::string err = mysql_error(db.conn_);
                mysql_close(db.conn_);
                db.conn_ = nullptr;
                throw std::runtime_error("MySQL async connect failed: " + err);
            }

            co_return std::move(db);
        }

        [[nodiscard]] int fd() const noexcept
        {
            return mysql_get_socket(conn_);
        }

        [[nodiscard]] bool is_open() const noexcept
        {
            return conn_ != nullptr;
        }
    };

    // ── MySQL async helpers ────────────────────────────────────────────────────
    namespace mysql_async_detail {

        // Generic helper: drive any mysql _start/_cont pair as a coroutine
        template <typename RetT, typename StartFn, typename ContFn>
        [[nodiscard]] auto mysql_async_op(
            int fd, IoContext& ctx, StartFn start_fn, ContFn cont_fn) -> Task<RetT>
        {
            RetT ret{};
            int status = start_fn(&ret);
            while (status)
            {
                if (status & MYSQL_WAIT_WRITE)
                    co_await ctx.watch_writable(fd);
                else
                    co_await ctx.watch_readable(fd);
                status = cont_fn(&ret, status);
            }
            co_return ret;
        }

        // Async query: sends query and retrieves MYSQL_RES*
        [[nodiscard]] inline auto query_async(
            AsyncMySQLDB& db, const std::string& sql) -> Task<MYSQL_RES*>
        {
            // Send query
            int err = 0;
            int status = mysql_real_query_start(&err, db.conn_, sql.c_str(), sql.size());
            while (status)
            {
                if (status & MYSQL_WAIT_WRITE)
                    co_await db.ctx_->watch_writable(db.fd());
                else
                    co_await db.ctx_->watch_readable(db.fd());
                status = mysql_real_query_cont(&err, db.conn_, status);
            }
            if (err)
                throw std::runtime_error(
                    std::format("MySQL async query failed: {}", mysql_error(db.conn_)));

            // Store result
            MYSQL_RES* result = nullptr;
            status = mysql_store_result_start(&result, db.conn_);
            while (status)
            {
                if (status & MYSQL_WAIT_WRITE)
                    co_await db.ctx_->watch_writable(db.fd());
                else
                    co_await db.ctx_->watch_readable(db.fd());
                status = mysql_store_result_cont(&result, db.conn_, status);
            }

            co_return result;
        }

    } // namespace mysql_async_detail

    // ── connector_trait<AsyncMySQLDB> ──────────────────────────────────────────
    //
    // History: an earlier specialisation referenced three helpers that never
    // existed in mysql_live.hpp (\code{table_for_query}, \code{render_joins},
    // \code{hydrate_row} as free functions). It therefore failed to compile
    // the moment any translation unit instantiated it, and no test exercised
    // it. The empirical probe at
    // \code{tests/integration/test_mysql_async_coroutine_probe.cpp} confirmed
    // that the underlying `mysql_async_detail::query_async` (which drives
    // `_start`/`_cont` from C++20 coroutines via `IoContext::watch_*`) works
    // correctly on plain (non-TLS) connections. After lifting `hydrate_row`
    // out of `connector_trait<MySQLLiveDB>` into the `mysql_live_detail`
    // namespace, this specialisation is now re-introduced — it shares the
    // same query-rendering and row-hydration pipeline as the synchronous
    // trait and adds an asynchronous execution path on top.
    template <>
    struct connector_trait<AsyncMySQLDB>
    {
        using supports_joins              = void;
        using supports_transactions       = void;
        using supports_aggregation        = void;
        using supports_async              = void;
        using supports_concurrent_execute = void;

        template <typename T>
        struct wire_type { using type = T; };

        struct cursor_type
        {
            [[nodiscard]] bool has_next() const noexcept { return false; }
        };

        // ── Async SELECT (no runtime params) ──────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders>
        static auto async_execute(
            AsyncMySQLDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
            -> Task<result<projected_type<Response>, Response>>
        {
            using Row = projected_type<Response>;
            using Entity = typename Response::template orm_type<0>::table_type;

            const std::string sql = std::format("SELECT {} FROM {}{}{}{}",
                mysql_live_detail::render_columns(q.selected_properties()),
                table_name<Entity>(),
                mysql_live_detail::render_wheres(q.where_clauses()),
                mysql_live_detail::render_order_by(q.order_clauses()),
                mysql_live_detail::render_limits(q.limit_clauses()));

            MYSQL_RES* res = co_await mysql_async_detail::query_async(db, sql);
            if (!res)
            {
                throw std::runtime_error(std::format(
                    "MySQL async SELECT failed: {}", mysql_error(db.conn_)));
            }

            std::vector<Row> rows;
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                unsigned long* lengths = mysql_fetch_lengths(res);
                rows.push_back(mysql_live_detail::hydrate_row<Row>(row, lengths));
            }

            mysql_free_result(res);
            co_return result<Row, Response>{ std::move(rows) };
        }

        // ── Sync execute fallback ─────────────────────────────────────────
        // Provided for symmetry with connector_trait<MySQLLiveDB>; it runs
        // synchronously on the calling thread and is intended for tests and
        // for the case where async_db<DB> is bypassed deliberately.
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders>
        static auto execute(
            AsyncMySQLDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
            -> result<projected_type<Response>, Response>
        {
            using Row = projected_type<Response>;
            using Entity = typename Response::template orm_type<0>::table_type;

            const std::string sql = std::format("SELECT {} FROM {}{}{}{}",
                mysql_live_detail::render_columns(q.selected_properties()),
                table_name<Entity>(),
                mysql_live_detail::render_wheres(q.where_clauses()),
                mysql_live_detail::render_order_by(q.order_clauses()),
                mysql_live_detail::render_limits(q.limit_clauses()));

            if (mysql_real_query(db.conn_, sql.c_str(), sql.size()) != 0)
            {
                throw std::runtime_error(std::format(
                    "MySQL query failed: {}", mysql_error(db.conn_)));
            }
            MYSQL_RES* res = mysql_store_result(db.conn_);
            if (!res)
            {
                throw std::runtime_error(std::format(
                    "MySQL store_result failed: {}", mysql_error(db.conn_)));
            }

            std::vector<Row> rows;
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                unsigned long* lengths = mysql_fetch_lengths(res);
                rows.push_back(mysql_live_detail::hydrate_row<Row>(row, lengths));
            }

            mysql_free_result(res);
            return result<Row, Response>{ std::move(rows) };
        }

        static void begin(AsyncMySQLDB& db)
        {
            mysql_real_query(db.conn_, "BEGIN", 5);
        }
        static void commit(AsyncMySQLDB& db)
        {
            mysql_real_query(db.conn_, "COMMIT", 6);
        }
        static void rollback(AsyncMySQLDB& db)
        {
            mysql_real_query(db.conn_, "ROLLBACK", 8);
        }
    };

} // namespace orm

#endif // ORM_MYSQL_LIVE_AVAILABLE
