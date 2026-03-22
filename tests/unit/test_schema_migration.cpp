#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/migration/migration.hpp"
#include "ORM/db/connectors/MockDB/mock_db_migration.hpp"

// ── Diff: CREATE TABLE for missing table ──────────────────────────────────────

TEST(SchemaMigration, DiffCreateTableForMissingTable)
{
    orm::live_schema live; // empty — no tables in DB

    orm::entity_meta user_entity{
        "users",
        {{"id", "INTEGER"}, {"name", "TEXT"}}
    };

    orm::MockMigrateDB db;
    orm::migrate<orm::MockMigrateDB> mig{db};
    const auto ops = mig.diff(live, std::span<const orm::entity_meta>(&user_entity, 1));

    ASSERT_EQ(ops.size(), 1u) << "Expected exactly one DDL op (CREATE TABLE)";
    ASSERT_TRUE(std::holds_alternative<orm::create_table_op>(ops[0]));
    EXPECT_EQ(std::get<orm::create_table_op>(ops[0]).table, "users");
}

// ── Diff: ADD COLUMN for missing column ───────────────────────────────────────

TEST(SchemaMigration, DiffAddColumnForMissingColumn)
{
    orm::live_schema live{
        {"users", {{"id", "INTEGER"}}} // missing "name" column
    };

    orm::entity_meta user_entity{
        "users",
        {{"id", "INTEGER"}, {"name", "TEXT"}}
    };

    orm::MockMigrateDB db;
    orm::migrate<orm::MockMigrateDB> mig{db};
    const auto ops = mig.diff(live, std::span<const orm::entity_meta>(&user_entity, 1));

    ASSERT_EQ(ops.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<orm::add_column_op>(ops[0]));
    const auto& op = std::get<orm::add_column_op>(ops[0]);
    EXPECT_EQ(op.table,  "users");
    EXPECT_EQ(op.column, "name");
    EXPECT_EQ(op.type,   "TEXT");
}

// ── Diff: DROP COLUMN for extra column in live schema ────────────────────────

TEST(SchemaMigration, DiffDropColumnForExtraLiveColumn)
{
    orm::live_schema live{
        {"users", {{"id", "INTEGER"}, {"name", "TEXT"}, {"deprecated_col", "TEXT"}}}
    };

    orm::entity_meta user_entity{
        "users",
        {{"id", "INTEGER"}, {"name", "TEXT"}} // no deprecated_col
    };

    orm::MockMigrateDB db;
    orm::migrate<orm::MockMigrateDB> mig{db};
    const auto ops = mig.diff(live, std::span<const orm::entity_meta>(&user_entity, 1));

    ASSERT_EQ(ops.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<orm::drop_column_op>(ops[0]));
    EXPECT_EQ(std::get<orm::drop_column_op>(ops[0]).column, "deprecated_col");
}

// ── Dry-run: drift detected → exit code 2 ────────────────────────────────────

TEST(SchemaMigration, DryRunDriftExitCode2)
{
    orm::live_schema live; // no tables

    orm::entity_meta entity{"orders", {{"id", "INTEGER"}}};
    orm::MockMigrateDB db;
    orm::migrate<orm::MockMigrateDB> mig{db};

    const int code = mig.run(live, std::span<const orm::entity_meta>(&entity, 1), /*dry_run=*/true);

    EXPECT_EQ(code, 2) << "Dry-run with drift must return exit code 2";
}

// ── Dry-run: in sync → exit code 0 ───────────────────────────────────────────

TEST(SchemaMigration, DryRunInSyncExitCode0)
{
    orm::live_schema live{
        {"orders", {{"id", "INTEGER"}}} // matches entity exactly
    };

    orm::entity_meta entity{"orders", {{"id", "INTEGER"}}};
    orm::MockMigrateDB db;
    orm::migrate<orm::MockMigrateDB> mig{db};

    const int code = mig.run(live, std::span<const orm::entity_meta>(&entity, 1), /*dry_run=*/true);

    EXPECT_EQ(code, 0) << "Dry-run with no drift must return exit code 0";
}

// ── DDL generation: CREATE TABLE produces non-empty SQL ──────────────────────

TEST(SchemaMigration, DdlGenerationCreateTable)
{
    orm::live_schema live;
    orm::entity_meta entity{"products", {{"id", "INTEGER"}, {"price", "REAL"}}};
    orm::MockMigrateDB db;
    orm::migrate<orm::MockMigrateDB> mig{db};

    const auto ops = mig.diff(live, std::span<const orm::entity_meta>(&entity, 1));
    const std::string ddl = mig.generate_ddl(ops);

    EXPECT_NE(ddl.find("CREATE TABLE"), std::string::npos)
        << "Generated DDL must contain CREATE TABLE";
    EXPECT_NE(ddl.find("products"), std::string::npos)
        << "Generated DDL must contain the table name";
}

// ── DDL generation: ADD COLUMN produces non-empty SQL ────────────────────────

TEST(SchemaMigration, DdlGenerationAddColumn)
{
    orm::live_schema live{{"products", {{"id", "INTEGER"}}}};
    orm::entity_meta entity{"products", {{"id", "INTEGER"}, {"price", "REAL"}}};
    orm::MockMigrateDB db;
    orm::migrate<orm::MockMigrateDB> mig{db};

    const auto ops = mig.diff(live, std::span<const orm::entity_meta>(&entity, 1));
    const std::string ddl = mig.generate_ddl(ops);

    EXPECT_NE(ddl.find("ADD COLUMN"), std::string::npos)
        << "Generated DDL must contain ADD COLUMN";
    EXPECT_NE(ddl.find("price"), std::string::npos);
}

// ── State machine: run() ends in Done state ───────────────────────────────────

TEST(SchemaMigration, StateMachineReachesDone)
{
    orm::live_schema live;
    orm::entity_meta entity{"items", {{"id", "INTEGER"}}};
    orm::MockMigrateDB db;
    orm::migrate<orm::MockMigrateDB> mig{db};

    (void)mig.run(live, std::span<const orm::entity_meta>(&entity, 1), /*dry_run=*/true);

    EXPECT_EQ(mig.state(), orm::migration_state::Done);
}
