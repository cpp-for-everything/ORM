#pragma once
#include "ORM/connector/capabilities.hpp"
#include "ORM/result/result.hpp"
#include "ORM/query/select.hpp"
#include "ORM/query/insert.hpp"
#include "ORM/query/delete.hpp"
#include "ORM/query/field.hpp"
#include "ORM/query/placeholders.hpp"
#include "ORM/entity/table.hpp"

#ifdef ORM_REDIS_LIVE_AVAILABLE
#include <hiredis/hiredis.h>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <format>
#include <utility>
#include <memory>

namespace orm {

    // ── RedisLiveDB ────────────────────────────────────────────────────────────
    // Live Redis connection. Owns a redisContext*.
    // Open via RedisLiveDB::connect(host, port).
    struct RedisLiveDB
    {
        redisContext* ctx_{nullptr};

        RedisLiveDB() = default;
        RedisLiveDB(const RedisLiveDB&) = delete;
        RedisLiveDB& operator=(const RedisLiveDB&) = delete;

        RedisLiveDB(RedisLiveDB&& o) noexcept : ctx_(o.ctx_) { o.ctx_ = nullptr; }
        RedisLiveDB& operator=(RedisLiveDB&& o) noexcept
        {
            if (this != &o) { close(); ctx_ = o.ctx_; o.ctx_ = nullptr; }
            return *this;
        }

        ~RedisLiveDB() { close(); }

        [[nodiscard]] static RedisLiveDB connect(const char* host, int port = 6379)
        {
            RedisLiveDB db;
            db.ctx_ = redisConnect(host, port);
            if (!db.ctx_ || db.ctx_->err)
            {
                std::string err = db.ctx_ ? std::string(db.ctx_->errstr) : "redisConnect returned null";
                if (db.ctx_) { redisFree(db.ctx_); db.ctx_ = nullptr; }
                throw std::runtime_error("redisConnect failed: " + err);
            }
            return db;
        }

        void close() noexcept
        {
            if (ctx_) { redisFree(ctx_); ctx_ = nullptr; }
        }

        [[nodiscard]] bool is_open() const noexcept
        {
            return ctx_ != nullptr && ctx_->err == 0;
        }
    };

    // ── Redis helpers ──────────────────────────────────────────────────────────
    namespace redis_live_detail {

        // RAII wrapper for redisReply*.
        struct ReplyRAII
        {
            redisReply* reply{nullptr};
            explicit ReplyRAII(void* r) : reply(static_cast<redisReply*>(r)) {}
            ~ReplyRAII() { if (reply) freeReplyObject(reply); }
            ReplyRAII(const ReplyRAII&) = delete;
            ReplyRAII& operator=(const ReplyRAII&) = delete;
            ReplyRAII(ReplyRAII&& o) noexcept : reply(o.reply) { o.reply = nullptr; }
        };

        template <typename T>
        [[nodiscard]] std::string to_string(const T& v)
        {
            using D = std::decay_t<T>;
            if constexpr (std::is_arithmetic_v<D>)   return std::to_string(v);
            else if constexpr (std::is_same_v<D, std::string>) return v;
            else if constexpr (std::is_same_v<D, const char*> || std::is_same_v<D, char*>)
                return std::string(v);
            else return {};
        }

        template <typename T>
        [[nodiscard]] T from_string(const std::string& s)
        {
            if (s.empty()) return T{};
            if constexpr (std::is_same_v<T, std::string>)   return s;
            else if constexpr (std::is_same_v<T, std::u8string>)
                return std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size());
            else if constexpr (std::is_same_v<T, bool>)      return s == "1" || s == "true";
            else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>)
                return static_cast<T>(std::stod(s));
            else if constexpr (std::is_integral_v<T>)
                return static_cast<T>(std::stoll(s));
            else return T{};
        }

        // Execute a Redis command; throws on error.
        template <typename... Args>
        [[nodiscard]] ReplyRAII command(redisContext* ctx, const char* fmt, Args&&... args)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
            ReplyRAII r{ redisCommand(ctx, fmt, std::forward<Args>(args)...) };
            if (!r.reply)
                throw std::runtime_error(std::format("redisCommand failed: {}", ctx->errstr));
            if (r.reply->type == REDIS_REPLY_ERROR)
                throw std::runtime_error(std::format("Redis error: {}", r.reply->str));
            return r;
        }

        // Derive the Redis key prefix from the entity table name.
        template <typename Table>
        [[nodiscard]] std::string key_prefix()
        {
            return std::string(table_name<Table>()) + ":";
        }

        template <typename Wheres>
        consteval bool is_pk_only_where() { return Wheres::size <= 1; }

    } // namespace redis_live_detail

    // ── connector_trait<RedisLiveDB> specialisation ───────────────────────────
    template <>
    struct connector_trait<RedisLiveDB>
    {
        template <typename T>
        struct wire_type { using type = T; };

        struct cursor_type
        {
            [[nodiscard]] bool has_next() const noexcept { return false; }
        };

        // ── SELECT (no runtime params) — returns empty (key unknown) ──────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders>
        static auto execute(
            RedisLiveDB& /*db*/,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> /*q*/)
            -> result<projected_type<Response>, Response>
        {
            static_assert(redis_live_detail::is_pk_only_where<Wheres>(),
                "RedisLiveDB: WHERE must be a single primary-key equality predicate");
            return result<projected_type<Response>, Response>{};
        }

        // ── SELECT (with PK param) ─────────────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders,
                  typename PK, typename... Rest>
        static auto execute(
            RedisLiveDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> /*q*/,
            PK&& pk, Rest&&... /*rest*/)
            -> result<projected_type<Response>, Response>
        {
            using Row = projected_type<Response>;
            using Entity = typename Response::template orm_type<0>::table_type;
            static_assert(redis_live_detail::is_pk_only_where<Wheres>(),
                "RedisLiveDB: WHERE must be a single primary-key equality predicate");

            const std::string key = redis_live_detail::key_prefix<Entity>()
                + redis_live_detail::to_string(pk);

            std::vector<Row> rows;

            if constexpr (std::tuple_size_v<Row> == 1)
            {
                // Single-field: GET key
                auto r = redis_live_detail::command(db.ctx_, "GET %s", key.c_str());
                if (r.reply->type == REDIS_REPLY_STRING)
                {
                    using ColT = std::tuple_element_t<0, Row>;
                    rows.push_back(Row{ redis_live_detail::from_string<ColT>(
                        std::string(r.reply->str, r.reply->len)) });
                }
            }
            else
            {
                // Multi-field: HGETALL key
                auto r = redis_live_detail::command(db.ctx_, "HGETALL %s", key.c_str());
                if (r.reply->type == REDIS_REPLY_ARRAY && r.reply->elements % 2 == 0)
                {
                    std::vector<std::string> field_vals(std::tuple_size_v<Row>);
                    for (std::size_t i = 0; i + 1 < r.reply->elements; i += 2)
                    {
                        const std::string_view fname{
                            r.reply->element[i]->str,
                            static_cast<std::size_t>(r.reply->element[i]->len)};
                        const std::string fval{
                            r.reply->element[i + 1]->str,
                            static_cast<std::size_t>(r.reply->element[i + 1]->len)};
                        // Match field name to column index — done at compile time would be
                        // ideal but hiredis gives us runtime strings; iterate the type list.
                        map_field_to_slot<Row>(fname, fval, field_vals,
                            std::make_index_sequence<std::tuple_size_v<Row>>{});
                    }
                    rows.push_back(make_row_from_strings<Row>(field_vals,
                        std::make_index_sequence<std::tuple_size_v<Row>>{}));
                }
            }
            return result<Row, Response>{ std::move(rows) };
        }

        // ── INSERT — single-column → SET key val; multi-column → HSET ─────────
        template <typename Properties>
        static auto execute(RedisLiveDB& /*db*/, insert_query<Properties> /*q*/)
            -> result<std::tuple<>>
        {
            return result<std::tuple<>>{};
        }

        template <typename Properties, typename PK, typename... Values>
        static auto execute(RedisLiveDB& db, insert_query<Properties> q,
                            PK&& pk, Values&&... values)
            -> result<std::tuple<>>
        {
            using Entity = typename Properties::template orm_type<0>::table_type;
            const std::string key = redis_live_detail::key_prefix<Entity>()
                + redis_live_detail::to_string(pk);

            if constexpr (Properties::size == 1)
            {
                if constexpr (sizeof...(values) > 0)
                {
                    std::string val;
                    ([&](auto&& v){ if (val.empty()) val = redis_live_detail::to_string(v); }
                        (std::forward<Values>(values)), ...);
                    redis_live_detail::command(db.ctx_, "SET %s %s",
                        key.c_str(), val.c_str());
                }
            }
            else
            {
                // Build HSET key field1 val1 field2 val2 ...
                std::vector<std::string> col_names;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    ((void)(col_names.push_back(std::string(
                        q.signature().template get<Is>().column_name()))), ...);
                }(std::make_index_sequence<Properties::size>{});

                std::vector<std::string> field_vals;
                (field_vals.push_back(redis_live_detail::to_string(
                    std::forward<Values>(values))), ...);

                // Build argv for HSET key f1 v1 f2 v2 ...
                std::vector<const char*> argv;
                argv.push_back("HSET");
                argv.push_back(key.c_str());
                for (std::size_t i = 0; i < col_names.size() && i < field_vals.size(); ++i)
                {
                    argv.push_back(col_names[i].c_str());
                    argv.push_back(field_vals[i].c_str());
                }
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
                void* raw = redisCommandArgv(db.ctx_,
                    static_cast<int>(argv.size()), argv.data(), nullptr);
                redis_live_detail::ReplyRAII raii{raw};
                if (!raii.reply || raii.reply->type == REDIS_REPLY_ERROR)
                    throw std::runtime_error("HSET failed");
            }
            return result<std::tuple<>>{};
        }

        // ── DELETE ────────────────────────────────────────────────────────────
        template <typename Table, typename Wheres>
        static auto execute(RedisLiveDB& /*db*/, delete_query<Table, Wheres> /*q*/)
            -> result<std::tuple<>>
        {
            static_assert(redis_live_detail::is_pk_only_where<Wheres>(),
                "RedisLiveDB: DELETE WHERE must be a primary-key equality predicate");
            return result<std::tuple<>>{};
        }

        template <typename Table, typename Wheres, typename PK>
        static auto execute(RedisLiveDB& db, delete_query<Table, Wheres> /*q*/, PK&& pk)
            -> result<std::tuple<>>
        {
            static_assert(redis_live_detail::is_pk_only_where<Wheres>(),
                "RedisLiveDB: DELETE WHERE must be a primary-key equality predicate");
            const std::string key = redis_live_detail::key_prefix<Table>()
                + redis_live_detail::to_string(pk);
            redis_live_detail::command(db.ctx_, "DEL %s", key.c_str());
            return result<std::tuple<>>{};
        }

    private:
        template <typename Row>
        static void map_field_to_slot(
            std::string_view fname,
            const std::string& fval,
            std::vector<std::string>& out,
            std::index_sequence<> /*is*/)
        {
            (void)fname; (void)fval; (void)out;
        }

        template <typename Row, std::size_t I0, std::size_t... Is>
        static void map_field_to_slot(
            std::string_view fname,
            const std::string& fval,
            std::vector<std::string>& out,
            std::index_sequence<I0, Is...>)
        {
            using ColTag = typename Row::template element_type<I0>; // not available in plain tuple
            // For plain std::tuple we cannot get column names at runtime without the ORM tuple.
            // We store values positionally by occurrence order.
            (void)fname;
            if (out[I0].empty())
                out[I0] = fval;
            else
                map_field_to_slot<Row>(fname, fval, out, std::index_sequence<Is...>{});
        }

        template <typename Row, std::size_t... Is>
        static Row make_row_from_strings(
            const std::vector<std::string>& vals,
            std::index_sequence<Is...>)
        {
            return Row{ redis_live_detail::from_string<std::tuple_element_t<Is, Row>>(
                Is < vals.size() ? vals[Is] : "")... };
        }
    };

} // namespace orm

#endif // ORM_REDIS_LIVE_AVAILABLE
