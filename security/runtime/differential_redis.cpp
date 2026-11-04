// Differential test on Redis. See differential_common.hpp for the outcomes and
// the failure rule, and differential_sqlite.cpp for the idea.
//
//   naive  a command assembled as text by concatenation and sent in the inline
//          protocol:  GET sec_item:<payload> CR LF
//          so a CR LF inside the payload ends the command and starts another.
//          This is what hand built command strings do. It is not hiredis "%s"
//          formatting, which sends the argument as a length prefixed bulk string
//          and is safe. A sentinel command (ECHO) follows, so the replies to
//          every injected command can be counted.
//   orm    select(name).where(name == Placeholder<std::string>{}) with the payload
//          supplied to execute(). A fresh connection per payload, and a PING
//          afterwards on the same connection: a forged reply that desynchronises
//          the stream shows up there.
//
// Every payload starts from FLUSHALL and three seeded string keys plus one key
// holding a secret. The snapshot covers all keys with their values and the
// configuration values requirepass, dir and dbfilename.
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/RedisDB/redis_live.hpp"
#include "differential_common.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <sys/time.h>
#include <vector>

struct Item
{
    orm::property<std::string, "name">  name;
    orm::property<std::string, "color"> color;
};

namespace orm
{
    template <>
    struct table_name_trait<Item>
    {
        static constexpr std::string_view value = "sec_item";
    };
} // namespace orm

namespace
{
    using namespace orm_security;

    constexpr const char* kEnd = "__END_OF_PAYLOAD__";

    redisContext* connect_raw()
    {
        redisContext* c = redisConnect(env_or("ORM_REDIS_HOST", "127.0.0.1"),
                                       std::atoi(env_or("ORM_REDIS_PORT", "6379")));
        if (!c || c->err)
        {
            const std::string e = c ? c->errstr : "redisConnect returned null";
            if (c) redisFree(c);
            throw std::runtime_error("Redis connection failed: " + e);
        }
        const timeval tv{3, 0};
        redisSetTimeout(c, tv);
        return c;
    }

    struct Ctx
    {
        redisContext* c;
        Ctx() : c(connect_raw()) {}
        Ctx(const Ctx&)            = delete;
        Ctx& operator=(const Ctx&) = delete;
        ~Ctx() { if (c) redisFree(c); }
    };

    std::string reply_text(redisReply* r)
    {
        if (!r) return "<none>";
        std::string v;
        switch (r->type)
        {
            case REDIS_REPLY_STRING:
            case REDIS_REPLY_STATUS:
            case REDIS_REPLY_ERROR: v.assign(r->str, r->len); break;
            case REDIS_REPLY_INTEGER: v = std::to_string(r->integer); break;
            case REDIS_REPLY_NIL: v = "<nil>"; break;
            case REDIS_REPLY_ARRAY:
                for (std::size_t i = 0; i < r->elements; ++i) v += reply_text(r->element[i]) + ",";
                break;
            default: v = "<other>"; break;
        }
        return v;
    }

    std::string cmd(redisContext* c, const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        auto* r = static_cast<redisReply*>(redisvCommand(c, fmt, ap));
        va_end(ap);
        std::string v = reply_text(r);
        if (r) freeReplyObject(r);
        return v;
    }

    std::string snapshot(redisContext* c)
    {
        std::string out;
        auto* keys = static_cast<redisReply*>(redisCommand(c, "KEYS *"));
        std::vector<std::string> names;
        if (keys && keys->type == REDIS_REPLY_ARRAY)
            for (std::size_t i = 0; i < keys->elements; ++i)
                names.emplace_back(keys->element[i]->str, keys->element[i]->len);
        if (keys) freeReplyObject(keys);
        std::sort(names.begin(), names.end());
        for (const auto& k : names)
            out += k + "=" + cmd(c, "TYPE %b", k.data(), k.size()) + ":" + cmd(c, "GET %b", k.data(), k.size()) + ";";
        out += "requirepass=" + cmd(c, "CONFIG GET requirepass");
        out += "dir=" + cmd(c, "CONFIG GET dir");
        out += "dbfilename=" + cmd(c, "CONFIG GET dbfilename");
        return out;
    }

    void fresh(redisContext* c)
    {
        cmd(c, "FLUSHALL");
        cmd(c, "SET sec_item:apple red");
        cmd(c, "SET sec_item:banana yellow");
        cmd(c, "SET sec_item:cherry red");
        cmd(c, "SET sec_secret:admin correct-horse");
    }

    bool timed_out(const redisContext* c)
    {
#ifdef REDIS_ERR_TIMEOUT
        if (c->err == REDIS_ERR_TIMEOUT) return true;
#endif
        return c->err == REDIS_ERR_IO && (errno == EAGAIN || errno == EWOULDBLOCK);
    }

    Run run_naive(redisContext* admin, const std::string& payload)
    {
        fresh(admin);
        const std::string before = snapshot(admin);

        Ctx victim;
        const std::string wire = "GET sec_item:" + payload + "\r\nECHO " + kEnd + "\r\n";
        long rows = 0;
        bool errored = false, tripped = false;
        const Stopwatch sw;
        if (redisAppendFormattedCommand(victim.c, wire.data(), wire.size()) != REDIS_OK)
            errored = true;
        else
            for (int idx = 0; idx < 64; ++idx)
            {
                void* raw = nullptr;
                if (redisGetReply(victim.c, &raw) != REDIS_OK)
                {
                    if (timed_out(victim.c)) tripped = true;
                    else errored = true;
                    break;
                }
                auto* r = static_cast<redisReply*>(raw);
                const bool sentinel = r->type == REDIS_REPLY_STRING && reply_text(r) == kEnd;
                if (!sentinel)
                {
                    if (r->type == REDIS_REPLY_ERROR) errored = true;
                    else if (r->type != REDIS_REPLY_NIL) ++rows;
                }
                freeReplyObject(r);
                if (sentinel) break;
            }
        if (sw.ms() >= kExecGuardMs) tripped = true;
        return classify(tripped, before != snapshot(admin), rows, errored);
    }

    Run run_library(redisContext* admin, const std::string& payload)
    {
        fresh(admin);
        const std::string before = snapshot(admin);

        orm::RedisLiveDB lib = orm::RedisLiveDB::connect(env_or("ORM_REDIS_HOST", "127.0.0.1"),
                                                         std::atoi(env_or("ORM_REDIS_PORT", "6379")));
        orm::db<orm::RedisLiveDB> db{lib};
        constexpr auto q = orm::select(orm::field<&Item::name>)
                               .where(orm::field<&Item::name> == orm::Placeholder<std::string>{});
        long rows = 0;
        bool errored = false;
        const Stopwatch sw;
        try
        {
            rows = static_cast<long>(db.execute(q, payload).to_vector().size());
        }
        catch (const std::exception&)
        {
            errored = true;
        }
        // The stream must still be in step: the next reply must be ours.
        const bool desync = cmd(lib.ctx_, "PING") != "PONG";
        const bool tripped = sw.ms() >= kExecGuardMs;
        return classify(tripped, desync || before != snapshot(admin), rows, errored);
    }

    std::string server_version(redisContext* c)
    {
        const std::string info = cmd(c, "INFO server");
        const auto pos = info.find("redis_version:");
        if (pos == std::string::npos) return "unknown";
        const auto end = info.find_first_of("\r\n", pos);
        return info.substr(pos + 14, end - pos - 14);
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        Ctx admin;
        Backend b;
        b.name       = "redis";
        b.server     = "Redis " + server_version(admin.c);
        b.naive_desc = "command text built by concatenation and sent in the inline protocol, CR LF ends a command";
        b.applies    = [](const Payload& p) { return p.cls == "redis"; };
        b.naive      = [&](const std::string& p) { return run_naive(admin.c, p); };
        b.library    = [&](const std::string& p) { return run_library(admin.c, p); };
        return run_differential(argc, argv, b);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
