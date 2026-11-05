// Migration differential on PostgreSQL. See migration_differential.hpp.
// The naive path uses PQexec, the simple query protocol, which runs stacked statements.
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/PostgreSQLDB/postgresql_live.hpp"
#include "differential_common.hpp"
#include "migration_differential.hpp"
#include <format>

namespace
{
    using namespace orm_security;

    std::string conninfo()
    {
        return std::format("host={} port={} user={} password={} dbname={}",
                           env_or("ORM_POSTGRESQL_HOST", "127.0.0.1"),
                           env_or("ORM_POSTGRESQL_PORT", "5432"),
                           env_or("ORM_POSTGRESQL_USER", "postgres"),
                           env_or("ORM_POSTGRESQL_PASSWORD", ""),
                           env_or("ORM_POSTGRESQL_DATABASE", "postgres"));
    }

    bool exec(PGconn* c, const std::string& sql)
    {
        PGresult* r = PQexec(c, sql.c_str());
        const auto st = PQresultStatus(r);
        PQclear(r);
        return st == PGRES_COMMAND_OK || st == PGRES_TUPLES_OK;
    }

    std::string text(PGconn* c, const char* sql)
    {
        PGresult* r = PQexec(c, sql);
        std::string v = "<gone>";
        if (PQresultStatus(r) == PGRES_TUPLES_OK)
            v = PQntuples(r) > 0 && !PQgetisnull(r, 0, 0) ? PQgetvalue(r, 0, 0) : "<empty>";
        PQclear(r);
        return v;
    }

    std::string quoted(const std::string& name)
    {
        std::string out = "\"";
        for (char c : name) { if (c == '"') out += '"'; out += c; }
        return out + "\"";
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        orm::PostgreSQLLiveDB lib = orm::PostgreSQLLiveDB::connect(conninfo().c_str());
        PGconn* c = lib.native();
        exec(c, "SET client_min_messages = error");
        exec(c, "SET statement_timeout = 6000");

        MigrationStore s;
        s.name    = "postgresql";
        s.server  = "PostgreSQL " + text(c, "SHOW server_version");
        s.dialect = "postgresql";
        s.reset = [&] {
            const std::string drops = text(c,
                "SELECT string_agg('DROP TABLE ' || quote_ident(tablename) || ' CASCADE;', ' ') "
                "FROM pg_tables WHERE schemaname='public'");
            if (drops != "<empty>" && drops != "<gone>") exec(c, drops);
            exec(c, "CREATE TABLE sec_items (id INTEGER, name TEXT)");
            exec(c, "INSERT INTO sec_items VALUES (1,'apple'),(2,'banana'),(3,'cherry')");
            exec(c, "CREATE TABLE sec_users (id INTEGER, username TEXT, password TEXT)");
            exec(c, "INSERT INTO sec_users VALUES (1,'admin','correct-horse')");
        };
        s.protected_state = [&] {
            return text(c, "SELECT string_agg(id||'|'||username||'|'||password, ';' ORDER BY id) FROM sec_users") + "#" +
                   text(c, "SELECT string_agg(id||'|'||name, ';' ORDER BY id) FROM sec_items");
        };
        s.catalogue = [&] {
            return text(c, "SELECT string_agg(table_name||':'||column_name||':'||data_type, ';' "
                           "ORDER BY table_name, ordinal_position) FROM information_schema.columns "
                           "WHERE table_schema='public'");
        };
        s.naive_exec   = [&](const std::string& sql) { return exec(c, sql); };
        s.plant_column = [&](const std::string& name) {
            return exec(c, "ALTER TABLE sec_items ADD COLUMN " + quoted(name) + " TEXT");
        };
        s.migrate = [&](const orm::entity_meta& e) {
            orm::migrate<orm::PostgreSQLLiveDB> m{lib};
            return m.sync(std::span<const orm::entity_meta>(&e, 1), orm::destructive_ops::allow);
        };
        return run_migration_differential(argc, argv, s);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
