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
#include "ORM/details/member_pointer.hpp"
#include <string>
#include <format>
#include <vector>

namespace orm {

    // ── MockCursor ────────────────────────────────────────────────────────────
    // Stub for a mongoc_cursor_t*. Tracks RAII destroy count.
    struct MockCursor
    {
        mutable int cursor_destroy_count = 0;
        ~MockCursor() { ++cursor_destroy_count; }
    };

    // ── MockCollection ────────────────────────────────────────────────────────
    // Stub for mongoc_collection_t*. Captures rendered filter and projection.
    struct MockCollection
    {
        mutable std::string last_filter;
        mutable std::string last_projection;
        mutable int         cursor_destroy_count = 0;

        void find_with_opts(std::string_view filter, std::string_view projection) const
        {
            last_filter     = std::string(filter);
            last_projection = std::string(projection);
            ++cursor_destroy_count; // simulate cursor created + immediately destroyed
        }
    };

    // ── MongoDB tag ───────────────────────────────────────────────────────────
    // Database tag type. Pass as DB template argument to orm::db<DB>.
    // Carries no SQL state — document database; filter/projection rendered to JSON-like strings.
    struct MongoDB
    {
        mutable MockCollection coll;
    };

    // ── BSON rendering helpers ────────────────────────────────────────────────
    namespace mongo_detail {

        // ── render_filter: walk a Rule tree → BSON-like filter JSON string
        template <typename T1, detail::string_literal Op, typename T2>
        [[nodiscard]] std::string render_filter(const Rule<T1, Op, T2>& r);

        template <typename T>
        [[nodiscard]] std::string render_value(const T& /*v*/)
        {
            if constexpr (is_field<T>)
                return std::string(T::column_name());
            else if constexpr (detail::is_raw_mem_ptr<T>)
                return std::string(detail::column_name_of<T>());
            else if constexpr (is_placeholder_v<T>)
                return "\"$placeholder\"";
            else
                return "\"?\"";
        }

        // Leaf rendering: field OP placeholder → {"field":{"$op":value}}
        template <typename T1, detail::string_literal Op, typename T2>
        [[nodiscard]] std::string render_filter(const Rule<T1, Op, T2>& r)
        {
            constexpr std::string_view op_sv = static_cast<std::string_view>(Op);

            if constexpr (is_rule<T1> && is_rule<T2>)
            {
                // Compound: AND / OR
                if (op_sv == "AND")
                    return std::format("{{\"$and\":[{},{}]}}", render_filter(r.lhs_), render_filter(r.rhs_));
                else
                    return std::format("{{\"$or\":[{},{}]}}", render_filter(r.lhs_), render_filter(r.rhs_));
            }
            else
            {
                // Leaf: field OP placeholder
                std::string field_name;
                if constexpr (is_field<T1>)
                    field_name = std::string(T1::column_name());
                else if constexpr (detail::is_raw_mem_ptr<T1>)
                    field_name = std::string(detail::column_name_of<T1>());
                else
                    field_name = "field";

                std::string bson_op;
                if      (op_sv == "==") bson_op = "$eq";
                else if (op_sv == ">")  bson_op = "$gt";
                else if (op_sv == "<")  bson_op = "$lt";
                else if (op_sv == ">=") bson_op = "$gte";
                else if (op_sv == "<=") bson_op = "$lte";
                else if (op_sv == "!=") bson_op = "$ne";
                else                    bson_op = "$eq";

                return std::format("{{\"{}\":{{\"{}\":\"?\"}}}}", field_name, bson_op);
            }
        }

        // ── render_wheres: combine multiple WHERE rules into a single filter
        template <typename Wheres>
        [[nodiscard]] std::string render_wheres(const Wheres& w)
        {
            if constexpr (Wheres::size == 0)
                return "{}";
            else if constexpr (Wheres::size == 1)
                return render_filter(w.template get<0>());
            else
            {
                std::string arr;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    std::size_t idx = 0;
                    ((void)(arr += (idx++ > 0 ? "," : "") + render_filter(w.template get<Is>())), ...);
                }(std::make_index_sequence<Wheres::size>{});
                return std::format("{{\"$and\":[{}]}}", arr);
            }
        }

        // ── render_projection: {"col1":1,"col2":1,"_id":0}
        template <typename Tuple>
        [[nodiscard]] std::string render_projection(const Tuple& t, bool include_id)
        {
            std::string out = "{";
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                std::size_t idx = 0;
                ((void)(out += (idx++ > 0 ? "," : "")
                    + std::format("\"{}\":1", t.template get<Is>().column_name())), ...);
            }(std::make_index_sequence<Tuple::size>{});
            if (!include_id)
                out += (Tuple::size > 0 ? ",\"_id\":0" : "\"_id\":0");
            out += "}";
            return out;
        }

    } // namespace mongo_detail

    // ── connector_trait<MongoDB> specialisation ───────────────────────────────
    template <>
    struct connector_trait<MongoDB>
    {
        // MongoDB does not support SQL-style joins, SQL transactions, or SQL aggregation
        // at this connector layer. No capability tags declared.

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
            MongoDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
            -> result<projected_type<Response>, Response>
        {
            std::string filter     = mongo_detail::render_wheres(q.where_clauses());
            std::string projection = mongo_detail::render_projection(q.selected_properties(), false);
            db.coll.find_with_opts(filter, projection);
            return result<projected_type<Response>, Response>{};
        }

        // ── SELECT (with runtime params) ──────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders,
                  typename... Params>
        static auto execute(
            MongoDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q,
            Params&&... /*params*/)
            -> result<projected_type<Response>, Response>
        {
            std::string filter     = mongo_detail::render_wheres(q.where_clauses());
            std::string projection = mongo_detail::render_projection(q.selected_properties(), false);
            db.coll.find_with_opts(filter, projection);
            return result<projected_type<Response>, Response>{};
        }

        // ── INSERT (no runtime params) ────────────────────────────────────────
        template <typename Properties>
        static auto execute(MongoDB& db, insert_query<Properties> /*q*/)
            -> result<std::tuple<>>
        {
            db.coll.last_filter = "{}";
            return result<std::tuple<>>{};
        }

        // ── INSERT (with runtime params) ──────────────────────────────────────
        template <typename Properties, typename... Params>
        static auto execute(MongoDB& db, insert_query<Properties> /*q*/, Params&&... /*params*/)
            -> result<std::tuple<>>
        {
            db.coll.last_filter = "{}";
            return result<std::tuple<>>{};
        }

        // ── DELETE (no runtime params) ────────────────────────────────────────
        template <typename Table, typename Wheres>
        static auto execute(MongoDB& db, delete_query<Table, Wheres> q)
            -> result<std::tuple<>>
        {
            db.coll.last_filter = mongo_detail::render_wheres(q.wheres());
            return result<std::tuple<>>{};
        }

        // ── DELETE (with runtime params) ──────────────────────────────────────
        template <typename Table, typename Wheres, typename... Params>
        static auto execute(MongoDB& db, delete_query<Table, Wheres> q, Params&&... /*params*/)
            -> result<std::tuple<>>
        {
            db.coll.last_filter = mongo_detail::render_wheres(q.wheres());
            return result<std::tuple<>>{};
        }
    };

} // namespace orm
