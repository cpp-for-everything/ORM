// Safe migrations, as named tests. The migration differential covers the inputs. These
// pin the rules one by one, so that a regression is reported by name.
#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/MockDB/mock_db.hpp"
#include "ORM/db/connectors/SQLite/sqlite_db.hpp"
#include "ORM/db/migration/migration.hpp"
#include "ORM/db/connectors/MockDB/mock_db_migration.hpp"
#include <string>
#include <vector>

struct Note
{
    orm::property<int, "id">                  id;
    orm::property<std::string, "body">        body;
    orm::property<double, "score">            score;
    orm::property<std::int64_t, "created_at"> created_at;
    orm::property<bool, "archived">           archived;
};

namespace orm
{
    template <>
    struct table_name_trait<Note>
    {
        static constexpr std::string_view value = "notes";
    };
} // namespace orm

namespace
{
    using orm::destructive_ops;
    using orm::entity_meta;
    using orm::migration_refused;
    using orm::migration_state;

    std::span<const entity_meta> one(const entity_meta& e) { return {&e, 1}; }

    constexpr orm::sql_dialect kPg{'"', orm::alter_type_form::alter_column_type, true};
    constexpr orm::sql_dialect kMy{'`', orm::alter_type_form::modify_column, false};
    constexpr orm::sql_dialect kLite{'"', orm::alter_type_form::unsupported, true};

    class MigrateSqlite : public ::testing::Test
    {
    protected:
        orm::SQLiteDB               conn = orm::SQLiteDB::open(":memory:");
        orm::migrate<orm::SQLiteDB> mig{conn};

        void exec(const char* sql)
        {
            ASSERT_EQ(sqlite3_exec(conn.handle, sql, nullptr, nullptr, nullptr), SQLITE_OK) << sql;
        }
        bool has_table(const char* name)
        {
            return orm::connector_trait<orm::SQLiteDB>::read_schema(conn).contains(name);
        }
        bool has_column(const char* table, const char* column)
        {
            const auto live = orm::connector_trait<orm::SQLiteDB>::read_schema(conn);
            return live.contains(table) && live.at(table).contains(column);
        }
    };
} // namespace

TEST(Identifier, AcceptsPlainNames)
{
    EXPECT_TRUE(orm::ddl::is_safe_identifier("users"));
    EXPECT_TRUE(orm::ddl::is_safe_identifier("_private"));
    EXPECT_TRUE(orm::ddl::is_safe_identifier("Table2"));
    EXPECT_TRUE(orm::ddl::is_safe_identifier(std::string(63, 'a')));
}

TEST(Identifier, RefusesEverythingElse)
{
    for (const char* bad : {"", "1abc", "a b", "a;b", "a--", "a\"b", "a`b", "a'b", "a.b",
                            "a/**/b", "a\nb", "a\tb", "x; DROP TABLE t; --"})
        EXPECT_FALSE(orm::ddl::is_safe_identifier(bad)) << bad;
    EXPECT_FALSE(orm::ddl::is_safe_identifier(std::string(64, 'a')));
    EXPECT_FALSE(orm::ddl::is_safe_identifier("caf\xC3\xA9")) << "bytes above ASCII";
    EXPECT_FALSE(orm::ddl::is_safe_identifier(std::string("a\0b", 3))) << "embedded NUL";
}

TEST(ColumnType, AcceptsTheClosedList)
{
    EXPECT_EQ(orm::ddl::checked_type("text"), "TEXT");
    EXPECT_EQ(orm::ddl::checked_type("Integer"), "INTEGER");
    EXPECT_EQ(orm::ddl::checked_type("DOUBLE PRECISION"), "DOUBLE PRECISION");
    EXPECT_EQ(orm::ddl::checked_type("varchar(255)"), "VARCHAR(255)");
}

TEST(ColumnType, RefusesEverythingElse)
{
    for (const char* bad : {"", "TEXT; DROP TABLE t", "TEXT DEFAULT 1", "TEXT NOT NULL",
                            "VARCHAR()", "VARCHAR(0)", "VARCHAR(999999)", "VARCHAR(1); --",
                            "VARCHAR(1) DEFAULT 1", "INTEGER PRIMARY KEY"})
        EXPECT_THROW((void)orm::ddl::checked_type(bad), migration_refused) << bad;
}

TEST(Rendering, QuotesPerDialect)
{
    const orm::add_column_op op{"orders", "order", "TEXT"};
    EXPECT_EQ(orm::ddl::render(op, kPg), R"(ALTER TABLE "orders" ADD COLUMN "order" TEXT)");
    EXPECT_EQ(orm::ddl::render(op, kMy), "ALTER TABLE `orders` ADD COLUMN `order` TEXT");
}

TEST(Rendering, AlterTypeFollowsTheEngine)
{
    const orm::alter_column_type_op op{"t", "c", "BIGINT"};
    EXPECT_EQ(orm::ddl::render(op, kPg), R"(ALTER TABLE "t" ALTER COLUMN "c" TYPE BIGINT)");
    EXPECT_EQ(orm::ddl::render(op, kMy), "ALTER TABLE `t` MODIFY COLUMN `c` BIGINT");
    EXPECT_THROW((void)orm::ddl::render(op, kLite), migration_refused);
}

TEST(Rendering, EveryOperationChecksEveryName)
{
    const std::string bad = "x; DROP TABLE t; --";
    EXPECT_THROW((void)orm::ddl::render(orm::create_table_op{bad, {{"a", "TEXT"}}}, kPg), migration_refused);
    EXPECT_THROW((void)orm::ddl::render(orm::create_table_op{"t", {{bad, "TEXT"}}}, kPg), migration_refused);
    EXPECT_THROW((void)orm::ddl::render(orm::create_table_op{"t", {{"a", bad}}}, kPg), migration_refused);
    EXPECT_THROW((void)orm::ddl::render(orm::add_column_op{bad, "a", "TEXT"}, kPg), migration_refused);
    EXPECT_THROW((void)orm::ddl::render(orm::add_column_op{"t", bad, "TEXT"}, kPg), migration_refused);
    EXPECT_THROW((void)orm::ddl::render(orm::add_column_op{"t", "a", bad}, kPg), migration_refused);
    EXPECT_THROW((void)orm::ddl::render(orm::drop_column_op{bad, "a"}, kPg), migration_refused);
    EXPECT_THROW((void)orm::ddl::render(orm::drop_column_op{"t", bad}, kPg), migration_refused);
    EXPECT_THROW((void)orm::ddl::render(orm::alter_column_type_op{"t", "a", bad}, kPg), migration_refused);
}

TEST(EntityMeta, ComesFromTheDeclaration)
{
    const entity_meta m = orm::entity_meta_of<&Note::id, &Note::body, &Note::score,
                                              &Note::created_at, &Note::archived>();
    EXPECT_EQ(m.table_name, "notes");
    ASSERT_EQ(m.columns.size(), 5u);
    EXPECT_EQ(m.columns[0], (std::pair<std::string, std::string>{"id", "INTEGER"}));
    EXPECT_EQ(m.columns[1], (std::pair<std::string, std::string>{"body", "TEXT"}));
    EXPECT_EQ(m.columns[2], (std::pair<std::string, std::string>{"score", "DOUBLE PRECISION"}));
    EXPECT_EQ(m.columns[3], (std::pair<std::string, std::string>{"created_at", "BIGINT"}));
    EXPECT_EQ(m.columns[4], (std::pair<std::string, std::string>{"archived", "BOOLEAN"}));
}

// One bad name among good ones: nothing at all may run.
TEST(Run, ChecksEverythingBeforeRunningAnything)
{
    orm::MockMigrateDB db;
    orm::migrate<orm::MockMigrateDB> mig{db};
    const std::vector<entity_meta> entities{
        {"good", {{"id", "INTEGER"}}},
        {"x; DROP TABLE t; --", {{"id", "INTEGER"}}},
    };
    EXPECT_THROW((void)mig.run({}, entities), migration_refused);
    EXPECT_TRUE(db.executed_ddl.empty());
    EXPECT_EQ(mig.state(), migration_state::Failed);
}

TEST(Run, ADryRunRefusesToo)
{
    orm::MockMigrateDB db;
    orm::migrate<orm::MockMigrateDB> mig{db};
    const entity_meta bad{"x; DROP TABLE t; --", {{"id", "INTEGER"}}};
    EXPECT_THROW((void)mig.run({}, one(bad), /*dry_run=*/true), migration_refused);
}

TEST(Run, RunsOneStatementPerOperation)
{
    orm::MockMigrateDB db;
    orm::migrate<orm::MockMigrateDB> mig{db};
    const entity_meta e{"t", {{"a", "TEXT"}, {"b", "TEXT"}}};
    const orm::live_schema live{{"t", {{"a", "TEXT"}}}};
    EXPECT_EQ(mig.run({}, one(e)), 0);
    EXPECT_EQ(mig.run(live, one(e)), 0);
    ASSERT_EQ(db.executed_ddl.size(), 2u);
    for (const auto& s : db.executed_ddl)
        EXPECT_EQ(s.find(';'), std::string::npos) << s;
}

TEST_F(MigrateSqlite, CreatesATableFromTheDeclaration)
{
    const entity_meta e = orm::entity_meta_of<&Note::id, &Note::body>();
    EXPECT_EQ(mig.sync(one(e)), 0);
    EXPECT_TRUE(has_column("notes", "body"));
    EXPECT_EQ(mig.state(), migration_state::Done);
}

TEST_F(MigrateSqlite, SecondRunFindsNothingToDo)
{
    const entity_meta e = orm::entity_meta_of<&Note::id, &Note::body>();
    EXPECT_EQ(mig.sync(one(e)), 0);
    EXPECT_TRUE(mig.diff(orm::connector_trait<orm::SQLiteDB>::read_schema(conn), one(e)).empty());
}

TEST_F(MigrateSqlite, AddsAColumnNamedAfterAKeyword)
{
    exec("CREATE TABLE t (id INTEGER)");
    const entity_meta e{"t", {{"id", "INTEGER"}, {"order", "TEXT"}}};
    EXPECT_EQ(mig.sync(one(e)), 0);
    EXPECT_TRUE(has_column("t", "order"));
}

TEST_F(MigrateSqlite, RefusesToDropAColumnUnlessToldTo)
{
    exec("CREATE TABLE t (id INTEGER, old TEXT)");
    const entity_meta e{"t", {{"id", "INTEGER"}}};
    EXPECT_THROW((void)mig.sync(one(e)), migration_refused);
    EXPECT_TRUE(has_column("t", "old"));
    EXPECT_EQ(mig.sync(one(e), destructive_ops::allow), 0);
    EXPECT_FALSE(has_column("t", "old"));
}

// The second order case. A column with a hostile name exists in the database, made by
// someone with far fewer rights than the migration. Reading it back must not run it.
TEST_F(MigrateSqlite, HostileLiveColumnIsRefusedAndRunsNothing)
{
    exec("CREATE TABLE secrets (v TEXT)");
    exec("CREATE TABLE t (id INTEGER, \"x; DROP TABLE secrets; --\" TEXT)");
    const entity_meta e{"t", {{"id", "INTEGER"}}};
    EXPECT_THROW((void)mig.sync(one(e), destructive_ops::allow), migration_refused);
    EXPECT_TRUE(has_table("secrets"));
    EXPECT_TRUE(has_column("t", "x; DROP TABLE secrets; --"));
    EXPECT_EQ(mig.state(), migration_state::Failed);
}

// The connector's own rule, without the checks in front of it: one call, one statement.
TEST_F(MigrateSqlite, ExecuteDdlRefusesASecondStatement)
{
    exec("CREATE TABLE secrets (v TEXT)");
    EXPECT_THROW(orm::connector_trait<orm::SQLiteDB>::execute_ddl(
                     conn, "CREATE TABLE a (b TEXT); DROP TABLE secrets"),
                 std::runtime_error);
    EXPECT_TRUE(has_table("secrets"));
    EXPECT_FALSE(has_table("a"));
}

// The checks pass and the server refuses the second statement, a duplicate column, after
// the first has run. Nothing of the migration may remain.
TEST_F(MigrateSqlite, AFailureOnTheServerRollsTheWholeMigrationBack)
{
    exec("CREATE TABLE t (id INTEGER)");
    const std::vector<entity_meta> entities{
        {"t", {{"id", "INTEGER"}, {"added", "TEXT"}}},
        {"u", {{"a", "TEXT"}, {"a", "TEXT"}}},
    };
    EXPECT_THROW((void)mig.sync(entities), std::runtime_error);
    EXPECT_FALSE(has_column("t", "added"));
    EXPECT_FALSE(has_table("u"));
    EXPECT_EQ(mig.state(), migration_state::Failed);
}

TEST_F(MigrateSqlite, ChangingATypeIsRefusedHere)
{
    exec("CREATE TABLE t (id INTEGER)");
    const entity_meta e{"t", {{"id", "TEXT"}}};
    EXPECT_THROW((void)mig.sync(one(e)), migration_refused);
}
