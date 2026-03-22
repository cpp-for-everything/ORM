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

namespace orm {

    // ── MockCassResultRAII ────────────────────────────────────────────────────
    // RAII stub for CassResult*. Tracks cass_result_free call count.
    struct MockCassResultRAII
    {
        mutable int* result_free_count = nullptr;
        explicit MockCassResultRAII(int* count) : result_free_count(count) {}
        ~MockCassResultRAII() { if (result_free_count) ++(*result_free_count); }

        MockCassResultRAII(const MockCassResultRAII&)            = delete;
        MockCassResultRAII& operator=(const MockCassResultRAII&) = delete;
        MockCassResultRAII(MockCassResultRAII&& o) noexcept
            : result_free_count(o.result_free_count) { o.result_free_count = nullptr; }
    };

    // ── MockCassSession ───────────────────────────────────────────────────────
    // Stub for DataStax C++ Driver session handle.
    struct MockCassSession
    {
        mutable std::string              last_cql;
        mutable std::vector<std::string> last_params;
        mutable int                      result_free_count = 0;

        void prepare(std::string_view cql) const { last_cql = std::string(cql); }
        void bind(std::size_t /*idx*/, std::string_view val) const
        {
            last_params.push_back(std::string(val));
        }
        void execute_stmt() const {}
        [[nodiscard]] MockCassResultRAII get_result() const
        {
            return MockCassResultRAII{&result_free_count};
        }
    };

    // ── CassandraDB tag ───────────────────────────────────────────────────────
    // Database tag type. Pass as DB template argument to orm::db<DB>.
    struct CassandraDB
    {
        mutable MockCassSession session;
    };

    // ── CQL rendering helpers ─────────────────────────────────────────────────
    namespace cass_detail {

        // ── Compile-time check: WHERE clause has at least one predicate
        //    (structural proxy for partition key validation).
        //    Full PK column type checking requires entity PK metadata integration.
        template <typename Wheres>
        consteval bool has_partition_key_predicate()
        {
            return Wheres::size >= 1;
        }

        // ── Render operand: field → column name; placeholder → "?"
        template <typename T>
        [[nodiscard]] std::string render_operand(const T& /*v*/)
        {
            if constexpr (is_field<T>)
                return std::string(T::column_name());
            else if constexpr (detail::is_raw_mem_ptr<T>)
                return std::string(detail::column_name_of<T>());
            else
                return "?";
        }

        [[nodiscard]] inline std::string render_operand(std::nullptr_t) { return "NULL"; }

        // ── Render rule tree
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

        // ── Render columns
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

        // ── Render WHERE
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

        // ── Render N positional ? placeholders
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

        // ── stringify param
        template <typename T>
        [[nodiscard]] std::string stringify(const T& v)
        {
            using D = std::decay_t<T>;
            if constexpr (std::is_arithmetic_v<D>)   return std::to_string(v);
            else if constexpr (std::is_same_v<D, std::string>) return v;
            else return "?";
        }

        template <typename... Params>
        std::vector<std::string> collect(Params&&... params)
        {
            std::vector<std::string> out;
            out.reserve(sizeof...(params));
            (out.push_back(stringify(params)), ...);
            return out;
        }

    } // namespace cass_detail

    // ── connector_trait<CassandraDB> specialisation ───────────────────────────
    template <>
    struct connector_trait<CassandraDB>
    {
        // supports_joins MUST NOT be declared (no cross-partition joins in CQL).
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
            CassandraDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
            -> result<projected_type<Response>, Response>
        {
            static_assert(cass_detail::has_partition_key_predicate<Wheres>(),
                "CassandraDB: WHERE clause must include an equality predicate on the partition key column");
            std::string cql = std::format("SELECT {} FROM ?{}",
                cass_detail::render_columns(q.selected_properties()),
                cass_detail::render_wheres(q.where_clauses()));
            db.session.prepare(cql);
            db.session.execute_stmt();
            {
                auto r = db.session.get_result();
                (void)r;
            }
            return result<projected_type<Response>, Response>{};
        }

        // ── SELECT (with runtime params) ──────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders,
                  typename... Params>
        static auto execute(
            CassandraDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q,
            Params&&... params)
            -> result<projected_type<Response>, Response>
        {
            static_assert(cass_detail::has_partition_key_predicate<Wheres>(),
                "CassandraDB: WHERE clause must include an equality predicate on the partition key column");
            std::string cql = std::format("SELECT {} FROM ?{}",
                cass_detail::render_columns(q.selected_properties()),
                cass_detail::render_wheres(q.where_clauses()));
            db.session.prepare(cql);
            auto vals = cass_detail::collect(std::forward<Params>(params)...);
            for (std::size_t i = 0; i < vals.size(); ++i)
                db.session.bind(i, vals[i]);
            db.session.execute_stmt();
            {
                auto r = db.session.get_result();
                (void)r;
            }
            return result<projected_type<Response>, Response>{};
        }

        // ── INSERT (no runtime params) ────────────────────────────────────────
        template <typename Properties>
        static auto execute(CassandraDB& db, insert_query<Properties> q)
            -> result<std::tuple<>>
        {
            std::string cql = std::format("INSERT INTO ? ({}) VALUES ({})",
                cass_detail::render_columns(q.signature()),
                cass_detail::positional_placeholders(Properties::size));
            db.session.prepare(cql);
            db.session.execute_stmt();
            {
                auto r = db.session.get_result();
                (void)r;
            }
            return result<std::tuple<>>{};
        }

        // ── INSERT (with runtime params) ──────────────────────────────────────
        template <typename Properties, typename... Params>
        static auto execute(CassandraDB& db, insert_query<Properties> q, Params&&... params)
            -> result<std::tuple<>>
        {
            std::string cql = std::format("INSERT INTO ? ({}) VALUES ({})",
                cass_detail::render_columns(q.signature()),
                cass_detail::positional_placeholders(Properties::size));
            db.session.prepare(cql);
            auto vals = cass_detail::collect(std::forward<Params>(params)...);
            for (std::size_t i = 0; i < vals.size(); ++i)
                db.session.bind(i, vals[i]);
            db.session.execute_stmt();
            {
                auto r = db.session.get_result();
                (void)r;
            }
            return result<std::tuple<>>{};
        }

        // ── DELETE (with runtime params) ──────────────────────────────────────
        template <typename Table, typename Wheres, typename... Params>
        static auto execute(CassandraDB& db, delete_query<Table, Wheres> q, Params&&... params)
            -> result<std::tuple<>>
        {
            static_assert(cass_detail::has_partition_key_predicate<Wheres>(),
                "CassandraDB: WHERE clause must include an equality predicate on the partition key column");
            std::string cql = std::format("DELETE FROM {}{}",
                table_name<Table>(),
                cass_detail::render_wheres(q.wheres()));
            db.session.prepare(cql);
            auto vals = cass_detail::collect(std::forward<Params>(params)...);
            for (std::size_t i = 0; i < vals.size(); ++i)
                db.session.bind(i, vals[i]);
            db.session.execute_stmt();
            {
                auto r = db.session.get_result();
                (void)r;
            }
            return result<std::tuple<>>{};
        }
    };

} // namespace orm
