#pragma once
#include "ORM/connector/capabilities.hpp"
#include "ORM/result/result.hpp"
#include "ORM/query/select.hpp"
#include "ORM/query/insert.hpp"
#include "ORM/query/update.hpp"
#include "ORM/query/delete.hpp"
#include "ORM/query/field.hpp"
#include "ORM/query/placeholders.hpp"
#include "ORM/result/joined_row.hpp"        // joined_row_for / hydrate_joined (pulls join_infer)
#include "ORM/entity/table.hpp"
#include "ORM/details/member_pointer.hpp"

#ifdef ORM_POSTGRESQL_LIVE_AVAILABLE
#include <libpq-fe.h>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <format>
#include <unordered_map>
#include <memory>
#include <cstring>

namespace orm {

    // ── PostgreSQLLiveDB ───────────────────────────────────────────────────────
    // Live PostgreSQL connection using libpq (C API).
    // Open via PostgreSQLLiveDB::connect(connection_string).
    struct PostgreSQLLiveDB
    {
        PGconn* conn_{nullptr};

        PostgreSQLLiveDB() = default;
        PostgreSQLLiveDB(const PostgreSQLLiveDB&) = delete;
        PostgreSQLLiveDB& operator=(const PostgreSQLLiveDB&) = delete;
        
        PostgreSQLLiveDB(PostgreSQLLiveDB&& other) noexcept
            : conn_(other.conn_)
        {
            other.conn_ = nullptr;
        }
        
        PostgreSQLLiveDB& operator=(PostgreSQLLiveDB&& other) noexcept
        {
            if (this != &other)
            {
                if (conn_)
                    PQfinish(conn_);
                conn_ = other.conn_;
                other.conn_ = nullptr;
            }
            return *this;
        }
        
        ~PostgreSQLLiveDB()
        {
            if (conn_)
                PQfinish(conn_);
        }

        // conninfo: libpq connection string, e.g.:
        //   "host=localhost port=5432 dbname=mydb user=postgres password=secret"
        [[nodiscard]] static PostgreSQLLiveDB connect(const char* conninfo)
        {
            PostgreSQLLiveDB db;
            db.conn_ = PQconnectdb(conninfo);
            if (!db.conn_ || PQstatus(db.conn_) != CONNECTION_OK)
            {
                std::string err = db.conn_ ? PQerrorMessage(db.conn_) : "PQconnectdb failed";
                if (db.conn_)
                    PQfinish(db.conn_);
                throw std::runtime_error("PostgreSQL connection failed: " + err);
            }
            return db;
        }

        [[nodiscard]] bool is_open() const noexcept
        {
            return conn_ && PQstatus(conn_) == CONNECTION_OK;
        }

        [[nodiscard]] PGconn* native() { return conn_; }
    };

    // ── PG rendering helpers ───────────────────────────────────────────────────
    namespace pg_live_detail {

        struct RenderCtx
        {
            int                          next_param{1};
            std::unordered_map<int, int> idx_map;
        };

        template <typename T>
        [[nodiscard]] std::string render_leaf(const T& /*v*/, RenderCtx& ctx)
        {
            if constexpr (is_placeholder_v<T>)
            {
                constexpr int idx = placeholder_index_v<T>;
                if constexpr (idx > 0)
                {
                    auto it = ctx.idx_map.find(idx);
                    if (it == ctx.idx_map.end())
                    {
                        int n = ctx.next_param++;
                        ctx.idx_map[idx] = n;
                        return std::format("${}", n);
                    }
                    return std::format("${}", it->second);
                }
                else
                {
                    return std::format("${}", ctx.next_param++);
                }
            }
            else if constexpr (is_field<T>)
                return std::string(T::column_name());
            else if constexpr (detail::is_raw_mem_ptr<T>)
                return std::string(detail::column_name_of<T>());
            else
                return "NULL";
        }

        // Translate ORM operators to SQL operators
        [[nodiscard]] inline std::string translate_operator(std::string_view op)
        {
            if (op == "==") return "=";
            if (op == "!=") return "!=";
            return std::string(op);
        }

        template <typename T1, detail::string_literal Op, typename T2>
        [[nodiscard]] std::string render_rule(const Rule<T1, Op, T2>& r, RenderCtx& ctx);

        template <typename T1, detail::string_literal Op, typename T2>
        [[nodiscard]] std::string render_rule(const Rule<T1, Op, T2>& r, RenderCtx& ctx)
        {
            std::string sql_op = translate_operator(static_cast<std::string_view>(Op));
            
            if constexpr (is_rule<T1> && is_rule<T2>)
            {
                return render_rule(r.lhs_, ctx)
                    + " " + sql_op + " "
                    + render_rule(r.rhs_, ctx);
            }
            else
            {
                return render_leaf(r.lhs_, ctx)
                    + " " + sql_op + " "
                    + render_leaf(r.rhs_, ctx);
            }
        }

        template <typename Tuple>
        [[nodiscard]] std::string render_columns(const Tuple& t)
        {
            std::string out;
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                std::size_t idx = 0;
                ((void)(out += (idx++ > 0 ? ", " : "")
                    + std::string(t.template get<Is>().column_name())), ...);
            }(std::make_index_sequence<Tuple::size>{});
            return out;
        }

        template <typename Wheres>
        [[nodiscard]] std::string render_wheres(const Wheres& w, RenderCtx& ctx)
        {
            if constexpr (Wheres::size == 0)
                return {};
            else
            {
                std::string out = " WHERE ";
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    std::size_t i = 0;
                    ((void)(out += (i++ > 0 ? " AND " : "") + render_rule(w.template get<Is>(), ctx)), ...);
                }(std::make_index_sequence<Wheres::size>{});
                return out;
            }
        }

        template <typename Orders>
        [[nodiscard]] std::string render_order_by(const Orders& /*o*/)
        {
            if constexpr (Orders::size == 0)
                return {};
            else
            {
                std::string out = " ORDER BY ";
                bool first = true;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    ([&]()
                    {
                        using OB = typename Orders::template orm_type<Is>;
                        using Tag = mem_ptr<OB::member>;
                        constexpr std::string_view dir =
                            (OB::sort == order::direction::asc) ? "ASC" : "DESC";
                        if (!first) out += ", ";
                        out += std::string(Tag::column_name()) + " " + std::string(dir);
                        first = false;
                    }(), ...);
                }(std::make_index_sequence<Orders::size>{});
                return out;
            }
        }

        template <typename Limits>
        [[nodiscard]] std::string render_limits(const Limits& lims)
        {
            if constexpr (Limits::size == 0)
                return {};
            else
            {
                const auto& p = lims.template get<0>();
                return std::format(" LIMIT {} OFFSET {}",
                    p.get_elements_per_page(),
                    p.get_elements_per_page() * p.get_number_of_page());
            }
        }

        [[nodiscard]] inline std::string dollar_placeholders(std::size_t n)
        {
            std::string out;
            for (std::size_t i = 1; i <= n; ++i)
            {
                if (i > 1) out += ", ";
                out += "$" + std::to_string(i);
            }
            return out;
        }

        template <typename Stmts>
        [[nodiscard]] std::string render_set(const Stmts& s, RenderCtx& ctx)
        {
            if constexpr (Stmts::size == 0)
                return {};
            else
            {
                std::string out;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    std::size_t idx = 0;
                    ([&]()
                    {
                        const auto& stmt = s.template get<Is>();
                        using StmtT = std::remove_cvref_t<decltype(stmt)>;
                        const std::string col = std::string(StmtT::field_tag::column_name());
                        out += (idx++ > 0 ? ", " : "") + col + " = $" + std::to_string(ctx.next_param++);
                    }(), ...);
                }(std::make_index_sequence<Stmts::size>{});
                return out;
            }
        }

        // Convert a runtime C++ value to a pqxx param string.
        template <typename T>
        [[nodiscard]] std::string param_to_string(const T& v)
        {
            using D = std::decay_t<T>;
            if constexpr (std::is_same_v<D, bool>)
                return v ? "t" : "f";
            else if constexpr (std::is_arithmetic_v<D>)
                return std::to_string(v);
            else if constexpr (std::is_same_v<D, std::string>)
                return v;
            else if constexpr (std::is_same_v<D, std::u8string>)
                return std::string(reinterpret_cast<const char*>(v.data()), v.size());
            else if constexpr (std::is_same_v<D, const char*> || std::is_same_v<D, char*>)
                return std::string(v);
            else
                return "";
        }

        template <typename... Params>
        [[nodiscard]] std::vector<std::string> collect_params(Params&&... params)
        {
            std::vector<std::string> out;
            out.reserve(sizeof...(params));
            (out.push_back(param_to_string(std::forward<Params>(params))), ...);
            return out;
        }

        // Convert a libpq result field to the typed column T.
        template <typename T>
        [[nodiscard]] T convert_field(PGresult* res, int row, int col)
        {
            if (PQgetisnull(res, row, col))
                return T{};
            
            const char* val = PQgetvalue(res, row, col);
            
            if constexpr (std::is_same_v<T, std::string>)
                return std::string(val);
            else if constexpr (std::is_same_v<T, std::u8string>)
            {
                return std::u8string(reinterpret_cast<const char8_t*>(val));
            }
            else if constexpr (std::is_same_v<T, bool>)
                return (val[0] == 't' || val[0] == 'T' || val[0] == '1');
            else if constexpr (std::is_same_v<T, double>)
                return std::stod(val);
            else if constexpr (std::is_same_v<T, float>)
                return std::stof(val);
            else if constexpr (std::is_integral_v<T>)
                return static_cast<T>(std::stoll(val));
            else
                return T{};
        }

        // ── relationship-aware SQL rendering (connector-local) ────────────────
        // The ORM core hands us the compile-time join plan; turning it into SQL
        // text — including which dialect keyword join::mode maps to — is the
        // connector's job, so this lives here rather than in the ORM core.
        [[nodiscard]] inline std::string_view join_kind_sql(orm::join::mode m) noexcept
        {
            switch (m)
            {
                case orm::join::mode::inner: return "INNER JOIN";
                case orm::join::mode::left:  return "LEFT JOIN";
                case orm::join::mode::right: return "RIGHT JOIN";
                default:                     return "FULL JOIN";
            }
        }

        template <typename... Fields>
        [[nodiscard]] std::string qualified_columns_impl(orm::detail::orm_tuple<Fields...>*)
        {
            std::string out;
            bool first = true;
            (((out += (first ? std::string{} : std::string{", "})
                 + std::string(orm::table_name<typename Fields::table_type>()) + "."
                 + std::string(Fields::column_name())),
              first = false),
             ...);
            return out;
        }
        template <typename Response>
        [[nodiscard]] std::string render_qualified_columns()
        {
            return qualified_columns_impl(static_cast<Response*>(nullptr));
        }

        template <typename... Steps>
        [[nodiscard]] std::string inferred_joins_impl(orm::detail::tl<Steps...>*)
        {
            std::string out;
            ((out += " " + std::string(join_kind_sql(Steps::mode)) + " "
                   + std::string(orm::table_name<typename Steps::to>()) + " ON "
                   + std::string(orm::table_name<typename Steps::from>()) + "."
                   + std::string(Steps::rel::column_name()) + " = "
                   + std::string(orm::table_name<typename Steps::to>()) + "."
                   + std::string(Steps::rel::target_column())),
             ...);
            return out;
        }
        template <typename Response>
        [[nodiscard]] std::string render_inferred_joins()
        {
            return inferred_joins_impl(static_cast<orm::detail::join_plan_t<Response>*>(nullptr));
        }

    } // namespace pg_live_detail

    // ── connector_trait<PostgreSQLLiveDB> specialisation ──────────────────────
    template <>
    struct connector_trait<PostgreSQLLiveDB>
    {
        using supports_joins        = void;
        using supports_transactions = void;
        using supports_aggregation  = void;

        template <typename T>
        struct wire_type { using type = T; };

        struct cursor_type
        {
            [[nodiscard]] bool has_next() const noexcept { return false; }
        };

        // ── SELECT (no runtime params) ────────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders>
        static auto execute(
            PostgreSQLLiveDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
        {
            constexpr bool inferred = orm::detail::is_multi_table<Response> && Joins::size == 0;
            pg_live_detail::RenderCtx ctx;
            const std::string sql = build_select_sql<Response, inferred>(q, ctx);
            return exec_select<Response, inferred>(db, sql, {});
        }

        // ── SELECT (with runtime params) ──────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders,
                  typename... Params>
        static auto execute(
            PostgreSQLLiveDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q,
            Params&&... params)
        {
            constexpr bool inferred = orm::detail::is_multi_table<Response> && Joins::size == 0;
            pg_live_detail::RenderCtx ctx;
            const std::string sql = build_select_sql<Response, inferred>(q, ctx);
            auto vals = pg_live_detail::collect_params(std::forward<Params>(params)...);
            return exec_select<Response, inferred>(db, sql, vals);
        }

        // ── INSERT (no runtime params) ────────────────────────────────────────
        template <typename Properties>
        static auto execute(PostgreSQLLiveDB& db, insert_query<Properties> q)
            -> result<std::tuple<>>
        {
            using Entity = typename Properties::template orm_type<0>::table_type;
            const std::string sql = std::format("INSERT INTO {} ({}) VALUES ({})",
                table_name<Entity>(),
                pg_live_detail::render_columns(q.signature()),
                pg_live_detail::dollar_placeholders(Properties::size));
            exec_no_result(db, sql, {});
            return result<std::tuple<>>{};
        }

        // ── INSERT (with runtime params) ──────────────────────────────────────
        template <typename Properties, typename... Params>
        static auto execute(PostgreSQLLiveDB& db, insert_query<Properties> q, Params&&... params)
            -> result<std::tuple<>>
        {
            using Entity = typename Properties::template orm_type<0>::table_type;
            const std::string sql = std::format("INSERT INTO {} ({}) VALUES ({})",
                table_name<Entity>(),
                pg_live_detail::render_columns(q.signature()),
                pg_live_detail::dollar_placeholders(Properties::size));
            auto vals = pg_live_detail::collect_params(std::forward<Params>(params)...);
            exec_no_result(db, sql, vals);
            return result<std::tuple<>>{};
        }

        // ── UPDATE (no runtime params) ────────────────────────────────────────
        template <typename Table, typename Statements, typename Wheres>
        static auto execute(PostgreSQLLiveDB& db, update_query<Table, Statements, Wheres> q)
            -> result<std::tuple<>>
        {
            // Render SET then WHERE sequentially: both mutate RenderCtx.next_param.
            // Passing them as sibling std::format args has unspecified evaluation
            // order, which can assign $1 to WHERE while runtime params stay
            // SET-first (grape→$1 typed as int) — CI PostgreSQLLiveTest.UpdateChangesName.
            pg_live_detail::RenderCtx ctx;
            const std::string set_sql = pg_live_detail::render_set(q.updates(), ctx);
            const std::string where_sql = pg_live_detail::render_wheres(q.wheres(), ctx);
            const std::string sql = std::format("UPDATE {} SET {}{}",
                table_name<Table>(), set_sql, where_sql);
            exec_no_result(db, sql, {});
            return result<std::tuple<>>{};
        }

        // ── UPDATE (with runtime params) ──────────────────────────────────────
        template <typename Table, typename Statements, typename Wheres, typename... Params>
        static auto execute(PostgreSQLLiveDB& db, update_query<Table, Statements, Wheres> q,
                            Params&&... params)
            -> result<std::tuple<>>
        {
            pg_live_detail::RenderCtx ctx;
            const std::string set_sql = pg_live_detail::render_set(q.updates(), ctx);
            const std::string where_sql = pg_live_detail::render_wheres(q.wheres(), ctx);
            const std::string sql = std::format("UPDATE {} SET {}{}",
                table_name<Table>(), set_sql, where_sql);
            auto vals = pg_live_detail::collect_params(std::forward<Params>(params)...);
            exec_no_result(db, sql, vals);
            return result<std::tuple<>>{};
        }

        // ── DELETE (no runtime params) ────────────────────────────────────────
        template <typename Table, typename Wheres>
        static auto execute(PostgreSQLLiveDB& db, delete_query<Table, Wheres> q)
            -> result<std::tuple<>>
        {
            pg_live_detail::RenderCtx ctx;
            const std::string sql = std::format("DELETE FROM {}{}",
                table_name<Table>(),
                pg_live_detail::render_wheres(q.wheres(), ctx));
            exec_no_result(db, sql, {});
            return result<std::tuple<>>{};
        }

        // ── DELETE (with runtime params) ──────────────────────────────────────
        template <typename Table, typename Wheres, typename... Params>
        static auto execute(PostgreSQLLiveDB& db, delete_query<Table, Wheres> q, Params&&... params)
            -> result<std::tuple<>>
        {
            pg_live_detail::RenderCtx ctx;
            const std::string sql = std::format("DELETE FROM {}{}",
                table_name<Table>(),
                pg_live_detail::render_wheres(q.wheres(), ctx));
            auto vals = pg_live_detail::collect_params(std::forward<Params>(params)...);
            exec_no_result(db, sql, vals);
            return result<std::tuple<>>{};
        }

        // ── Transaction helpers ───────────────────────────────────────────────
        static void begin(PostgreSQLLiveDB& db)
        {
            PGresult* res = PQexec(db.native(), "BEGIN");
            if (PQresultStatus(res) != PGRES_COMMAND_OK)
            {
                std::string err = PQerrorMessage(db.native());
                PQclear(res);
                throw std::runtime_error("BEGIN failed: " + err);
            }
            PQclear(res);
        }
        
        static void commit(PostgreSQLLiveDB& db)
        {
            PGresult* res = PQexec(db.native(), "COMMIT");
            if (PQresultStatus(res) != PGRES_COMMAND_OK)
            {
                std::string err = PQerrorMessage(db.native());
                PQclear(res);
                throw std::runtime_error("COMMIT failed: " + err);
            }
            PQclear(res);
        }
        
        static void rollback(PostgreSQLLiveDB& db)
        {
            PGresult* res = PQexec(db.native(), "ROLLBACK");
            if (PQresultStatus(res) != PGRES_COMMAND_OK)
            {
                std::string err = PQerrorMessage(db.native());
                PQclear(res);
                throw std::runtime_error("ROLLBACK failed: " + err);
            }
            PQclear(res);
        }

    private:
        static void exec_no_result(
            PostgreSQLLiveDB& db,
            const std::string& sql,
            const std::vector<std::string>& vals)
        {
            // Prepare parameter arrays for PQexecParams
            std::vector<const char*> param_values;
            param_values.reserve(vals.size());
            for (const auto& v : vals)
                param_values.push_back(v.c_str());
            
            PGresult* res = PQexecParams(
                db.native(),
                sql.c_str(),
                static_cast<int>(vals.size()),
                nullptr,  // let server infer param types
                param_values.data(),
                nullptr,  // text format
                nullptr,  // text format
                0         // result in text format
            );
            
            ExecStatusType status = PQresultStatus(res);
            if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK)
            {
                std::string err = PQerrorMessage(db.native());
                PQclear(res);
                throw std::runtime_error("PostgreSQL exec failed: " + err);
            }
            PQclear(res);
        }

        // Build the SELECT SQL. Inferred = multi-table relationship query with no
        // explicit .join(): table-qualified columns + the inferred JOIN chain (shared
        // renderer). Otherwise the bare single-table form.
        template <typename Response, bool Inferred, typename Query>
        static std::string build_select_sql(const Query& q, pg_live_detail::RenderCtx& ctx)
        {
            if constexpr (Inferred)
            {
                using Base = orm::detail::base_table_t<Response>;
                return std::format("SELECT {} FROM {}{}{}{}{}",
                    pg_live_detail::render_qualified_columns<Response>(),
                    std::string(table_name<Base>()),
                    pg_live_detail::render_inferred_joins<Response>(),
                    pg_live_detail::render_wheres(q.where_clauses(), ctx),
                    pg_live_detail::render_order_by(q.order_clauses()),
                    pg_live_detail::render_limits(q.limit_clauses()));
            }
            else
            {
                using Entity = typename Response::template orm_type<0>::table_type;
                return std::format("SELECT {} FROM {}{}{}{}",
                    pg_live_detail::render_columns(q.selected_properties()),
                    std::string(table_name<Entity>()),
                    pg_live_detail::render_wheres(q.where_clauses(), ctx),
                    pg_live_detail::render_order_by(q.order_clauses()),
                    pg_live_detail::render_limits(q.limit_clauses()));
            }
        }

        // Run a SELECT and hydrate. Inferred → partial entities grouped into a
        // joined_row_for<Response>; otherwise a flat projected_type<Response> tuple.
        template <typename Response, bool Inferred>
        static auto exec_select(
            PostgreSQLLiveDB& db,
            const std::string& sql,
            const std::vector<std::string>& vals)
        {
            std::vector<const char*> param_values;
            param_values.reserve(vals.size());
            for (const auto& v : vals)
                param_values.push_back(v.c_str());

            PGresult* res = PQexecParams(
                db.native(),
                sql.c_str(),
                static_cast<int>(vals.size()),
                nullptr,
                param_values.data(),
                nullptr,
                nullptr,
                0
            );

            if (PQresultStatus(res) != PGRES_TUPLES_OK)
            {
                std::string err = PQerrorMessage(db.native());
                PQclear(res);
                throw std::runtime_error("PostgreSQL SELECT failed: " + err);
            }

            const int nrows = PQntuples(res);

            if constexpr (Inferred)
            {
                using Flat = projected_type<Response>;
                using Row  = joined_row_for<Response>;
                std::vector<Row> rows;
                rows.reserve(nrows);
                for (int i = 0; i < nrows; ++i)
                    rows.push_back(orm::hydrate_joined<Response>(hydrate_row<Flat>(res, i)));
                PQclear(res);
                return result<Row, Response>{ std::move(rows) };
            }
            else
            {
                using Row = projected_type<Response>;
                std::vector<Row> rows;
                rows.reserve(nrows);
                for (int i = 0; i < nrows; ++i)
                    rows.push_back(hydrate_row<Row>(res, i));
                PQclear(res);
                return result<Row, Response>{ std::move(rows) };
            }
        }

        template <typename Row>
        static auto hydrate_row(PGresult* res, int row_idx) -> Row
        {
            return hydrate_impl<Row>(res, row_idx, std::make_index_sequence<std::tuple_size_v<Row>>{});
        }

        template <typename Row, std::size_t... Is>
        static auto hydrate_impl(PGresult* res, int row_idx, std::index_sequence<Is...>) -> Row
        {
            return Row{ pg_live_detail::convert_field<std::tuple_element_t<Is, Row>>(res, row_idx, Is)... };
        }
    };

} // namespace orm

#endif // ORM_POSTGRESQL_LIVE_AVAILABLE
