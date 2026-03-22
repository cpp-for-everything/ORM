#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/RedisDB/redis_live.hpp"
#include <hiredis/hiredis.h>
#include <cstdlib>
#include <string>

// ── Test entity ───────────────────────────────────────────────────────────────

struct Session
{
    orm::property<std::string, "id">    id;
    orm::property<std::string, "user">  user;
    orm::property<int, "ttl">           ttl;
};

namespace orm
{
    template <>
    struct table_name_trait<Session>
    {
        static constexpr std::string_view value = "session";
    };
} // namespace orm

// ── Connection helper ─────────────────────────────────────────────────────────

[[nodiscard]] static orm::RedisLiveDB make_connection()
{
    const char* host     = std::getenv("ORM_REDIS_HOST");
    const char* port_str = std::getenv("ORM_REDIS_PORT");
    return orm::RedisLiveDB::connect(
        host     ? host : "127.0.0.1",
        port_str ? std::stoi(port_str) : 6379);
}

// ── Fixture ───────────────────────────────────────────────────────────────────

class RedisLiveTest : public ::testing::Test
{
protected:
    orm::RedisLiveDB conn;
    orm::db<orm::RedisLiveDB> dbc{conn};

    void SetUp() override
    {
        conn = make_connection();
        dbc.rebind(conn);
        // Flush DB 0 to start clean
        if (conn.ctx_) {
            redisReply* reply = static_cast<redisReply*>(redisCommand(conn.ctx_, "FLUSHDB"));
            if (reply) freeReplyObject(reply);
        }
    }
};

// ── Tests ──────────────────────────────────────────────────────────────────────

TEST_F(RedisLiveTest, ConnectionIsOpen)
{
    EXPECT_TRUE(conn.is_open());
}

TEST_F(RedisLiveTest, InsertSingleFieldAndGet)
{
    // INSERT with only PK value uses SET key val
    constexpr auto ins = orm::insert(orm::field<&Session::id>);
    dbc.execute(ins, std::string("abc123"), std::string("active"));

    // Verify via direct EXISTS command
    redisReply* reply = static_cast<redisReply*>(redisCommand(conn.ctx_, "EXISTS session:abc123"));
    ASSERT_NE(reply, nullptr);
    EXPECT_EQ(reply->integer, 1);
    freeReplyObject(reply);
}

TEST_F(RedisLiveTest, InsertMultiFieldAndHGetAll)
{
    constexpr auto ins = orm::insert(
        orm::field<&Session::id>,
        orm::field<&Session::user>,
        orm::field<&Session::ttl>);
    dbc.execute(ins, std::string("sess1"), std::string("alice"), 3600, std::string("alice"), 3600);

    // Verify fields via HGETALL
    redisReply* reply = static_cast<redisReply*>(redisCommand(conn.ctx_, "HGETALL session:sess1"));
    ASSERT_NE(reply, nullptr);
    ASSERT_EQ(reply->type, REDIS_REPLY_ARRAY);
    EXPECT_GT(reply->elements, 0u);
    freeReplyObject(reply);
}

TEST_F(RedisLiveTest, DeleteRemovesKey)
{
    // Insert a key
    redisReply* setup = static_cast<redisReply*>(redisCommand(conn.ctx_, "SET session:del_me test_val"));
    if (setup) freeReplyObject(setup);

    constexpr auto del = orm::deleteq<Session>()
        .where(orm::field<&Session::id> == orm::Placeholder<std::string>{});
    dbc.execute(del, std::string("del_me"));

    redisReply* reply = static_cast<redisReply*>(redisCommand(conn.ctx_, "EXISTS session:del_me"));
    ASSERT_NE(reply, nullptr);
    EXPECT_EQ(reply->integer, 0);
    freeReplyObject(reply);
}

TEST_F(RedisLiveTest, SelectWithPkReturnsValue)
{
    redisReply* setup = static_cast<redisReply*>(redisCommand(conn.ctx_, "SET session:lookup_key my_value"));
    if (setup) freeReplyObject(setup);

    constexpr auto sel = orm::select(orm::field<&Session::id>)
        .where(orm::field<&Session::id> == orm::Placeholder<std::string>{});
    const auto rows = dbc.execute(sel, std::string("lookup_key")).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), "my_value");
}
