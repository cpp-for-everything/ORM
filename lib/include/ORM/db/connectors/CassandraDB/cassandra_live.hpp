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
#include <cassandra.h>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <format>
#include <cstdint>

namespace orm {

    // ── CassandraLiveDB ────────────────────────────────────────────────────────
    // Live Cassandra connection. Owns a CassSession* and CassCluster*.
    // Open via CassandraLiveDB::connect(contact_points, keyspace).
    struct CassandraLiveDB
    {
        CassCluster* cluster_{nullptr};
        CassSession* session_{nullptr};

        CassandraLiveDB() = default;
        CassandraLiveDB(const CassandraLiveDB&) = delete;
        CassandraLiveDB& operator=(const CassandraLiveDB&) = delete;

        CassandraLiveDB(CassandraLiveDB&& o) noexcept
            : cluster_(o.cluster_), session_(o.session_)
        {
            o.cluster_ = nullptr;
            o.session_ = nullptr;
        }

        CassandraLiveDB& operator=(CassandraLiveDB&& o) noexcept
        {
            if (this != &o)
            {
                close();
                cluster_ = o.cluster_;
                session_ = o.session_;
                o.cluster_ = nullptr;
                o.session_ = nullptr;
            }
            return *this;
        }

        ~CassandraLiveDB() { close(); }

        // contact_points: comma-separated host list, e.g. "127.0.0.1"
        // keyspace: Cassandra keyspace name
        [[nodiscard]] static CassandraLiveDB connect(
            const char* contact_points,
            const char* keyspace,
            unsigned int port = 9042)
        {
            CassandraLiveDB db;
            db.cluster_ = cass_cluster_new();
            db.session_ = cass_session_new();

            cass_cluster_set_contact_points(db.cluster_, contact_points);
            cass_cluster_set_port(db.cluster_, static_cast<int>(port));

            CassFuture* connect_future = cass_session_connect_keyspace(
                db.session_, db.cluster_, keyspace);
            cass_future_wait(connect_future);

            if (cass_future_error_code(connect_future) != CASS_OK)
            {
                const char* msg = nullptr;
                std::size_t msg_len = 0;
                cass_future_error_message(connect_future, &msg, &msg_len);
                std::string err(msg, msg_len);
                cass_future_free(connect_future);
                db.close();
                throw std::runtime_error("Cassandra connect failed: " + err);
            }

            cass_future_free(connect_future);
            return db;
        }

        void close() noexcept
        {
            if (session_)
            {
                CassFuture* f = cass_session_close(session_);
                cass_future_wait(f);
                cass_future_free(f);
                cass_session_free(session_);
                session_ = nullptr;
            }
            if (cluster_) { cass_cluster_free(cluster_); cluster_ = nullptr; }
        }

        [[nodiscard]] bool is_open() const noexcept
        {
            return session_ != nullptr && cluster_ != nullptr;
        }
    };

    // ── CQL rendering helpers ──────────────────────────────────────────────────
    namespace cass_live_detail {

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

        // Translate ORM operators to CQL operators
        [[nodiscard]] inline std::string translate_operator(std::string_view op)
        {
            if (op == "==") return "=";
            if (op == "!=") return "!=";
            return std::string(op);
        }

        template <typename T1, detail::string_literal Op, typename T2>
        [[nodiscard]] std::string render_rule(const Rule<T1, Op, T2>& r)
        {
            std::string cql_op = translate_operator(static_cast<std::string_view>(Op));
            
            if constexpr (is_rule<T1> && is_rule<T2>)
            {
                return render_rule(r.lhs_)
                    + " " + cql_op + " "
                    + render_rule(r.rhs_);
            }
            else
            {
                return render_operand(r.lhs_)
                    + " " + cql_op + " "
                    + render_operand(r.rhs_);
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
        [[nodiscard]] std::string render_wheres(const Wheres& w)
        {
            if constexpr (Wheres::size == 0)
                return {};
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

        template <typename Wheres>
        consteval bool has_partition_key_predicate() { return Wheres::size >= 1; }

        // Bind a C++ value to a CassStatement at the given index.
        template <typename T>
        void bind_value(CassStatement* stmt, std::size_t idx, const T& v)
        {
            using D = std::decay_t<T>;
            if constexpr (std::is_same_v<D, bool>)
                cass_statement_bind_bool(stmt, idx, v ? cass_true : cass_false);
            else if constexpr (std::is_same_v<D, std::int8_t>)
                cass_statement_bind_int8(stmt, idx, v);
            else if constexpr (std::is_same_v<D, std::int16_t>)
                cass_statement_bind_int16(stmt, idx, v);
            else if constexpr (std::is_same_v<D, std::int32_t> || std::is_same_v<D, int>)
                cass_statement_bind_int32(stmt, idx, static_cast<cass_int32_t>(v));
            else if constexpr (std::is_same_v<D, std::int64_t> || std::is_same_v<D, long long>)
                cass_statement_bind_int64(stmt, idx, static_cast<cass_int64_t>(v));
            else if constexpr (std::is_same_v<D, float>)
                cass_statement_bind_float(stmt, idx, v);
            else if constexpr (std::is_same_v<D, double>)
                cass_statement_bind_double(stmt, idx, v);
            else if constexpr (std::is_same_v<D, std::string>)
                cass_statement_bind_string_n(stmt, idx, v.data(), v.size());
            else if constexpr (std::is_same_v<D, std::u8string>)
                cass_statement_bind_string_n(stmt, idx,
                    reinterpret_cast<const char*>(v.data()), v.size());
            else if constexpr (std::is_same_v<D, const char*> || std::is_same_v<D, char*>)
                cass_statement_bind_string(stmt, idx, v);
            else if constexpr (std::is_same_v<D, std::nullptr_t>)
                cass_statement_bind_null(stmt, idx);
        }

        template <typename... Params>
        void bind_params(CassStatement* stmt, Params&&... params)
        {
            std::size_t idx = 0;
            (bind_value(stmt, idx++, std::forward<Params>(params)), ...);
        }

        // Execute a CQL statement (no result rows).
        inline void exec_no_result(CassSession* session, const std::string& cql,
                                   CassStatement* stmt)
        {
            CassFuture* future = cass_session_execute(session, stmt);
            cass_future_wait(future);
            if (cass_future_error_code(future) != CASS_OK)
            {
                const char* msg = nullptr;
                std::size_t msg_len = 0;
                cass_future_error_message(future, &msg, &msg_len);
                std::string err(msg, msg_len);
                cass_future_free(future);
                throw std::runtime_error(std::format("Cassandra exec failed ({}): {}",
                    cql, err));
            }
            cass_future_free(future);
        }

        // Convert CassValue to typed C++ value.
        template <typename T>
        [[nodiscard]] T read_cass_value(const CassValue* val)
        {
            if (!val || cass_value_is_null(val)) return T{};
            if constexpr (std::is_same_v<T, bool>)
            {
                cass_bool_t out = cass_false;
                cass_value_get_bool(val, &out);
                return out == cass_true;
            }
            else if constexpr (std::is_same_v<T, std::int32_t> || std::is_same_v<T, int>)
            {
                cass_int32_t out = 0;
                cass_value_get_int32(val, &out);
                return static_cast<T>(out);
            }
            else if constexpr (std::is_same_v<T, std::int64_t> || std::is_same_v<T, long long>)
            {
                cass_int64_t out = 0;
                cass_value_get_int64(val, &out);
                return static_cast<T>(out);
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                cass_float_t out = 0.f;
                cass_value_get_float(val, &out);
                return static_cast<T>(out);
            }
            else if constexpr (std::is_same_v<T, double>)
            {
                cass_double_t out = 0.0;
                cass_value_get_double(val, &out);
                return static_cast<T>(out);
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                const char* s = nullptr;
                std::size_t len = 0;
                cass_value_get_string(val, &s, &len);
                return std::string(s, len);
            }
            else if constexpr (std::is_same_v<T, std::u8string>)
            {
                const char* s = nullptr;
                std::size_t len = 0;
                cass_value_get_string(val, &s, &len);
                return std::u8string(reinterpret_cast<const char8_t*>(s), len);
            }
            else
                return T{};
        }

    } // namespace cass_live_detail

    // ── connector_trait<CassandraLiveDB> specialisation ───────────────────────
    template <>
    struct connector_trait<CassandraLiveDB>
    {
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
            CassandraLiveDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
            -> result<projected_type<Response>, Response>
        {
            static_assert(cass_live_detail::has_partition_key_predicate<Wheres>(),
                "CassandraLiveDB: WHERE must include an equality predicate on the partition key");
            using Entity = typename Response::template orm_type<0>::table_type;
            const std::string cql = std::format("SELECT {} FROM {}{}",
                cass_live_detail::render_columns(q.selected_properties()),
                table_name<Entity>(),
                cass_live_detail::render_wheres(q.where_clauses()));
            return exec_select<projected_type<Response>, Response>(db, cql, q);
        }

        // ── SELECT (with runtime params) ──────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders,
                  typename... Params>
        static auto execute(
            CassandraLiveDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q,
            Params&&... params)
            -> result<projected_type<Response>, Response>
        {
            static_assert(cass_live_detail::has_partition_key_predicate<Wheres>(),
                "CassandraLiveDB: WHERE must include an equality predicate on the partition key");
            using Entity = typename Response::template orm_type<0>::table_type;
            const std::string cql = std::format("SELECT {} FROM {}{}",
                cass_live_detail::render_columns(q.selected_properties()),
                table_name<Entity>(),
                cass_live_detail::render_wheres(q.where_clauses()));
            return exec_select_params<projected_type<Response>, Response>(
                db, cql, q, std::forward<Params>(params)...);
        }

        // ── INSERT (no runtime params) ────────────────────────────────────────
        template <typename Properties>
        static auto execute(CassandraLiveDB& db, insert_query<Properties> q)
            -> result<std::tuple<>>
        {
            using Entity = typename Properties::template orm_type<0>::table_type;
            const std::string cql = std::format("INSERT INTO {} ({}) VALUES ({})",
                table_name<Entity>(),
                cass_live_detail::render_columns(q.signature()),
                cass_live_detail::positional_placeholders(Properties::size));
            CassStatement* stmt = cass_statement_new(cql.c_str(), 0);
            cass_live_detail::exec_no_result(db.session_, cql, stmt);
            cass_statement_free(stmt);
            return result<std::tuple<>>{};
        }

        // ── INSERT (with runtime params) ──────────────────────────────────────
        template <typename Properties, typename... Params>
        static auto execute(CassandraLiveDB& db, insert_query<Properties> q, Params&&... params)
            -> result<std::tuple<>>
        {
            using Entity = typename Properties::template orm_type<0>::table_type;
            const std::string cql = std::format("INSERT INTO {} ({}) VALUES ({})",
                table_name<Entity>(),
                cass_live_detail::render_columns(q.signature()),
                cass_live_detail::positional_placeholders(Properties::size));
            CassStatement* stmt = cass_statement_new(cql.c_str(),
                static_cast<std::size_t>(sizeof...(params)));
            cass_live_detail::bind_params(stmt, std::forward<Params>(params)...);
            cass_live_detail::exec_no_result(db.session_, cql, stmt);
            cass_statement_free(stmt);
            return result<std::tuple<>>{};
        }

        // ── DELETE (with runtime params) ──────────────────────────────────────
        template <typename Table, typename Wheres, typename... Params>
        static auto execute(CassandraLiveDB& db, delete_query<Table, Wheres> q, Params&&... params)
            -> result<std::tuple<>>
        {
            static_assert(cass_live_detail::has_partition_key_predicate<Wheres>(),
                "CassandraLiveDB: DELETE WHERE must include the partition key predicate");
            const std::string cql = std::format("DELETE FROM {}{}",
                table_name<Table>(),
                cass_live_detail::render_wheres(q.wheres()));
            CassStatement* stmt = cass_statement_new(cql.c_str(),
                static_cast<std::size_t>(sizeof...(params)));
            cass_live_detail::bind_params(stmt, std::forward<Params>(params)...);
            cass_live_detail::exec_no_result(db.session_, cql, stmt);
            cass_statement_free(stmt);
            return result<std::tuple<>>{};
        }

    private:
        template <typename Row, typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders>
        static auto exec_select(
            CassandraLiveDB& db,
            const std::string& cql,
            const select_query<Response, Joins, Wheres, Limits, Groups, Orders>& /*q*/)
            -> result<Row, Response>
        {
            CassStatement* stmt = cass_statement_new(cql.c_str(), 0);
            CassFuture* future  = cass_session_execute(db.session_, stmt);
            cass_statement_free(stmt);
            cass_future_wait(future);

            if (cass_future_error_code(future) != CASS_OK)
            {
                const char* msg = nullptr;
                std::size_t msg_len = 0;
                cass_future_error_message(future, &msg, &msg_len);
                std::string err(msg, msg_len);
                cass_future_free(future);
                throw std::runtime_error("Cassandra SELECT failed: " + err);
            }

            const CassResult* cass_res = cass_future_get_result(future);
            cass_future_free(future);

            std::vector<Row> rows;
            CassIterator* it = cass_iterator_from_result(cass_res);
            while (cass_iterator_next(it))
            {
                const CassRow* row = cass_iterator_get_row(it);
                rows.push_back(hydrate_row<Row>(row,
                    std::make_index_sequence<std::tuple_size_v<Row>>{}));
            }
            cass_iterator_free(it);
            cass_result_free(cass_res);
            return result<Row, Response>{ std::move(rows) };
        }

        template <typename Row, typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders,
                  typename... Params>
        static auto exec_select_params(
            CassandraLiveDB& db,
            const std::string& cql,
            const select_query<Response, Joins, Wheres, Limits, Groups, Orders>& /*q*/,
            Params&&... params)
            -> result<Row, Response>
        {
            CassStatement* stmt = cass_statement_new(cql.c_str(),
                static_cast<std::size_t>(sizeof...(params)));
            cass_live_detail::bind_params(stmt, std::forward<Params>(params)...);
            CassFuture* future = cass_session_execute(db.session_, stmt);
            cass_statement_free(stmt);
            cass_future_wait(future);

            if (cass_future_error_code(future) != CASS_OK)
            {
                const char* msg = nullptr;
                std::size_t msg_len = 0;
                cass_future_error_message(future, &msg, &msg_len);
                std::string err(msg, msg_len);
                cass_future_free(future);
                throw std::runtime_error("Cassandra SELECT failed: " + err);
            }

            const CassResult* cass_res = cass_future_get_result(future);
            cass_future_free(future);

            std::vector<Row> rows;
            CassIterator* it = cass_iterator_from_result(cass_res);
            while (cass_iterator_next(it))
            {
                const CassRow* row = cass_iterator_get_row(it);
                rows.push_back(hydrate_row<Row>(row,
                    std::make_index_sequence<std::tuple_size_v<Row>>{}));
            }
            cass_iterator_free(it);
            cass_result_free(cass_res);
            return result<Row, Response>{ std::move(rows) };
        }

        template <typename Row, std::size_t... Is>
        static Row hydrate_row(const CassRow* row, std::index_sequence<Is...>)
        {
            return Row{ cass_live_detail::read_cass_value<std::tuple_element_t<Is, Row>>(
                cass_row_get_column(row, Is))... };
        }
    };

} // namespace orm
