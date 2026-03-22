#pragma once
#include "ORM/connector/capabilities.hpp"
#include "ORM/result/result.hpp"
#include "ORM/query/select.hpp"
#include "ORM/query/insert.hpp"
#include "ORM/query/update.hpp"
#include "ORM/query/delete.hpp"
#include "ORM/query/field.hpp"
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

        // ── render a comma-separated list of column names from an orm_tuple of mem_ptr<>
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

        // ── render WHERE clauses
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

        // ── render JOIN clauses
        template <typename Joins>
        [[nodiscard]] std::string render_joins(const Joins& j)
        {
            if constexpr (Joins::size == 0)
                return {};
            std::string out;
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                ([&]()
                {
                    const auto& jr = j.template get<Is>();
                    using JR = std::remove_cvref_t<decltype(jr)>;
                    std::string_view kind;
                    if constexpr (JR::mode == join::mode::inner)       kind = "INNER JOIN";
                    else if constexpr (JR::mode == join::mode::left)   kind = "LEFT JOIN";
                    else if constexpr (JR::mode == join::mode::right)  kind = "RIGHT JOIN";
                    else                                                kind = "FULL JOIN";
                    out += std::string(" ") + std::string(kind) + " ? ON " + render_rule(jr.to_rule());
                }(), ...);
            }(std::make_index_sequence<Joins::size>{});
            return out;
        }

        // ── render GROUP BY
        template <typename Groups>
        [[nodiscard]] std::string render_group_by(const Groups& /*g*/)
        {
            if constexpr (Groups::size == 0)
                return {};
            std::string out = " GROUP BY ";
            bool first = true;
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                ([&]()
                {
                    using GB = typename Groups::template orm_type<Is>;
                    using Tag = mem_ptr<GB::member>;
                    if (!first) out += ", ";
                    out += std::string(Tag::column_name());
                    first = false;
                }(), ...);
            }(std::make_index_sequence<Groups::size>{});
            return out;
        }

        // ── render ORDER BY col [ASC|DESC], ...
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
                    // field_tag is mem_ptr<Ptr>; its column_name() returns the property column name
                    const std::string col = std::string(StmtT::field_tag::column_name());
                    out += (idx++ > 0 ? ", " : "") + col + " = ?";
                }(), ...);
            }(std::make_index_sequence<Stmts::size>{});
            return out;
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
            db.last_sql = std::format("SELECT {} FROM ?{}{}{}{}{}",
                mockdb::render_columns(q.selected_properties()),
                mockdb::render_joins(q.join_clauses()),
                mockdb::render_wheres(q.where_clauses()),
                mockdb::render_group_by(q.group_clauses()),
                mockdb::render_order_by(q.order_clauses()),
                mockdb::render_limits(q.limit_clauses()));
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
            db.last_sql = std::format("SELECT {} FROM ?{}{}{}{}{}",
                mockdb::render_columns(q.selected_properties()),
                mockdb::render_joins(q.join_clauses()),
                mockdb::render_wheres(q.where_clauses()),
                mockdb::render_group_by(q.group_clauses()),
                mockdb::render_order_by(q.order_clauses()),
                mockdb::render_limits(q.limit_clauses()));
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
