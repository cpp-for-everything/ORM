#pragma once
#include "ORM/connector/capabilities.hpp"
#include "ORM/result/result.hpp"
#include "ORM/query/select.hpp"
#include "ORM/query/insert.hpp"
#include "ORM/query/update.hpp"
#include "ORM/query/delete.hpp"
#include "ORM/query/field.hpp"
#include "ORM/query/join_infer.hpp"
#include "ORM/entity/table.hpp"
#include <string>
#include <format>
#include <functional>
#include <vector>

namespace orm {

    // ── MockDB tag ────────────────────────────────────────────────────────────
    // In-memory SQL-rendering connector used for testing and documentation.
    // Stores the generated SQL string for test inspection.
    struct MockDB
    {
        mutable std::string       last_sql;
        mutable std::vector<std::string> last_params;
    };

    // ── SQL renderer helpers ──────────────────────────────────────────────────
    namespace mockdb {

        // ── render_operand: mem_ptr<Ptr> → column name; placeholder → "?" or "?N"
        template <typename T>
        [[nodiscard]] std::string render_operand(const T& /*v*/)
        {
            if constexpr (is_field<T>)
                return std::string(T::column_name());
            else if constexpr (is_placeholder_v<T>)
            {
                constexpr int idx = placeholder_index_v<T>;
                if constexpr (idx > 0)
                    return std::string("?") + std::to_string(idx);
                else
                    return "?";
            }
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

        // ── render a comma-separated list of bare column names from an orm_tuple of
        // mem_ptr<> (single-table SELECT, plus INSERT/UPDATE column lists). Multi-table
        // SELECTs use render_qualified_columns<Response>() below.
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

        // ── render WHERE clauses
        // Clang 18 ICE: empty-pack generic-lambda after `if constexpr (size==0)
        // return {}` without `else` still instantiates the lambda (isPackExpansion).
        template <typename Wheres>
        [[nodiscard]] std::string render_wheres(const Wheres& w)
        {
            if constexpr (Wheres::size == 0)
            {
                return {};
            }
            else
            {
                std::string out = " WHERE ";
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    std::size_t idx = 0;
                    ((void)(out += (idx++ > 0 ? " AND " : "") + render_rule(w.template get<Is>())), ...);
                }(std::make_index_sequence<Wheres::size>{});
                return out;
            }
        }

        // ── render JOIN clauses
        // Clang 18 ICE: nested IIFE generic-lambda + pack (even non-empty) crashes
        // in TemplateArgument::isPackExpansion. Use a free function template instead.
        template <typename Joins, std::size_t I>
        void append_join_item(std::string& out, const Joins& j)
        {
            const auto& jr = j.template get<I>();
            using JR = std::remove_cvref_t<decltype(jr)>;
            std::string_view kind;
            if constexpr (JR::mode == join::mode::inner)       kind = "INNER JOIN";
            else if constexpr (JR::mode == join::mode::left)   kind = "LEFT JOIN";
            else if constexpr (JR::mode == join::mode::right)  kind = "RIGHT JOIN";
            else                                                kind = "FULL JOIN";
            out += std::string(" ") + std::string(kind) + " ? ON " + render_rule(jr.to_rule());
        }

        template <typename Joins, std::size_t... Is>
        [[nodiscard]] std::string render_joins_impl(const Joins& j, std::index_sequence<Is...>)
        {
            std::string out;
            (append_join_item<Joins, Is>(out, j), ...);
            return out;
        }

        template <typename Joins>
        [[nodiscard]] std::string render_joins(const Joins& j)
        {
            if constexpr (Joins::size == 0)
            {
                return {};
            }
            else
            {
                return render_joins_impl(j, std::make_index_sequence<Joins::size>{});
            }
        }

        // ── render GROUP BY
        template <typename Groups, std::size_t I>
        void append_group_by_item(std::string& out, bool& first)
        {
            using GB = typename Groups::template orm_type<I>;
            using Tag = mem_ptr<GB::member>;
            if (!first) out += ", ";
            out += std::string(Tag::column_name());
            first = false;
        }

        template <typename Groups, std::size_t... Is>
        [[nodiscard]] std::string render_group_by_impl(std::index_sequence<Is...>)
        {
            std::string out = " GROUP BY ";
            bool first = true;
            (append_group_by_item<Groups, Is>(out, first), ...);
            return out;
        }

        template <typename Groups>
        [[nodiscard]] std::string render_group_by(const Groups& /*g*/)
        {
            if constexpr (Groups::size == 0)
            {
                return {};
            }
            else
            {
                return render_group_by_impl<Groups>(std::make_index_sequence<Groups::size>{});
            }
        }

        // ── render ORDER BY col [ASC|DESC], ...
        template <typename Orders, std::size_t I>
        void append_order_by_item(std::string& out, bool& first)
        {
            using OB = typename Orders::template orm_type<I>;
            using Tag = mem_ptr<OB::member>;
            constexpr std::string_view dir =
                (OB::sort == order::direction::asc) ? "ASC" : "DESC";
            if (!first) out += ", ";
            out += std::string(Tag::column_name()) + " " + std::string(dir);
            first = false;
        }

        template <typename Orders, std::size_t... Is>
        [[nodiscard]] std::string render_order_by_impl(std::index_sequence<Is...>)
        {
            std::string out = " ORDER BY ";
            bool first = true;
            (append_order_by_item<Orders, Is>(out, first), ...);
            return out;
        }

        template <typename Orders>
        [[nodiscard]] std::string render_order_by(const Orders& /*o*/)
        {
            if constexpr (Orders::size == 0)
            {
                return {};
            }
            else
            {
                return render_order_by_impl<Orders>(std::make_index_sequence<Orders::size>{});
            }
        }

        // ── render LIMIT / OFFSET — guarded so get<0> is only called when size>0
        template <typename Limits>
        [[nodiscard]] std::string render_limits(const Limits& lims)
        {
            if constexpr (Limits::size == 0)
            {
                return {};
            }
            else
            {
                const auto& p = lims.template get<0>();
                return std::format(" LIMIT {} OFFSET {}",
                    p.get_elements_per_page(),
                    p.get_elements_per_page() * p.get_number_of_page());
            }
        }

        // ── assemble the full SELECT (relationship-aware) ──────────────────────
        // A query that spans >1 table WITHOUT explicit .join() clauses gets
        // relationship-driven JOIN inference: columns are table-qualified, the
        // inferred JOIN chain is rendered, and the base table name is used.
        // Single-table queries and explicit-.join() queries render exactly as
        // before (`FROM ?`, bare columns, explicit joins).
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders>
        [[nodiscard]] std::string render_select(
            const select_query<Response, Joins, Wheres, Limits, Groups, Orders>& q)
        {
            if constexpr (orm::detail::is_multi_table<Response> && Joins::size == 0)
            {
                using Base = orm::detail::base_table_t<Response>;
                return std::format("SELECT {} FROM {}{}{}{}{}{}",
                    render_qualified_columns<Response>(),
                    std::string(orm::table_name<Base>()),
                    render_inferred_joins<Response>(),
                    render_wheres(q.where_clauses()),
                    render_group_by(q.group_clauses()),
                    render_order_by(q.order_clauses()),
                    render_limits(q.limit_clauses()));
            }
            else
            {
                return std::format("SELECT {} FROM ?{}{}{}{}{}",
                    render_columns(q.selected_properties()),
                    render_joins(q.join_clauses()),
                    render_wheres(q.where_clauses()),
                    render_group_by(q.group_clauses()),
                    render_order_by(q.order_clauses()),
                    render_limits(q.limit_clauses()));
            }
        }

        // ── render N placeholder question marks
        [[nodiscard]] inline std::string placeholders(std::size_t n)
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
        template <typename Stmts, std::size_t I>
        void append_set_item(std::string& out, std::size_t& idx, const Stmts& s)
        {
            const auto& stmt = s.template get<I>();
            using StmtT = std::remove_cvref_t<decltype(stmt)>;
            const std::string col = std::string(StmtT::field_tag::column_name());
            out += (idx++ > 0 ? ", " : "") + col + " = ?";
        }

        template <typename Stmts, std::size_t... Is>
        [[nodiscard]] std::string render_set_impl(const Stmts& s, std::index_sequence<Is...>)
        {
            std::string out;
            std::size_t idx = 0;
            (append_set_item<Stmts, Is>(out, idx, s), ...);
            return out;
        }

        template <typename Stmts>
        [[nodiscard]] std::string render_set(const Stmts& s)
        {
            if constexpr (Stmts::size == 0)
            {
                return {};
            }
            else
            {
                return render_set_impl(s, std::make_index_sequence<Stmts::size>{});
            }
        }

        // ── stringify a runtime parameter for last_params storage ──────────
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
            else if constexpr (std::is_same_v<D, const char8_t*> || std::is_same_v<D, char8_t*>)
                return std::string(reinterpret_cast<const char*>(v));
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

    } // namespace mockdb

    // ── connector_trait<MockDB> specialisation ────────────────────────────────
    template <>
    struct connector_trait<MockDB>
    {
        using supports_joins               = void;
        using supports_transactions        = void;
        using supports_aggregation         = void;
        using supports_upsert              = void;
        using supports_bulk_insert         = void;
        using supports_concurrent_execute  = void; // required by connection_pool<MockDB, N>
        using supports_constexpr_sql       = void; // required by wire_protocol constexpr SQL tests

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
            MockDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
            -> result<projected_type<Response>, Response>
        {
            db.last_sql = mockdb::render_select(q);
            db.last_params.clear();
            return result<projected_type<Response>, Response>{};
        }

        // ── SELECT (with runtime params) ──────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders,
                  typename... Params>
        static auto execute(
            MockDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q,
            Params&&... params)
            -> result<projected_type<Response>, Response>
        {
            db.last_sql = mockdb::render_select(q);
            db.last_params = mockdb::collect_params(std::forward<Params>(params)...);
            return result<projected_type<Response>, Response>{};
        }

        // ── INSERT (no runtime params) ─────────────────────────────────────
        template <typename Properties>
        static auto execute(MockDB& db, insert_query<Properties> q)
            -> result<std::tuple<>>
        {
            db.last_sql = std::format("INSERT INTO ? ({}) VALUES ({})",
                mockdb::render_columns(q.signature()),
                mockdb::placeholders(Properties::size));
            db.last_params.clear();
            return result<std::tuple<>>{};
        }

        // ── INSERT (with runtime params) ──────────────────────────────────────
        template <typename Properties, typename... Params>
        static auto execute(MockDB& db, insert_query<Properties> q, Params&&... params)
            -> result<std::tuple<>>
        {
            db.last_sql = std::format("INSERT INTO ? ({}) VALUES ({})",
                mockdb::render_columns(q.signature()),
                mockdb::placeholders(Properties::size));
            db.last_params = mockdb::collect_params(std::forward<Params>(params)...);
            return result<std::tuple<>>{};
        }

        // ── UPDATE (no runtime params) ────────────────────────────────────────
        template <typename Table, typename Statements, typename Wheres>
        static auto execute(MockDB& db, update_query<Table, Statements, Wheres> q)
            -> result<std::tuple<>>
        {
            db.last_sql = std::format("UPDATE {} SET {}{}",
                table_name<Table>(),
                mockdb::render_set(q.updates()),
                mockdb::render_wheres(q.wheres()));
            db.last_params.clear();
            return result<std::tuple<>>{};
        }

        // ── UPDATE (with runtime params) ──────────────────────────────────────
        template <typename Table, typename Statements, typename Wheres, typename... Params>
        static auto execute(MockDB& db, update_query<Table, Statements, Wheres> q, Params&&... params)
            -> result<std::tuple<>>
        {
            db.last_sql = std::format("UPDATE {} SET {}{}",
                table_name<Table>(),
                mockdb::render_set(q.updates()),
                mockdb::render_wheres(q.wheres()));
            db.last_params = mockdb::collect_params(std::forward<Params>(params)...);
            return result<std::tuple<>>{};
        }

        // ── DELETE (no runtime params) ────────────────────────────────────────
        template <typename Table, typename Wheres>
        static auto execute(MockDB& db, delete_query<Table, Wheres> q)
            -> result<std::tuple<>>
        {
            db.last_sql = std::format("DELETE FROM {}{}",
                table_name<Table>(),
                mockdb::render_wheres(q.wheres()));
            db.last_params.clear();
            return result<std::tuple<>>{};
        }

        // ── DELETE (with runtime params) ──────────────────────────────────────
        template <typename Table, typename Wheres, typename... Params>
        static auto execute(MockDB& db, delete_query<Table, Wheres> q, Params&&... params)
            -> result<std::tuple<>>
        {
            db.last_sql = std::format("DELETE FROM {}{}",
                table_name<Table>(),
                mockdb::render_wheres(q.wheres()));
            db.last_params = mockdb::collect_params(std::forward<Params>(params)...);
            return result<std::tuple<>>{};
        }

        // ── Transaction control (for transaction_guard<MockDB>) ───────────────
        static void begin(MockDB& db)    { db.last_sql = "BEGIN"; }
        static void commit(MockDB& db)   { db.last_sql = "COMMIT"; }
        static void rollback(MockDB& db) { db.last_sql = "ROLLBACK"; }

        // ── Constexpr SQL renderer (for wire_protocol constexpr SQL tests) ────
        template <typename Query>
        static constexpr std::string_view render_constexpr(Query)
        {
            return "SELECT id FROM ?"; // representative static SQL string
        }
    };

} // namespace orm
