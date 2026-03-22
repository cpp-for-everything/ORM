#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MySQLDB/mysql_db.hpp"

// ── Test entity ───────────────────────────────────────────────────────────────

struct User
{
    orm::property<int, "id">              id;
    orm::property<std::u8string, "name"> name;
    orm::property<int, "age">            age;
};

// ── is_connector concept ──────────────────────────────────────────────────────

TEST(MySQLConnector, SatisfiesIsConnector)
{
    static_assert(orm::is_connector<orm::MySQLDB>,
        "connector_trait<MySQLDB> must satisfy is_connector<MySQLDB>");
}

// ── Capability tags ───────────────────────────────────────────────────────────

TEST(MySQLConnector, SupportsJoinsCapabilityPresent)
{
    static_assert(orm::has_capability<orm::MySQLDB, orm::cap::supports_joins>,
        "MySQLDB must declare supports_joins");
}

TEST(MySQLConnector, SupportsTransactionsCapabilityPresent)
{
    static_assert(orm::has_capability<orm::MySQLDB, orm::cap::supports_transactions>,
        "MySQLDB must declare supports_transactions");
}

TEST(MySQLConnector, SupportsAggregationNotDeclared)
{
    static_assert(!orm::has_capability<orm::MySQLDB, orm::cap::supports_aggregation>,
        "MySQLDB must NOT declare supports_aggregation");
}

// ── SELECT with positional placeholder ────────────────────────────────────────

TEST(MySQLConnector, SelectPositionalPlaceholder)
{
    orm::MySQLDB db;
    orm::db<orm::MySQLDB> dbc{db};

    const auto q = orm::select(orm::field<&User::id>, orm::field<&User::name>)
                       .where(orm::field<&User::id> == orm::Placeholder<int>{});

    dbc.execute(q, 42);

    EXPECT_NE(db.handle.last_sql.find("SELECT"), std::string::npos);
    EXPECT_NE(db.handle.last_sql.find("?"), std::string::npos);
    ASSERT_EQ(db.handle.last_params.size(), 1u);
    EXPECT_EQ(db.handle.last_params[0], "42");
}

// ── SELECT with no parameters ─────────────────────────────────────────────────

TEST(MySQLConnector, SelectNoParams)
{
    orm::MySQLDB db;
    orm::db<orm::MySQLDB> dbc{db};

    const auto q = orm::select(orm::field<&User::id>);
    dbc << q;

    EXPECT_NE(db.handle.last_sql.find("SELECT"), std::string::npos);
    EXPECT_NE(db.handle.last_sql.find("id"), std::string::npos);
}

// ── Indexed-placeholder rewrite ───────────────────────────────────────────────
// orm::ph<int, _1> used in two WHERE predicates.
// MySQL has no native $N; each occurrence maps to a separate positional "?"
// and the caller duplicates the runtime argument value in the bind list.

TEST(MySQLConnector, IndexedPlaceholderRewrite)
{
    using namespace std::placeholders;

    orm::MySQLDB db;
    orm::db<orm::MySQLDB> dbc{db};

    // Two WHERE conditions that both reference ph<int, _1>.
    // The render_rule emits "?" for each IndexedPlaceholder occurrence.
    // We pass the value twice (once per occurrence) to reflect MySQL's
    // positional binding requirement.
    const auto q = orm::select(orm::field<&User::id>)
                       .where(orm::field<&User::age> == orm::ph<int, _1>)
                       .where(orm::field<&User::id>  == orm::ph<int, _1>);

    dbc.execute(q, 99, 99); // argument duplicated for each ? occurrence

    // SQL must contain two "?" for the two WHERE predicates
    const std::string& sql = db.handle.last_sql;
    EXPECT_NE(sql.find("WHERE"), std::string::npos);

    std::size_t first  = sql.find('?');
    std::size_t second = (first != std::string::npos)
                           ? sql.find('?', first + 1)
                           : std::string::npos;
    EXPECT_NE(first,  std::string::npos) << "Expected first ? in SQL";
    EXPECT_NE(second, std::string::npos) << "Expected second ? in SQL for indexed-placeholder reuse";

    // Both bind slots carry the same value
    ASSERT_EQ(db.handle.last_params.size(), 2u);
    EXPECT_EQ(db.handle.last_params[0], "99");
    EXPECT_EQ(db.handle.last_params[1], "99");
}

// ── RAII stmt_close ───────────────────────────────────────────────────────────

TEST(MySQLConnector, RaiiStmtCloseCalledOnce)
{
    orm::MySQLDB db;
    orm::db<orm::MySQLDB> dbc{db};

    EXPECT_EQ(db.handle.stmt_close_count, 0);

    const auto q = orm::select(orm::field<&User::id>);
    dbc << q;

    EXPECT_EQ(db.handle.stmt_close_count, 1)
        << "close_stmt() must be called exactly once per execute() invocation";
}

TEST(MySQLConnector, RaiiStmtCloseCalledOnceWithParams)
{
    orm::MySQLDB db;
    orm::db<orm::MySQLDB> dbc{db};

    const auto q = orm::select(orm::field<&User::id>)
                       .where(orm::field<&User::id> == orm::Placeholder<int>{});
    dbc.execute(q, 7);

    EXPECT_EQ(db.handle.stmt_close_count, 1)
        << "close_stmt() must be called exactly once even when runtime params are supplied";
}

// ── SQL string correctness ────────────────────────────────────────────────────

TEST(MySQLConnector, SelectSqlContainsColumnNames)
{
    orm::MySQLDB db;
    orm::db<orm::MySQLDB> dbc{db};

    dbc << orm::select(orm::field<&User::id>, orm::field<&User::name>);

    EXPECT_NE(db.handle.last_sql.find("id"),   std::string::npos);
    EXPECT_NE(db.handle.last_sql.find("name"), std::string::npos);
    EXPECT_NE(db.handle.last_sql.find("FROM"), std::string::npos);
}
