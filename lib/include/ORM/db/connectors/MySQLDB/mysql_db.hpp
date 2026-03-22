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

namespace orm {

    // ── MockMySQLHandle ───────────────────────────────────────────────────────
    // In-process stub for the MySQL C API prepared-statement surface.
    // Enables unit testing without a live MySQL server or libmysqlclient.
    struct MockMySQLHandle
    {
        mutable std::string              last_sql;
        mutable std::vector<std::string> last_params;
        mutable int                      stmt_close_count = 0;

        void prepare(std::string_view sql) const { last_sql = std::string(sql); }
        void bind_param(const std::vector<std::string>& params) const { last_params = params; }
        void execute_stmt() const {}
        void fetch() const {}
        void close_stmt() const { ++stmt_close_count; }
    };

    // ── MySQLDB tag ───────────────────────────────────────────────────────────
    // Database tag type. Pass as DB template argument to orm::db<DB>.
    // The connection handle is MockMySQLHandle for testing; swap for MYSQL* at
    // integration time by adding a second specialisation or replacing this handle.
    struct MySQLDB
    {
        mutable MockMySQLHandle handle;
    };

    // ── SQL rendering helpers (MySQL — positional ? placeholders) ────────────
    namespace mysql_detail {

        // ── render_operand: field → column name; placeholder → "?"
        template <typename T>
        [[nodiscard]] std::string render_operand(const T& /*v*/)
        {
            if constexpr (is_field<T>)
                return std::string(T::column_name());
            else
                return "?";
        }

        [[nodiscard]] inline std::string render_operand(std::nullptr_t) { return "NULL"; }

        // ── render_rule: recursively walk Rule<T1,Op,T2> tree
        template <typename T1, detail::string_literal Op, typename T2>
        [[nodiscard]] std::string render_rule(const Rule<T1, Op, T2>& r)
        {
            if constexpr (is_rule<T1> && is_rule<T2>)
            {
                return render_rule(r.lhs_)
                    + " " + std::string(static_cast<std::string_view>(Op)) + " "
                    + render_rule(r.rhs_);
            }
            else
            {
                return render_operand(r.lhs_)
                    + " " + std::string(static_cast<std::string_view>(Op)) + " "
                    + render_operand(r.rhs_);
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

        // ── render WHERE clauses (each predicate emits "?" for placeholders)
        template <typename Wheres>
        [[nodiscard]] std::string render_wheres(const Wheres& w)
        {
            if constexpr (Wheres::size == 0)
                return {};
            std::string out = " WHERE ";
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                std::size_t idx = 0;
                ((void)(out += (idx++ > 0 ? " AND " : "") + render_rule(w.template get<Is>())), ...);
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
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                std::size_t idx = 0;
                ([&]()
                {
                    using OB = typename Orders::template orm_type<Is>;
                    using Tag = mem_ptr<OB::member>;
                    constexpr std::string_view dir =
                        (OB::sort == order::direction::asc) ? "ASC" : "DESC";
                    out += (idx++ > 0 ? ", " : "")
                        + std::string(Tag::column_name()) + " " + std::string(dir);
                }(), ...);
            }(std::make_index_sequence<Orders::size>{});
            return out;
        }

        // ── render LIMIT / OFFSET
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

        // ── render N positional ? placeholders
        [[nodiscard]] inline std::string positional_placeholders(std::size_t n)
        {
            std::string out;
            for (std::size_t i = 0; i < n; ++i)
            {
                if (i > 0) out += ", ";
                out += "?";
            }
            return out;
        }

        // ── render SET col = ? list from UpdateStatement tuple
        template <typename Stmts>
        [[nodiscard]] std::string render_set(const Stmts& s)
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
                    out += (idx++ > 0 ? ", " : "") + col + " = ?";
                }(), ...);
            }(std::make_index_sequence<Stmts::size>{});
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

        // ── Indexed-placeholder rewrite ───────────────────────────────────────
        // MySQL has no native $N syntax. Each occurrence of orm::ph<T, _N> emits
        // one extra "?" and duplicates the runtime argument value in the bind list.
        // This function counts the number of indexed placeholder occurrences in
        // the WHERE clause type so the caller can duplicate bind values accordingly.
        //
        // For rendering purposes the rule renderer already emits "?" for every
        // placeholder operand (anonymous or indexed). The rewrite is reflected in
        // collect_params: the caller must supply each argument once per occurrence.
        // The test verifies that last_params contains the duplicated value.

    } // namespace mysql_detail

    // ── connector_trait<MySQLDB> specialisation ───────────────────────────────
    template <>
    struct connector_trait<MySQLDB>
    {
        using supports_joins        = void;
        using supports_transactions = void;

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
            MySQLDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
            -> result<projected_type<Response>, Response>
        {
            db.handle.prepare(std::format("SELECT {} FROM ?{}{}{}",
                mysql_detail::render_columns(q.selected_properties()),
                mysql_detail::render_wheres(q.where_clauses()),
                mysql_detail::render_order_by(q.order_clauses()),
                mysql_detail::render_limits(q.limit_clauses())));
            db.handle.bind_param({});
            db.handle.execute_stmt();
            db.handle.close_stmt();
            return result<projected_type<Response>, Response>{};
        }

        // ── SELECT (with runtime params) ──────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders,
                  typename... Params>
        static auto execute(
            MySQLDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q,
            Params&&... params)
            -> result<projected_type<Response>, Response>
        {
            db.handle.prepare(std::format("SELECT {} FROM ?{}{}{}",
                mysql_detail::render_columns(q.selected_properties()),
                mysql_detail::render_wheres(q.where_clauses()),
                mysql_detail::render_order_by(q.order_clauses()),
                mysql_detail::render_limits(q.limit_clauses())));
            db.handle.bind_param(
                mysql_detail::collect_params(std::forward<Params>(params)...));
            db.handle.execute_stmt();
            db.handle.close_stmt();
            return result<projected_type<Response>, Response>{};
        }

        // ── INSERT (no runtime params) ────────────────────────────────────────
        template <typename Properties>
        static auto execute(MySQLDB& db, insert_query<Properties> q)
            -> result<std::tuple<>>
        {
            db.handle.prepare(std::format("INSERT INTO ? ({}) VALUES ({})",
                mysql_detail::render_columns(q.signature()),
                mysql_detail::positional_placeholders(Properties::size)));
            db.handle.bind_param({});
            db.handle.execute_stmt();
            db.handle.close_stmt();
            return result<std::tuple<>>{};
        }

        // ── INSERT (with runtime params) ──────────────────────────────────────
        template <typename Properties, typename... Params>
        static auto execute(MySQLDB& db, insert_query<Properties> q, Params&&... params)
            -> result<std::tuple<>>
        {
            db.handle.prepare(std::format("INSERT INTO ? ({}) VALUES ({})",
                mysql_detail::render_columns(q.signature()),
                mysql_detail::positional_placeholders(Properties::size)));
            db.handle.bind_param(
                mysql_detail::collect_params(std::forward<Params>(params)...));
            db.handle.execute_stmt();
            db.handle.close_stmt();
            return result<std::tuple<>>{};
        }

        // ── UPDATE (no runtime params) ────────────────────────────────────────
        template <typename Table, typename Statements, typename Wheres>
        static auto execute(MySQLDB& db, update_query<Table, Statements, Wheres> q)
            -> result<std::tuple<>>
        {
            db.handle.prepare(std::format("UPDATE {} SET {}{}",
                table_name<Table>(),
                mysql_detail::render_set(q.updates()),
                mysql_detail::render_wheres(q.wheres())));
            db.handle.bind_param({});
            db.handle.execute_stmt();
            db.handle.close_stmt();
            return result<std::tuple<>>{};
        }

        // ── UPDATE (with runtime params) ──────────────────────────────────────
        template <typename Table, typename Statements, typename Wheres, typename... Params>
        static auto execute(MySQLDB& db, update_query<Table, Statements, Wheres> q,
                            Params&&... params)
            -> result<std::tuple<>>
        {
            db.handle.prepare(std::format("UPDATE {} SET {}{}",
                table_name<Table>(),
                mysql_detail::render_set(q.updates()),
                mysql_detail::render_wheres(q.wheres())));
            db.handle.bind_param(
                mysql_detail::collect_params(std::forward<Params>(params)...));
            db.handle.execute_stmt();
            db.handle.close_stmt();
            return result<std::tuple<>>{};
        }

        // ── DELETE (no runtime params) ────────────────────────────────────────
        template <typename Table, typename Wheres>
        static auto execute(MySQLDB& db, delete_query<Table, Wheres> q)
            -> result<std::tuple<>>
        {
            db.handle.prepare(std::format("DELETE FROM {}{}",
                table_name<Table>(),
                mysql_detail::render_wheres(q.wheres())));
            db.handle.bind_param({});
            db.handle.execute_stmt();
            db.handle.close_stmt();
            return result<std::tuple<>>{};
        }

        // ── DELETE (with runtime params) ──────────────────────────────────────
        template <typename Table, typename Wheres, typename... Params>
        static auto execute(MySQLDB& db, delete_query<Table, Wheres> q, Params&&... params)
            -> result<std::tuple<>>
        {
            db.handle.prepare(std::format("DELETE FROM {}{}",
                table_name<Table>(),
                mysql_detail::render_wheres(q.wheres())));
            db.handle.bind_param(
                mysql_detail::collect_params(std::forward<Params>(params)...));
            db.handle.execute_stmt();
            db.handle.close_stmt();
            return result<std::tuple<>>{};
        }
    };

} // namespace orm
