#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/RedisDB/redis_db.hpp"

// ── Test entities ─────────────────────────────────────────────────────────────

struct CacheItem
{
    orm::property<int, "id">    id;
    orm::property<int, "value"> value;
};

struct SingleColItem
{
    orm::property<int, "id"> id;
};

// ── is_connector concept ──────────────────────────────────────────────────────

TEST(RedisConnector, SatisfiesIsConnector)
{
    static_assert(orm::is_connector<orm::RedisDB>,
        "connector_trait<RedisDB> must satisfy is_connector<RedisDB>");
}

// ── No capability tags ────────────────────────────────────────────────────────

TEST(RedisConnector, NoJoinsCapability)
{
    static_assert(!orm::has_capability<orm::RedisDB, orm::cap::supports_joins>,
        "RedisDB must NOT declare supports_joins");
}

TEST(RedisConnector, NoTransactionsCapability)
{
    static_assert(!orm::has_capability<orm::RedisDB, orm::cap::supports_transactions>,
        "RedisDB must NOT declare supports_transactions");
}

TEST(RedisConnector, NoAggregationCapability)
{
    static_assert(!orm::has_capability<orm::RedisDB, orm::cap::supports_aggregation>,
        "RedisDB must NOT declare supports_aggregation");
}

// ── Single-column SET command ─────────────────────────────────────────────────

TEST(RedisConnector, SingleColumnInsertIssuesSet)
{
    orm::RedisDB db;
    orm::db<orm::RedisDB> dbc{db};

    const auto q = orm::insert(orm::field<&SingleColItem::id>);
    dbc.execute(q, 42, 100);

    EXPECT_EQ(db.ctx.last_command, "SET")
        << "Single-column INSERT must issue SET command";
    EXPECT_NE(db.ctx.last_key.find("42"), std::string::npos)
        << "Key must contain the primary-key value";
}

// ── Multi-column HSET command ─────────────────────────────────────────────────

TEST(RedisConnector, MultiColumnInsertIssuesHset)
{
    orm::RedisDB db;
    orm::db<orm::RedisDB> dbc{db};

    const auto q = orm::insert(orm::field<&CacheItem::id>, orm::field<&CacheItem::value>);
    dbc.execute(q, 7, 99);

    EXPECT_EQ(db.ctx.last_command, "HSET")
        << "Multi-column INSERT must issue HSET command";
    EXPECT_NE(db.ctx.last_key.find("7"), std::string::npos)
        << "Key must contain the primary-key value";
}

// ── Single-column SELECT issues GET ──────────────────────────────────────────

TEST(RedisConnector, SingleColumnSelectIssuesGet)
{
    orm::RedisDB db;
    orm::db<orm::RedisDB> dbc{db};

    const auto q = orm::select(orm::field<&SingleColItem::id>)
                       .where(orm::field<&SingleColItem::id> == orm::Placeholder<int>{});
    dbc.execute(q, 42);

    EXPECT_EQ(db.ctx.last_command, "GET")
        << "Single-column SELECT must issue GET command";
    EXPECT_NE(db.ctx.last_key.find("42"), std::string::npos);
}

// ── Multi-column SELECT issues HGETALL ────────────────────────────────────────

TEST(RedisConnector, MultiColumnSelectIssuesHgetall)
{
    orm::RedisDB db;
    orm::db<orm::RedisDB> dbc{db};

    const auto q = orm::select(orm::field<&CacheItem::id>, orm::field<&CacheItem::value>)
                       .where(orm::field<&CacheItem::id> == orm::Placeholder<int>{});
    dbc.execute(q, 5);

    EXPECT_EQ(db.ctx.last_command, "HGETALL")
        << "Multi-column SELECT must issue HGETALL command";
}
