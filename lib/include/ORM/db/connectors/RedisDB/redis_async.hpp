#pragma once

#include "ORM/db/connectors/RedisDB/redis_live.hpp"
#include "ORM/async/io_context.hpp"
#include "ORM/async/task.hpp"
#include "ORM/connector/capabilities.hpp"
#include "ORM/result/result.hpp"

#ifdef ORM_REDIS_LIVE_AVAILABLE

#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <format>
#include <coroutine>

namespace orm {

    // ── Redis event loop adapter ───────────────────────────────────────────────
    // Bridges hiredis' async event callbacks to our IoContext reactor.
    namespace redis_adapter {

        struct AdapterState
        {
            redisAsyncContext* ac;
            IoContext* ctx;
        };

        inline void add_read(void* privdata)
        {
            auto* state = static_cast<AdapterState*>(privdata);
            state->ctx->watch_readable(state->ac->c.fd, [ac = state->ac]() {
                redisAsyncHandleRead(ac);
            });
        }

        inline void del_read(void* /*privdata*/) {}

        inline void add_write(void* privdata)
        {
            auto* state = static_cast<AdapterState*>(privdata);
            state->ctx->watch_writable(state->ac->c.fd, [ac = state->ac]() {
                redisAsyncHandleWrite(ac);
            });
        }

        inline void del_write(void* /*privdata*/) {}

        inline void cleanup(void* privdata)
        {
            delete static_cast<AdapterState*>(privdata);
        }

        inline void attach(redisAsyncContext* ac, IoContext& ctx)
        {
            auto* state = new AdapterState{ac, &ctx};
            ac->ev.addRead = add_read;
            ac->ev.delRead = del_read;
            ac->ev.addWrite = add_write;
            ac->ev.delWrite = del_write;
            ac->ev.cleanup = cleanup;
            ac->ev.data = state;
        }

    } // namespace redis_adapter

    // ── RedisCommandAwaitable ──────────────────────────────────────────────────
    // Awaitable for a single Redis async command. Suspends the coroutine until
    // the hiredis callback fires with the reply.
    struct RedisCommandAwaitable
    {
        redisAsyncContext* ctx_;
        std::vector<std::string> args_;

        std::coroutine_handle<> handle_{};
        redisReply* reply_{nullptr};
        std::exception_ptr exception_{};

        [[nodiscard]] bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h)
        {
            handle_ = h;

            std::vector<const char*> argv;
            std::vector<std::size_t> argvlen;
            argv.reserve(args_.size());
            argvlen.reserve(args_.size());
            for (const auto& a : args_)
            {
                argv.push_back(a.c_str());
                argvlen.push_back(a.size());
            }

            int status = redisAsyncCommandArgv(ctx_,
                [](redisAsyncContext* /*ac*/, void* reply, void* privdata) {
                    auto* self = static_cast<RedisCommandAwaitable*>(privdata);
                    self->reply_ = static_cast<redisReply*>(reply);
                    if (!self->reply_ || self->reply_->type == REDIS_REPLY_ERROR)
                    {
                        self->exception_ = std::make_exception_ptr(
                            std::runtime_error(
                                self->reply_ ? self->reply_->str : "null reply from Redis"));
                    }
                    self->handle_.resume();
                },
                this,
                static_cast<int>(argv.size()),
                argv.data(),
                argvlen.data());

            if (status != REDIS_OK)
            {
                exception_ = std::make_exception_ptr(
                    std::runtime_error("redisAsyncCommandArgv failed"));
                h.resume();
            }
        }

        [[nodiscard]] redisReply* await_resume()
        {
            if (exception_) std::rethrow_exception(exception_);
            return reply_;
        }
    };

    // ── AsyncRedisDB ───────────────────────────────────────────────────────────
    // Async Redis connection using hiredis' async API with IoContext adapter.
    struct AsyncRedisDB
    {
        redisAsyncContext* ctx_{nullptr};
        IoContext* io_ctx_{nullptr};

        AsyncRedisDB() = default;
        AsyncRedisDB(const AsyncRedisDB&) = delete;
        AsyncRedisDB& operator=(const AsyncRedisDB&) = delete;

        AsyncRedisDB(AsyncRedisDB&& other) noexcept
            : ctx_(other.ctx_), io_ctx_(other.io_ctx_)
        {
            other.ctx_ = nullptr;
            other.io_ctx_ = nullptr;
        }

        AsyncRedisDB& operator=(AsyncRedisDB&& other) noexcept
        {
            if (this != &other)
            {
                if (ctx_) redisAsyncFree(ctx_);
                ctx_ = other.ctx_;
                io_ctx_ = other.io_ctx_;
                other.ctx_ = nullptr;
                other.io_ctx_ = nullptr;
            }
            return *this;
        }

        ~AsyncRedisDB()
        {
            if (ctx_) redisAsyncFree(ctx_);
        }

        [[nodiscard]] static auto connect(
            const char* host, int port, IoContext& ctx) -> Task<AsyncRedisDB>
        {
            AsyncRedisDB db;
            db.io_ctx_ = &ctx;
            db.ctx_ = redisAsyncConnect(host, port);

            if (!db.ctx_ || db.ctx_->err)
            {
                std::string err = db.ctx_ ? db.ctx_->errstr : "redisAsyncConnect returned null";
                if (db.ctx_)
                {
                    redisAsyncFree(db.ctx_);
                    db.ctx_ = nullptr;
                }
                throw std::runtime_error("Redis async connect failed: " + err);
            }

            redis_adapter::attach(db.ctx_, ctx);
            co_return std::move(db);
        }

        [[nodiscard]] bool is_open() const noexcept
        {
            return ctx_ != nullptr && !ctx_->err;
        }
    };

    // ── connector_trait<AsyncRedisDB> ──────────────────────────────────────────
    template <>
    struct connector_trait<AsyncRedisDB>
    {
        using supports_async = void;

        template <typename T>
        struct wire_type
        {
            using type = T;
        };

        struct cursor_type
        {
            [[nodiscard]] bool has_next() const noexcept { return false; }
        };

        // ── Async SELECT (GET key pattern) ─────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders>
        static auto async_execute(
            AsyncRedisDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
            -> Task<result<projected_type<Response>, Response>>
        {
            using Row = projected_type<Response>;
            auto key = redis_live_detail::key_for_query(q);

            auto* reply = co_await RedisCommandAwaitable{
                db.ctx_, {"HGETALL", key}};

            std::vector<Row> rows;
            if (reply && reply->type == REDIS_REPLY_ARRAY && reply->elements > 0)
                rows.push_back(redis_live_detail::make_row_from_reply<Row>(reply));

            co_return result<Row, Response>{std::move(rows)};
        }

        // ── Sync execute fallback ──────────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders>
        static auto execute(
            AsyncRedisDB& /*db*/,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> /*q*/)
            -> result<projected_type<Response>, Response>
        {
            throw std::runtime_error(
                "AsyncRedisDB does not support synchronous execute. "
                "Use async_db<AsyncRedisDB> or RedisLiveDB for sync operations.");
        }

        static void begin(AsyncRedisDB& /*db*/) {}
        static void commit(AsyncRedisDB& /*db*/) {}
        static void rollback(AsyncRedisDB& /*db*/) {}
    };

} // namespace orm

#endif // ORM_REDIS_LIVE_AVAILABLE
