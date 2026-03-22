#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/CassandraDB/cassandra_db.hpp"

// ── Test entity ───────────────────────────────────────────────────────────────

struct CassRow
{
    orm::property<int, "partition_key"> partition_key;
    orm::property<int, "clustering">    clustering;
    orm::property<std::u8string, "val"> val;
};

// ── is_connector concept ──────────────────────────────────────────────────────

TEST(CassandraConnector, SatisfiesIsConnector)
{
    static_assert(orm::is_connector<orm::CassandraDB>,
        "connector_trait<CassandraDB> must satisfy is_connector<CassandraDB>");
}

// ── Capability tags ───────────────────────────────────────────────────────────

TEST(CassandraConnector, NoJoinsCapability)
{
    static_assert(!orm::has_capability<orm::CassandraDB, orm::cap::supports_joins>,
        "CassandraDB must NOT declare supports_joins");
}

TEST(CassandraConnector, SupportsTransactions)
{
    static_assert(orm::has_capability<orm::CassandraDB, orm::cap::supports_transactions>);
}

// ── CQL with partition key — compiles and renders correctly ───────────────────

TEST(CassandraConnector, SelectWithPartitionKeyRendersCorrectCql)
{
    orm::CassandraDB db;
    orm::db<orm::CassandraDB> dbc{db};

    const auto q = orm::select(orm::field<&CassRow::partition_key>, orm::field<&CassRow::val>)
                       .where(orm::field<&CassRow::partition_key> == orm::Placeholder<int>{});

    dbc.execute(q, 42);

    EXPECT_NE(db.session.last_cql.find("SELECT"),         std::string::npos);
    EXPECT_NE(db.session.last_cql.find("partition_key"),  std::string::npos);
    EXPECT_NE(db.session.last_cql.find("WHERE"),          std::string::npos);
    EXPECT_NE(db.session.last_cql.find("?"),              std::string::npos);
    ASSERT_EQ(db.session.last_params.size(), 1u);
    EXPECT_EQ(db.session.last_params[0], "42");
}

// ── Two-predicate CQL (partition key + clustering key) ────────────────────────

TEST(CassandraConnector, TwoPredicateCql)
{
    orm::CassandraDB db;
    orm::db<orm::CassandraDB> dbc{db};

    const auto q = orm::select(orm::field<&CassRow::val>)
                       .where(orm::field<&CassRow::partition_key> == orm::Placeholder<int>{})
                       .where(orm::field<&CassRow::clustering>    == orm::Placeholder<int>{});

    dbc.execute(q, 1, 2);

    EXPECT_NE(db.session.last_cql.find("partition_key"), std::string::npos);
    EXPECT_NE(db.session.last_cql.find("clustering"),    std::string::npos);
    ASSERT_EQ(db.session.last_params.size(), 2u);
}

// ── cass_result_free RAII ─────────────────────────────────────────────────────

TEST(CassandraConnector, ResultFreeCalledOnce)
{
    orm::CassandraDB db;
    orm::db<orm::CassandraDB> dbc{db};

    EXPECT_EQ(db.session.result_free_count, 0);

    const auto q = orm::select(orm::field<&CassRow::val>)
                       .where(orm::field<&CassRow::partition_key> == orm::Placeholder<int>{});
    dbc.execute(q, 7);

    EXPECT_EQ(db.session.result_free_count, 1)
        << "cass_result_free must be called exactly once per execute()";
}
