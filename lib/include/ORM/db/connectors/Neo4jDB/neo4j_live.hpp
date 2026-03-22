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
#include <neo4j-client.h>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <format>
#include <cstdint>

namespace orm {

    // ── Neo4jLiveDB ────────────────────────────────────────────────────────────
    // Live Neo4j connection using libneo4j-client (Bolt protocol).
    // Open via Neo4jLiveDB::connect(url).
    struct Neo4jLiveDB
    {
        neo4j_connection_t* conn_{nullptr};

        Neo4jLiveDB() = default;
        Neo4jLiveDB(const Neo4jLiveDB&) = delete;
        Neo4jLiveDB& operator=(const Neo4jLiveDB&) = delete;

        Neo4jLiveDB(Neo4jLiveDB&& o) noexcept : conn_(o.conn_) { o.conn_ = nullptr; }
        Neo4jLiveDB& operator=(Neo4jLiveDB&& o) noexcept
        {
            if (this != &o) { close(); conn_ = o.conn_; o.conn_ = nullptr; }
            return *this;
        }

        ~Neo4jLiveDB() { close(); }

        // url: e.g. "neo4j://localhost:7687" or "bolt://localhost:7687"
        // username/password: Neo4j authentication credentials
        [[nodiscard]] static Neo4jLiveDB connect(
            const char* url,
            const char* username = "neo4j",
            const char* password = "neo4j")
        {
            neo4j_client_init();

            neo4j_config_t* config = neo4j_new_config();
            if (!config)
                throw std::runtime_error("neo4j_new_config failed");

            neo4j_config_set_username(config, username);
            neo4j_config_set_password(config, password);

            // Disable TLS certificate verification for self-signed certs in test containers.
            neo4j_config_set_unverified_host_callback(config,
                [](void*, const char*, const char*, neo4j_unverified_host_reason_t) -> int
                {
                    return NEO4J_HOST_VERIFICATION_TRUST;
                },
                nullptr);

            Neo4jLiveDB db;
            db.conn_ = neo4j_connect(url, config, NEO4J_INSECURE);
            neo4j_config_free(config);

            if (!db.conn_)
                throw std::runtime_error(std::format("neo4j_connect failed: {}", strerror(errno)));

            return db;
        }

        void close() noexcept
        {
            if (conn_) { neo4j_close(conn_); conn_ = nullptr; }
        }

        [[nodiscard]] bool is_open() const noexcept { return conn_ != nullptr; }
    };

    // ── Cypher rendering helpers ───────────────────────────────────────────────
    namespace neo4j_live_detail {

        struct CypherCtx
        {
            int                                          next_param{1};
            std::unordered_map<std::string, std::string> params;

            [[nodiscard]] std::string add_param(std::string val)
            {
                std::string key = "p" + std::to_string(next_param++);
                params[key] = std::move(val);
                return "$" + key;
            }
        };

        template <typename Tuple>
        [[nodiscard]] std::string render_return(const Tuple& t)
        {
            std::string out;
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                std::size_t idx = 0;
                ((void)(out += (idx++ > 0 ? ", " : "")
                    + std::string("n.") + std::string(t.template get<Is>().column_name())), ...);
            }(std::make_index_sequence<Tuple::size>{});
            return out;
        }

        template <typename T>
        [[nodiscard]] std::string render_leaf(const T& /*v*/, CypherCtx& ctx)
        {
            if constexpr (is_field<T>)
                return "n." + std::string(T::column_name());
            else if constexpr (detail::is_raw_mem_ptr<T>)
                return "n." + std::string(detail::column_name_of<T>());
            else if constexpr (is_placeholder_v<T>)
                return ctx.add_param("?");
            else
                return "NULL";
        }

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
                return render_leaf(r.lhs_, ctx)
                    + " " + std::string(static_cast<std::string_view>(Op)) + " "
                    + render_leaf(r.rhs_, ctx);
            }
        }

        template <typename Wheres>
        [[nodiscard]] std::string render_where(const Wheres& w, CypherCtx& ctx)
        {
            if constexpr (Wheres::size == 0)
                return {};
            else
            {
                std::string out = " WHERE ";
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    std::size_t i = 0;
                    ((void)(out += (i++ > 0 ? " AND " : "")
                        + render_where_rule(w.template get<Is>(), ctx)), ...);
                }(std::make_index_sequence<Wheres::size>{});
                return out;
            }
        }

        template <typename T>
        [[nodiscard]] std::string stringify(const T& v)
        {
            using D = std::decay_t<T>;
            if constexpr (std::is_arithmetic_v<D>)             return std::to_string(v);
            else if constexpr (std::is_same_v<D, std::string>) return v;
            else if constexpr (std::is_same_v<D, const char*> || std::is_same_v<D, char*>)
                return std::string(v);
            else return {};
        }

        // Build a neo4j_map_entry array from the CypherCtx param map.
        // Returns the entries vector (caller must keep strings alive).
        [[nodiscard]] inline std::vector<neo4j_map_entry_t> build_map_entries(
            const std::unordered_map<std::string, std::string>& params)
        {
            std::vector<neo4j_map_entry_t> entries;
            entries.reserve(params.size());
            for (const auto& [k, v] : params)
            {
                entries.push_back(neo4j_map_entry(k.c_str(),
                    neo4j_string(v.c_str())));
            }
            return entries;
        }

        // Convert a neo4j_value_t to a C++ typed value.
        template <typename T>
        [[nodiscard]] T read_neo4j_value(neo4j_value_t val)
        {
            if constexpr (std::is_same_v<T, std::string>)
            {
                if (neo4j_type(val) == NEO4J_STRING)
                {
                    char buf[4096] = {};
                    neo4j_string_value(val, buf, sizeof(buf));
                    return std::string(buf);
                }
                // Coerce other types to string
                char buf[256] = {};
                neo4j_tostring(val, buf, sizeof(buf));
                return std::string(buf);
            }
            else if constexpr (std::is_same_v<T, std::u8string>)
            {
                char buf[4096] = {};
                neo4j_string_value(val, buf, sizeof(buf));
                return std::u8string(reinterpret_cast<const char8_t*>(buf));
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                if (neo4j_type(val) == NEO4J_BOOL)
                    return neo4j_bool_value(val) != 0;
                return false;
            }
            else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>)
            {
                if (neo4j_type(val) == NEO4J_FLOAT)
                    return static_cast<T>(neo4j_float_value(val));
                if (neo4j_type(val) == NEO4J_INT)
                    return static_cast<T>(neo4j_int_value(val));
                return T{};
            }
            else if constexpr (std::is_integral_v<T>)
            {
                if (neo4j_type(val) == NEO4J_INT)
                    return static_cast<T>(neo4j_int_value(val));
                return T{};
            }
            else
                return T{};
        }

    } // namespace neo4j_live_detail

    // ── connector_trait<Neo4jLiveDB> specialisation ───────────────────────────
    template <>
    struct connector_trait<Neo4jLiveDB>
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
            Neo4jLiveDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
            -> result<projected_type<Response>, Response>
        {
            using Entity = typename Response::template orm_type<0>::table_type;
            neo4j_live_detail::CypherCtx ctx;
            const std::string where_clause = neo4j_live_detail::render_where(q.where_clauses(), ctx);
            const std::string cypher = std::format("MATCH (n:{}) RETURN {}{}",
                table_name<Entity>(),
                neo4j_live_detail::render_return(q.selected_properties()),
                where_clause);
            return exec_select<projected_type<Response>, Response>(db, cypher, ctx.params);
        }

        // ── SELECT (with runtime params) ──────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders,
                  typename... Params>
        static auto execute(
            Neo4jLiveDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q,
            Params&&... params)
            -> result<projected_type<Response>, Response>
        {
            using Entity = typename Response::template orm_type<0>::table_type;
            neo4j_live_detail::CypherCtx ctx;
            const std::string where_clause = neo4j_live_detail::render_where(q.where_clauses(), ctx);
            const std::string cypher = std::format("MATCH (n:{}) RETURN {}{}",
                table_name<Entity>(),
                neo4j_live_detail::render_return(q.selected_properties()),
                where_clause);
            // Substitute placeholder values into param map.
            int pi = 1;
            ([&](auto&& v)
            {
                std::string key = "p" + std::to_string(pi++);
                if (ctx.params.count(key))
                    ctx.params[key] = neo4j_live_detail::stringify(v);
            }(std::forward<Params>(params)), ...);
            return exec_select<projected_type<Response>, Response>(db, cypher, ctx.params);
        }

        // ── INSERT (no runtime params) ────────────────────────────────────────
        template <typename Properties>
        static auto execute(Neo4jLiveDB& db, insert_query<Properties> /*q*/)
            -> result<std::tuple<>>
        {
            using Entity = typename Properties::template orm_type<0>::table_type;
            const std::string cypher = std::format("CREATE (n:{} {{}})",
                table_name<Entity>());
            exec_no_result(db, cypher, {});
            return result<std::tuple<>>{};
        }

        // ── INSERT (with runtime params) ──────────────────────────────────────
        template <typename Properties, typename... Params>
        static auto execute(Neo4jLiveDB& db, insert_query<Properties> q, Params&&... params)
            -> result<std::tuple<>>
        {
            using Entity = typename Properties::template orm_type<0>::table_type;

            // Build property map: {col1: $p1, col2: $p2, ...}
            std::vector<std::string> col_names;
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                ((void)(col_names.push_back(
                    std::string(q.signature().template get<Is>().column_name()))), ...);
            }(std::make_index_sequence<Properties::size>{});

            std::unordered_map<std::string, std::string> param_map;
            std::size_t pi = 1;
            ([&](auto&& v)
            {
                if (pi <= col_names.size())
                {
                    std::string key = "p" + std::to_string(pi);
                    param_map[key] = neo4j_live_detail::stringify(v);
                }
                ++pi;
            }(std::forward<Params>(params)), ...);

            // Build property string: {col1: $p1, col2: $p2}
            std::string props;
            for (std::size_t i = 0; i < col_names.size(); ++i)
            {
                if (i > 0) props += ", ";
                props += col_names[i] + ": $p" + std::to_string(i + 1);
            }

            const std::string cypher = std::format("CREATE (n:{} {{{}}})",
                table_name<Entity>(), props);
            exec_no_result(db, cypher, param_map);
            return result<std::tuple<>>{};
        }

        // ── DELETE (no runtime params) ────────────────────────────────────────
        template <typename Table, typename Wheres>
        static auto execute(Neo4jLiveDB& db, delete_query<Table, Wheres> q)
            -> result<std::tuple<>>
        {
            neo4j_live_detail::CypherCtx ctx;
            const std::string where_clause = neo4j_live_detail::render_where(q.wheres(), ctx);
            const std::string cypher = std::format("MATCH (n:{}){}DETACH DELETE n",
                table_name<Table>(), where_clause);
            exec_no_result(db, cypher, ctx.params);
            return result<std::tuple<>>{};
        }

        // ── DELETE (with runtime params) ──────────────────────────────────────
        template <typename Table, typename Wheres, typename... Params>
        static auto execute(Neo4jLiveDB& db, delete_query<Table, Wheres> q, Params&&... params)
            -> result<std::tuple<>>
        {
            neo4j_live_detail::CypherCtx ctx;
            const std::string where_clause = neo4j_live_detail::render_where(q.wheres(), ctx);
            const std::string cypher = std::format("MATCH (n:{}){}DETACH DELETE n",
                table_name<Table>(), where_clause);
            int pi = 1;
            ([&](auto&& v)
            {
                std::string key = "p" + std::to_string(pi++);
                if (ctx.params.count(key))
                    ctx.params[key] = neo4j_live_detail::stringify(v);
            }(std::forward<Params>(params)), ...);
            exec_no_result(db, cypher, ctx.params);
            return result<std::tuple<>>{};
        }

    private:
        static void exec_no_result(
            Neo4jLiveDB& db,
            const std::string& cypher,
            const std::unordered_map<std::string, std::string>& params)
        {
            auto entries = neo4j_live_detail::build_map_entries(params);
            neo4j_value_t params_val = entries.empty()
                ? neo4j_map(nullptr, 0)
                : neo4j_map(entries.data(), static_cast<unsigned int>(entries.size()));

            neo4j_result_stream_t* stream = neo4j_run(
                db.conn_, cypher.c_str(), params_val);
            if (!stream)
                throw std::runtime_error(std::format("neo4j_run failed: {}", strerror(errno)));

            if (neo4j_check_failure(stream) != 0)
            {
                const struct neo4j_failure_details* det = neo4j_failure_details(stream);
                std::string err = det ? std::string(det->message) : "unknown error";
                neo4j_close_results(stream);
                throw std::runtime_error("Neo4j query failed: " + err);
            }
            neo4j_close_results(stream);
        }

        template <typename Row, typename Response>
        static auto exec_select(
            Neo4jLiveDB& db,
            const std::string& cypher,
            const std::unordered_map<std::string, std::string>& params)
            -> result<Row, Response>
        {
            auto entries = neo4j_live_detail::build_map_entries(params);
            neo4j_value_t params_val = entries.empty()
                ? neo4j_map(nullptr, 0)
                : neo4j_map(entries.data(), static_cast<unsigned int>(entries.size()));

            neo4j_result_stream_t* stream = neo4j_run(
                db.conn_, cypher.c_str(), params_val);
            if (!stream)
                throw std::runtime_error(std::format("neo4j_run failed: {}", strerror(errno)));

            if (neo4j_check_failure(stream) != 0)
            {
                const struct neo4j_failure_details* det = neo4j_failure_details(stream);
                std::string err = det ? std::string(det->message) : "unknown error";
                neo4j_close_results(stream);
                throw std::runtime_error("Neo4j SELECT failed: " + err);
            }

            constexpr std::size_t ncols = std::tuple_size_v<Row>;
            std::vector<Row> rows;

            neo4j_result_t* neo_result = nullptr;
            while ((neo_result = neo4j_fetch_next(stream)) != nullptr)
            {
                rows.push_back(hydrate_row<Row>(neo_result,
                    std::make_index_sequence<ncols>{}));
            }

            neo4j_close_results(stream);
            return result<Row, Response>{ std::move(rows) };
        }

        template <typename Row, std::size_t... Is>
        static Row hydrate_row(neo4j_result_t* neo_result, std::index_sequence<Is...>)
        {
            return Row{ neo4j_live_detail::read_neo4j_value<std::tuple_element_t<Is, Row>>(
                neo4j_result_field(neo_result, Is))... };
        }
    };

} // namespace orm
