#pragma once
#include "ORM/connector/capabilities.hpp"
#include "ORM/result/result.hpp"
#include "ORM/query/select.hpp"
#include "ORM/query/insert.hpp"
#include "ORM/query/update.hpp"
#include "ORM/query/delete.hpp"
#include "ORM/query/field.hpp"
#include "ORM/query/placeholders.hpp"
#include "ORM/entity/table.hpp"
#include <string>
#include <format>
#include <vector>
#include <unordered_map>

namespace orm {

    // ── MockPGconn ────────────────────────────────────────────────────────────
    // In-process stub for the libpq prepared-statement surface.
    // Enables unit testing without a live PostgreSQL server or libpq.
    struct MockPGconn
    {
        mutable std::string              last_sql;
        mutable std::vector<std::string> last_params;
        mutable int                      pq_clear_count = 0;
        mutable int                      nparams_used   = 0;

        void prepare(std::string_view /*name*/, std::string_view sql, int nparams) const
        {
            last_sql     = std::string(sql);
            nparams_used = nparams;
        }
        void exec_prepared(std::string_view /*name*/, int nparams,
                           const std::vector<std::string>& params) const
        {
            nparams_used = nparams;
            last_params  = params;
        }
        void clear_result() const { ++pq_clear_count; }
    };

    // ── PGresultRAII ──────────────────────────────────────────────────────────
    // RAII wrapper ensuring clear_result() (i.e. PQclear) is called exactly once.
    struct PGresultRAII
    {
        explicit PGresultRAII(MockPGconn* conn) : conn_(conn) {}
        ~PGresultRAII() { if (conn_) conn_->clear_result(); }

        PGresultRAII(const PGresultRAII&)            = delete;
        PGresultRAII& operator=(const PGresultRAII&) = delete;
        PGresultRAII(PGresultRAII&& o) noexcept : conn_(o.conn_) { o.conn_ = nullptr; }

    private:
        MockPGconn* conn_;
    };

    // ── PostgreSQLDB tag ──────────────────────────────────────────────────────
    // Database tag type. Pass as DB template argument to orm::db<DB>.
    struct PostgreSQLDB
    {
        mutable MockPGconn conn;
    };

    // ── SQL rendering helpers (PostgreSQL — $N dollar parameters) ────────────
    namespace pg_detail {

        // ── Rendering context tracks the next $N counter and a map from
        //    indexed-placeholder index → assigned $M (for native reuse).
        struct RenderCtx
        {
            int                            next_param = 1; // 1-based
            std::unordered_map<int, int>   idx_map;        // ph index → assigned $M
            std::vector<std::string>       values;         // accumulated param values
        };

        // ── render_rule with context
        template <typename T1, detail::string_literal Op, typename T2>
        [[nodiscard]] std::string render_rule_ctx(const Rule<T1, Op, T2>& r, RenderCtx& ctx);

        template <typename T>
        [[nodiscard]] std::string render_leaf_ctx(const T& /*v*/, RenderCtx& ctx)
        {
            if constexpr (is_placeholder_v<T>)
            {
                constexpr int idx = placeholder_index_v<T>;
                if constexpr (idx > 0)
                {
                    // Indexed placeholder: assign $N on first occurrence; reuse on subsequent
                    auto it = ctx.idx_map.find(idx);
                    if (it == ctx.idx_map.end())
                    {
                        int n = ctx.next_param++;
                        ctx.idx_map[idx] = n;
                        return std::format("${}", n);
                    }
                    else
                    {
                        // Same $M emitted again — no new param added to values array
                        return std::format("${}", it->second);
                    }
                }
                else
                {
                    // Anonymous placeholder — always a new $N
                    return std::format("${}", ctx.next_param++);
                }
            }
            else if constexpr (is_field<T>)
            {
                return std::string(T::column_name());
            }
            else
            {
                return "NULL";
            }
        }

        template <typename T1, detail::string_literal Op, typename T2>
        [[nodiscard]] std::string render_rule_ctx(const Rule<T1, Op, T2>& r, RenderCtx& ctx)
        {
            if constexpr (is_rule<T1> && is_rule<T2>)
            {
                return render_rule_ctx(r.lhs_, ctx)
                    + " " + std::string(static_cast<std::string_view>(Op)) + " "
                    + render_rule_ctx(r.rhs_, ctx);
            }
            else
            {
                return render_leaf_ctx(r.lhs_, ctx)
                    + " " + std::string(static_cast<std::string_view>(Op)) + " "
                    + render_leaf_ctx(r.rhs_, ctx);
            }
        }

        // ── render a comma-separated list of column names
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

        // ── render WHERE clauses with $N context
        template <typename Wheres>
        [[nodiscard]] std::string render_wheres_ctx(const Wheres& w, RenderCtx& ctx)
        {
            if constexpr (Wheres::size == 0)
                return {};
            std::string out = " WHERE ";
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                std::size_t i = 0;
                ((void)(out += (i++ > 0 ? " AND " : "") + render_rule_ctx(w.template get<Is>(), ctx)), ...);
            }(std::make_index_sequence<Wheres::size>{});
            return out;
        }

        // ── render ORDER BY
        template <typename Orders>
        [[nodiscard]] std::string render_order_by(const Orders& /*o*/)
        {
            if constexpr (Orders::size == 0)
                return {};
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

        // ── render LIMIT
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

        // ── render SET col = $N list
        template <typename Stmts>
        [[nodiscard]] std::string render_set_ctx(const Stmts& s, RenderCtx& ctx)
        {
            if constexpr (Stmts::size == 0)
                return {};
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

        // ── render N $1,$2,... placeholders for INSERT
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

        // ── stringify a runtime parameter value
        template <typename T>
        [[nodiscard]] std::string stringify_param(const T& v)
        {
            using D = std::decay_t<T>;
            if constexpr (std::is_arithmetic_v<D>)
                return std::to_string(v);
            else if constexpr (std::is_same_v<D, std::string>)
                return v;
            else if constexpr (std::is_same_v<D, std::u8string>)
                return std::string(reinterpret_cast<const char*>(v.data()), v.size());
            else if constexpr (std::is_same_v<D, const char*> || std::is_same_v<D, char*>)
                return std::string(v);
            else
                return "?";
        }

        template <typename... Params>
        std::vector<std::string> collect_params(Params&&... params)
        {
            std::vector<std::string> out;
            out.reserve(sizeof...(params));
            (out.push_back(stringify_param(params)), ...);
            return out;
        }

    } // namespace pg_detail

    // ── connector_trait<PostgreSQLDB> specialisation ──────────────────────────
    template <>
    struct connector_trait<PostgreSQLDB>
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
            PostgreSQLDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
            -> result<projected_type<Response>, Response>
        {
            pg_detail::RenderCtx ctx;
            std::string where_sql = pg_detail::render_wheres_ctx(q.where_clauses(), ctx);
            std::string sql = std::format("SELECT {} FROM ?{}{}{}",
                pg_detail::render_columns(q.selected_properties()),
                where_sql,
                pg_detail::render_order_by(q.order_clauses()),
                pg_detail::render_limits(q.limit_clauses()));
            int nparams = ctx.next_param - 1;
            db.conn.prepare("stmt", sql, nparams);
            db.conn.exec_prepared("stmt", nparams, {});
            {
                PGresultRAII raii{&db.conn};
                (void)raii;
            }
            return result<projected_type<Response>, Response>{};
        }

        // ── SELECT (with runtime params) ──────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders,
                  typename... Params>
        static auto execute(
            PostgreSQLDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q,
            Params&&... params)
            -> result<projected_type<Response>, Response>
        {
            pg_detail::RenderCtx ctx;
            std::string where_sql = pg_detail::render_wheres_ctx(q.where_clauses(), ctx);
            std::string sql = std::format("SELECT {} FROM ?{}{}{}",
                pg_detail::render_columns(q.selected_properties()),
                where_sql,
                pg_detail::render_order_by(q.order_clauses()),
                pg_detail::render_limits(q.limit_clauses()));
            int nparams = ctx.next_param - 1;
            auto values = pg_detail::collect_params(std::forward<Params>(params)...);
            db.conn.prepare("stmt", sql, nparams);
            db.conn.exec_prepared("stmt", nparams, values);
            {
                PGresultRAII raii{&db.conn};
                (void)raii;
            }
            return result<projected_type<Response>, Response>{};
        }

        // ── INSERT (no runtime params) ────────────────────────────────────────
        template <typename Properties>
        static auto execute(PostgreSQLDB& db, insert_query<Properties> q)
            -> result<std::tuple<>>
        {
            std::string sql = std::format("INSERT INTO ? ({}) VALUES ({})",
                pg_detail::render_columns(q.signature()),
                pg_detail::dollar_placeholders(Properties::size));
            db.conn.prepare("stmt", sql, static_cast<int>(Properties::size));
            db.conn.exec_prepared("stmt", static_cast<int>(Properties::size), {});
            {
                PGresultRAII raii{&db.conn};
                (void)raii;
            }
            return result<std::tuple<>>{};
        }

        // ── INSERT (with runtime params) ──────────────────────────────────────
        template <typename Properties, typename... Params>
        static auto execute(PostgreSQLDB& db, insert_query<Properties> q, Params&&... params)
            -> result<std::tuple<>>
        {
            std::string sql = std::format("INSERT INTO ? ({}) VALUES ({})",
                pg_detail::render_columns(q.signature()),
                pg_detail::dollar_placeholders(Properties::size));
            auto values = pg_detail::collect_params(std::forward<Params>(params)...);
            db.conn.prepare("stmt", sql, static_cast<int>(Properties::size));
            db.conn.exec_prepared("stmt", static_cast<int>(Properties::size), values);
            {
                PGresultRAII raii{&db.conn};
                (void)raii;
            }
            return result<std::tuple<>>{};
        }

        // ── UPDATE (no runtime params) ────────────────────────────────────────
        template <typename Table, typename Statements, typename Wheres>
        static auto execute(PostgreSQLDB& db, update_query<Table, Statements, Wheres> q)
            -> result<std::tuple<>>
        {
            pg_detail::RenderCtx ctx;
            std::string sql = std::format("UPDATE {} SET {}{}",
                table_name<Table>(),
                pg_detail::render_set_ctx(q.updates(), ctx),
                pg_detail::render_wheres_ctx(q.wheres(), ctx));
            int nparams = ctx.next_param - 1;
            db.conn.prepare("stmt", sql, nparams);
            db.conn.exec_prepared("stmt", nparams, {});
            {
                PGresultRAII raii{&db.conn};
                (void)raii;
            }
            return result<std::tuple<>>{};
        }

        // ── UPDATE (with runtime params) ──────────────────────────────────────
        template <typename Table, typename Statements, typename Wheres, typename... Params>
        static auto execute(PostgreSQLDB& db, update_query<Table, Statements, Wheres> q,
                            Params&&... params)
            -> result<std::tuple<>>
        {
            pg_detail::RenderCtx ctx;
            std::string sql = std::format("UPDATE {} SET {}{}",
                table_name<Table>(),
                pg_detail::render_set_ctx(q.updates(), ctx),
                pg_detail::render_wheres_ctx(q.wheres(), ctx));
            int nparams = ctx.next_param - 1;
            auto values = pg_detail::collect_params(std::forward<Params>(params)...);
            db.conn.prepare("stmt", sql, nparams);
            db.conn.exec_prepared("stmt", nparams, values);
            {
                PGresultRAII raii{&db.conn};
                (void)raii;
            }
            return result<std::tuple<>>{};
        }

        // ── DELETE (no runtime params) ────────────────────────────────────────
        template <typename Table, typename Wheres>
        static auto execute(PostgreSQLDB& db, delete_query<Table, Wheres> q)
            -> result<std::tuple<>>
        {
            pg_detail::RenderCtx ctx;
            std::string sql = std::format("DELETE FROM {}{}",
                table_name<Table>(),
                pg_detail::render_wheres_ctx(q.wheres(), ctx));
            int nparams = ctx.next_param - 1;
            db.conn.prepare("stmt", sql, nparams);
            db.conn.exec_prepared("stmt", nparams, {});
            {
                PGresultRAII raii{&db.conn};
                (void)raii;
            }
            return result<std::tuple<>>{};
        }

        // ── DELETE (with runtime params) ──────────────────────────────────────
        template <typename Table, typename Wheres, typename... Params>
        static auto execute(PostgreSQLDB& db, delete_query<Table, Wheres> q, Params&&... params)
            -> result<std::tuple<>>
        {
            pg_detail::RenderCtx ctx;
            std::string sql = std::format("DELETE FROM {}{}",
                table_name<Table>(),
                pg_detail::render_wheres_ctx(q.wheres(), ctx));
            int nparams = ctx.next_param - 1;
            auto values = pg_detail::collect_params(std::forward<Params>(params)...);
            db.conn.prepare("stmt", sql, nparams);
            db.conn.exec_prepared("stmt", nparams, values);
            {
                PGresultRAII raii{&db.conn};
                (void)raii;
            }
            return result<std::tuple<>>{};
        }
    };

} // namespace orm
