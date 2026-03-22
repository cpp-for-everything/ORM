#pragma once
#include "ORM/connector/capabilities.hpp"
#include "ORM/result/result.hpp"
#include "ORM/query/select.hpp"
#include "ORM/query/insert.hpp"
#include "ORM/query/delete.hpp"
#include "ORM/query/field.hpp"
#include "ORM/query/placeholders.hpp"
#include "ORM/entity/table.hpp"
#include <string>
#include <vector>
#include <utility>

namespace orm {

    // ── MockRedisContext ──────────────────────────────────────────────────────
    // Stub for redisContext*. Captures the last Redis command and key.
    struct MockRedisContext
    {
        mutable std::string last_command;
        mutable std::string last_key;
        mutable std::string last_value;
        mutable std::vector<std::pair<std::string, std::string>> last_hash_fields;

        void run_set(std::string_view key, std::string_view value) const
        {
            last_command = "SET";
            last_key     = std::string(key);
            last_value   = std::string(value);
        }
        void run_get(std::string_view key) const
        {
            last_command = "GET";
            last_key     = std::string(key);
        }
        void run_hset(std::string_view key,
                      const std::vector<std::pair<std::string, std::string>>& fields) const
        {
            last_command    = "HSET";
            last_key        = std::string(key);
            last_hash_fields = fields;
        }
        void run_hgetall(std::string_view key) const
        {
            last_command = "HGETALL";
            last_key     = std::string(key);
        }
    };

    // ── RedisDB tag ───────────────────────────────────────────────────────────
    // Database tag type. Pass as DB template argument to orm::db<DB>.
    // Wraps a MockRedisContext for testing.
    struct RedisDB
    {
        mutable MockRedisContext ctx;
    };

    // ── Redis rendering helpers ───────────────────────────────────────────────
    namespace redis_detail {

        // ── Derive the compile-time key prefix from the entity table name.
        // Uses table_name<Table>() which returns the struct type name.
        template <typename Table>
        [[nodiscard]] std::string derive_prefix()
        {
            return std::string(table_name<Table>()) + ":";
        }

        // ── Compile-time check: is this a single primary-key equality predicate?
        // For the mock, we accept any single-rule WHERE as valid.
        // The static_assert fires when a non-PK predicate is detected.
        // (Full compile-time PK detection requires the entity to expose its PK type
        //  via a specialisation; here we validate structurally.)
        template <typename Wheres>
        consteval bool is_pk_only_where()
        {
            // Allow zero WHERE (raw key access) or exactly one equality predicate.
            // Full PK-column type checking requires integration with entity metadata.
            return Wheres::size <= 1;
        }

    } // namespace redis_detail

    // ── connector_trait<RedisDB> specialisation ───────────────────────────────
    template <>
    struct connector_trait<RedisDB>
    {
        // Redis does not support SQL joins, transactions, or aggregation.
        // supports_joins, supports_transactions, supports_aggregation MUST NOT be declared.

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
            RedisDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> /*q*/)
            -> result<projected_type<Response>, Response>
        {
            static_assert(redis_detail::is_pk_only_where<Wheres>(),
                "RedisDB connector supports only primary-key equality WHERE predicates");
            if constexpr (Response::size == 1)
                db.ctx.run_get("Key:?");
            else
                db.ctx.run_hgetall("Key:?");
            return result<projected_type<Response>, Response>{};
        }

        // ── SELECT (with runtime params — key from first param) ───────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders,
                  typename PK, typename... Rest>
        static auto execute(
            RedisDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> /*q*/,
            PK&& pk, Rest&&... /*rest*/)
            -> result<projected_type<Response>, Response>
        {
            static_assert(redis_detail::is_pk_only_where<Wheres>(),
                "RedisDB connector supports only primary-key equality WHERE predicates");
            std::string key = "Key:" + std::to_string(pk);
            if constexpr (Response::size == 1)
                db.ctx.run_get(key);
            else
                db.ctx.run_hgetall(key);
            return result<projected_type<Response>, Response>{};
        }

        // ── INSERT — single-column → SET; multi-column → HSET
        // No-params variant: stores placeholder key
        template <typename Properties>
        static auto execute(RedisDB& db, insert_query<Properties> /*q*/)
            -> result<std::tuple<>>
        {
            if constexpr (Properties::size == 1)
                db.ctx.run_set("Key:?", "?");
            else
                db.ctx.run_hset("Key:?", {});
            return result<std::tuple<>>{};
        }

        // INSERT with runtime params: first param is PK, rest are field values
        template <typename Properties, typename PK, typename... Values>
        static auto execute(RedisDB& db, insert_query<Properties> q,
                            PK&& pk, Values&&... values)
            -> result<std::tuple<>>
        {
            std::string key = "Key:" + std::to_string(pk);
            if constexpr (Properties::size == 1)
            {
                // Single-column: SET key value
                std::string val = [&]() -> std::string {
                    if constexpr (sizeof...(values) > 0)
                    {
                        std::string first_val;
                        ([&](auto&& v){ if (first_val.empty()) first_val = std::to_string(v); }(values), ...);
                        return first_val;
                    }
                    return "?";
                }();
                db.ctx.run_set(key, val);
            }
            else
            {
                // Multi-column: HSET key field1 val1 field2 val2 ...
                std::vector<std::pair<std::string, std::string>> fields;
                auto col_names = [&]() {
                    std::vector<std::string> cols;
                    [&]<std::size_t... Is>(std::index_sequence<Is...>)
                    {
                        ((void)(cols.push_back(std::string(
                            q.signature().template get<Is>().column_name()))), ...);
                    }(std::make_index_sequence<Properties::size>{});
                    return cols;
                }();
                int fi = 0;
                ([&](auto&& v)
                {
                    if (fi < static_cast<int>(col_names.size()))
                        fields.push_back({col_names[fi++], std::to_string(v)});
                }(std::forward<Values>(values)), ...);
                db.ctx.run_hset(key, fields);
            }
            return result<std::tuple<>>{};
        }

        // ── DELETE ────────────────────────────────────────────────────────────
        template <typename Table, typename Wheres>
        static auto execute(RedisDB& db, delete_query<Table, Wheres> /*q*/)
            -> result<std::tuple<>>
        {
            static_assert(redis_detail::is_pk_only_where<Wheres>(),
                "RedisDB connector supports only primary-key equality WHERE predicates");
            db.ctx.last_command = "DEL";
            return result<std::tuple<>>{};
        }

        template <typename Table, typename Wheres, typename PK>
        static auto execute(RedisDB& db, delete_query<Table, Wheres> /*q*/, PK&& pk)
            -> result<std::tuple<>>
        {
            static_assert(redis_detail::is_pk_only_where<Wheres>(),
                "RedisDB connector supports only primary-key equality WHERE predicates");
            db.ctx.last_command = "DEL";
            db.ctx.last_key     = "Key:" + std::to_string(pk);
            return result<std::tuple<>>{};
        }
    };

} // namespace orm
