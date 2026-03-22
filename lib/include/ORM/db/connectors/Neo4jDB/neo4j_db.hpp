#pragma once
#include "ORM/connector/capabilities.hpp"
#include "ORM/result/result.hpp"
#include "ORM/query/select.hpp"
#include "ORM/query/insert.hpp"
#include "ORM/query/delete.hpp"
#include "ORM/query/field.hpp"
#include "ORM/query/placeholders.hpp"
#include "ORM/entity/table.hpp"
#include "ORM/details/member_pointer.hpp"
#include <string>
#include <format>
#include <vector>
#include <unordered_map>

namespace orm {

    // ── MockResultStreamRAII ──────────────────────────────────────────────────
    // RAII stub for neo4j_result_stream_t*. Tracks close_results count.
    struct MockResultStreamRAII
    {
        mutable int* close_results_count = nullptr;
        explicit MockResultStreamRAII(int* count) : close_results_count(count) {}
        ~MockResultStreamRAII() { if (close_results_count) ++(*close_results_count); }

        MockResultStreamRAII(const MockResultStreamRAII&)            = delete;
        MockResultStreamRAII& operator=(const MockResultStreamRAII&) = delete;
        MockResultStreamRAII(MockResultStreamRAII&& o) noexcept
            : close_results_count(o.close_results_count) { o.close_results_count = nullptr; }
    };

    // ── MockNeo4jConnection ───────────────────────────────────────────────────
    // Stub for neo4j_connection_t*. Captures Cypher string and parameter map.
    struct MockNeo4jConnection
    {
        mutable std::string                              last_cypher;
        mutable std::unordered_map<std::string, std::string> last_params_map;
        mutable int                                      close_results_count = 0;

        void run(std::string_view cypher,
                 const std::unordered_map<std::string, std::string>& params) const
        {
            last_cypher     = std::string(cypher);
            last_params_map = params;
        }
        [[nodiscard]] MockResultStreamRAII get_stream() const
        {
            return MockResultStreamRAII{&close_results_count};
        }
    };

    // ── Neo4jDB tag ───────────────────────────────────────────────────────────
    // Database tag type. Pass as DB template argument to orm::db<DB>.
    struct Neo4jDB
    {
        mutable MockNeo4jConnection conn;
    };

    // ── Cypher rendering helpers ──────────────────────────────────────────────
    namespace neo4j_detail {

        struct CypherCtx
        {
            int next_param = 1;
            std::unordered_map<std::string, std::string> params;

            [[nodiscard]] std::string add_param(std::string val)
            {
                std::string key = "p" + std::to_string(next_param++);
                params[key] = std::move(val);
                return "$" + key;
            }
        };

        // ── Render columns as RETURN clause: n.col1, n.col2, ...
        template <typename Tuple>
        [[nodiscard]] std::string render_return(const Tuple& t)
        {
            std::string out;
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                std::size_t idx = 0;
                ((void)(out += (idx++ > 0 ? std::string(", ") : std::string{})
                    + std::string("n.") + std::string(t.template get<Is>().column_name())), ...);
            }(std::make_index_sequence<Tuple::size>{});
            return out;
        }

        // ── Render leaf of a WHERE predicate
        template <typename T>
        [[nodiscard]] std::string render_where_leaf(const T& /*v*/, CypherCtx& ctx)
        {
            if constexpr (is_field<T>)
                return std::string("n.") + std::string(T::column_name());
            else if constexpr (detail::is_raw_mem_ptr<T>)
                return std::string("n.") + std::string(detail::column_name_of<T>());
            else if constexpr (is_placeholder_v<T>)
                return ctx.add_param("?"); // value filled at runtime bind
            else
                return "NULL";
        }

        // ── Render WHERE rule tree
        template <typename T1, detail::string_literal Op, typename T2>
        [[nodiscard]] std::string render_where_rule(const Rule<T1, Op, T2>& r, CypherCtx& ctx)
        {
            if constexpr (is_rule<T1> && is_rule<T2>)
            {
                return render_where_rule(r.lhs_, ctx)
                    + " " + std::string(static_cast<std::string_view>(Op)) + " "
                    + render_where_rule(r.rhs_, ctx);
            }
            else
            {
                return render_where_leaf(r.lhs_, ctx)
                    + " " + std::string(static_cast<std::string_view>(Op)) + " "
                    + render_where_leaf(r.rhs_, ctx);
            }
        }

        // ── Render WHERE clause
        template <typename Wheres>
        [[nodiscard]] std::string render_where(const Wheres& w, CypherCtx& ctx)
        {
            if constexpr (Wheres::size == 0)
                return {};
            std::string out = " WHERE ";
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                std::size_t i = 0;
                ((void)(out += (i++ > 0 ? " AND " : "") + render_where_rule(w.template get<Is>(), ctx)), ...);
            }(std::make_index_sequence<Wheres::size>{});
            return out;
        }

        // ── Stringify param
        template <typename T>
        [[nodiscard]] std::string stringify(const T& v)
        {
            using D = std::decay_t<T>;
            if constexpr (std::is_arithmetic_v<D>)             return std::to_string(v);
            else if constexpr (std::is_same_v<D, std::string>) return v;
            else return "?";
        }

    } // namespace neo4j_detail

    // ── connector_trait<Neo4jDB> specialisation ───────────────────────────────
    template <>
    struct connector_trait<Neo4jDB>
    {
        // Graph database. No SQL aggregation at this connector layer.
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
            Neo4jDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
            -> result<projected_type<Response>, Response>
        {
            neo4j_detail::CypherCtx ctx;
            std::string where_clause = neo4j_detail::render_where(q.where_clauses(), ctx);
            std::string cypher = std::format("MATCH (n:{}) RETURN {}{}",
                "Label",
                neo4j_detail::render_return(q.selected_properties()),
                where_clause);
            db.conn.run(cypher, ctx.params);
            {
                auto s = db.conn.get_stream();
                (void)s;
            }
            return result<projected_type<Response>, Response>{};
        }

        // ── SELECT (with runtime params) ──────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders,
                  typename... Params>
        static auto execute(
            Neo4jDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q,
            Params&&... params)
            -> result<projected_type<Response>, Response>
        {
            neo4j_detail::CypherCtx ctx;
            std::string where_clause = neo4j_detail::render_where(q.where_clauses(), ctx);
            std::string cypher = std::format("MATCH (n:{}) RETURN {}{}",
                "Label",
                neo4j_detail::render_return(q.selected_properties()),
                where_clause);
            // Override placeholder values with runtime args
            int pi = 1;
            ([&](auto&& v)
            {
                std::string key = "p" + std::to_string(pi++);
                if (ctx.params.count(key))
                    ctx.params[key] = neo4j_detail::stringify(v);
            }(std::forward<Params>(params)), ...);
            db.conn.run(cypher, ctx.params);
            {
                auto s = db.conn.get_stream();
                (void)s;
            }
            return result<projected_type<Response>, Response>{};
        }

        // ── INSERT ────────────────────────────────────────────────────────────
        template <typename Properties>
        static auto execute(Neo4jDB& db, insert_query<Properties> /*q*/)
            -> result<std::tuple<>>
        {
            neo4j_detail::CypherCtx ctx;
            db.conn.run("CREATE (n:Label {})", ctx.params);
            {
                auto s = db.conn.get_stream();
                (void)s;
            }
            return result<std::tuple<>>{};
        }

        template <typename Properties, typename... Params>
        static auto execute(Neo4jDB& db, insert_query<Properties> /*q*/, Params&&... /*params*/)
            -> result<std::tuple<>>
        {
            neo4j_detail::CypherCtx ctx;
            db.conn.run("CREATE (n:Label {})", ctx.params);
            {
                auto s = db.conn.get_stream();
                (void)s;
            }
            return result<std::tuple<>>{};
        }
    };

} // namespace orm
