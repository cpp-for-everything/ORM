#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/PostgreSQLDB/postgresql_db.hpp"

// ── Test entity ───────────────────────────────────────────────────────────────

struct PGUser
{
    orm::property<int, "id">              id;
    orm::property<std::u8string, "name"> name;
    orm::property<int, "age">            age;
};

// ── is_connector concept ──────────────────────────────────────────────────────

TEST(PGConnector, SatisfiesIsConnector)
{
    static_assert(orm::is_connector<orm::PostgreSQLDB>,
        "connector_trait<PostgreSQLDB> must satisfy is_connector<PostgreSQLDB>");
}

// ── Capability tags ───────────────────────────────────────────────────────────

TEST(PGConnector, SupportsJoinsCapabilityPresent)
{
    static_assert(orm::has_capability<orm::PostgreSQLDB, orm::cap::supports_joins>);
}

TEST(PGConnector, SupportsTransactionsCapabilityPresent)
{
    static_assert(orm::has_capability<orm::PostgreSQLDB, orm::cap::supports_transactions>);
}

TEST(PGConnector, SupportsAggregationCapabilityPresent)
{
    static_assert(orm::has_capability<orm::PostgreSQLDB, orm::cap::supports_aggregation>,
        "PostgreSQLDB must declare supports_aggregation");
}

// ── Dollar-parameter rendering ────────────────────────────────────────────────

TEST(PGConnector, DollarParamRendering)
{
    orm::PostgreSQLDB db;
    orm::db<orm::PostgreSQLDB> dbc{db};

    const auto q = orm::select(orm::field<&PGUser::id>)
                       .where(orm::field<&PGUser::id>  == orm::Placeholder<int>{})
                       .where(orm::field<&PGUser::age> == orm::Placeholder<int>{});

    dbc.execute(q, 1, 25);

    EXPECT_NE(db.conn.last_sql.find("$1"), std::string::npos) << "Expected $1 in SQL";
    EXPECT_NE(db.conn.last_sql.find("$2"), std::string::npos) << "Expected $2 in SQL";
    EXPECT_EQ(db.conn.nparams_used, 2);
    ASSERT_EQ(db.conn.last_params.size(), 2u);
    EXPECT_EQ(db.conn.last_params[0], "1");
    EXPECT_EQ(db.conn.last_params[1], "25");
}

// ── Indexed-placeholder native reuse ─────────────────────────────────────────
// _1 used twice → same $1 at both positions; nparams_used == 1.

TEST(PGConnector, IndexedPlaceholderNativeReuse)
{
    using namespace std::placeholders;

    orm::PostgreSQLDB db;
    orm::db<orm::PostgreSQLDB> dbc{db};

    const auto q = orm::select(orm::field<&PGUser::id>)
                       .where(orm::field<&PGUser::age> == orm::ph<int, _1>)
                       .where(orm::field<&PGUser::id>  == orm::ph<int, _1>);

    dbc.execute(q, 42);

    const std::string& sql = db.conn.last_sql;
    EXPECT_NE(sql.find("WHERE"), std::string::npos);

    // Both predicates must reference $1 (same token, native reuse)
    std::size_t first  = sql.find("$1");
    std::size_t second = (first != std::string::npos)
                           ? sql.find("$1", first + 1)
                           : std::string::npos;
    EXPECT_NE(first,  std::string::npos) << "Expected first $1 in SQL";
    EXPECT_NE(second, std::string::npos) << "Expected second $1 in SQL for native reuse";

    // nparams == 1 (only one distinct placeholder index)
    EXPECT_EQ(db.conn.nparams_used, 1)
        << "nparams must equal number of distinct placeholder indices (not occurrences)";

    // Values array has exactly 1 entry
    ASSERT_EQ(db.conn.last_params.size(), 1u);
    EXPECT_EQ(db.conn.last_params[0], "42");
}

// ── PQclear (RAII) ────────────────────────────────────────────────────────────

TEST(PGConnector, PQClearCalledOnce)
{
    orm::PostgreSQLDB db;
    orm::db<orm::PostgreSQLDB> dbc{db};

    EXPECT_EQ(db.conn.pq_clear_count, 0);
    dbc << orm::select(orm::field<&PGUser::id>);
    EXPECT_EQ(db.conn.pq_clear_count, 1)
        << "PQclear must be called exactly once per execute()";
}

TEST(PGConnector, PQClearCalledOnceWithParams)
{
    orm::PostgreSQLDB db;
    orm::db<orm::PostgreSQLDB> dbc{db};

    const auto q = orm::select(orm::field<&PGUser::id>)
                       .where(orm::field<&PGUser::id> == orm::Placeholder<int>{});
    dbc.execute(q, 5);

    EXPECT_EQ(db.conn.pq_clear_count, 1);
}

// ── SQL content ───────────────────────────────────────────────────────────────

TEST(PGConnector, SelectSqlContainsDollarParams)
{
    orm::PostgreSQLDB db;
    orm::db<orm::PostgreSQLDB> dbc{db};

    const auto q = orm::select(orm::field<&PGUser::name>)
                       .where(orm::field<&PGUser::id> == orm::Placeholder<int>{});
    dbc.execute(q, 3);

    EXPECT_NE(db.conn.last_sql.find("SELECT"), std::string::npos);
    EXPECT_NE(db.conn.last_sql.find("name"),   std::string::npos);
    EXPECT_NE(db.conn.last_sql.find("$1"),     std::string::npos);
    EXPECT_EQ(db.conn.nparams_used, 1);
}
