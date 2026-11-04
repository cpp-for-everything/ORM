// Differential test on PostgreSQL. See differential_common.hpp for the outcomes
// and the failure rule, and differential_sqlite.cpp for the idea.
//
//   naive  SELECT id, name FROM sec_items WHERE name = '<payload>'
//          built by string concatenation and sent with the simple query protocol
//          (PQsendQuery), which runs stacked statements: the worst realistic case;
//   orm    select(id, name).where(name == Placeholder<std::string>{})
//          with the payload supplied to execute() (PQexecParams, typed parameter).
//
// Every payload starts from freshly created tables: sec_items with three rows and
// sec_users holding one credential, so that a UNION payload has something to
// steal. The snapshot covers both tables, the column catalogue, the table list
// and the role list, so a stacked statement that changes any of them is seen.
// Both connections carry a statement timeout, so a sleep payload cannot hang CI.
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/PostgreSQLDB/postgresql_live.hpp"
#include "differential_common.hpp"
#include <cstdlib>
#include <format>
#include <memory>

struct Item
{
    orm::property<int, "id">           id;
    orm::property<std::string, "name"> name;
};

namespace orm
{
    template <>
    struct table_name_trait<Item>
    {
        static constexpr std::string_view value = "sec_items";
    };
} // namespace orm

namespace
{
    using namespace orm_security;

    constexpr const char* kTimeout = "SET statement_timeout = 6000";

    struct PgConn
    {
        PGconn* c = nullptr;
        explicit PgConn(const std::string& info) : c(PQconnectdb(info.c_str()))
        {
            if (PQstatus(c) != CONNECTION_OK)
            {
                const std::string e = PQerrorMessage(c);
                PQfinish(c);
                throw std::runtime_error("PostgreSQL connection failed: " + e);
            }
        }
        PgConn(const PgConn&)            = delete;
        PgConn& operator=(const PgConn&) = delete;
        ~PgConn() { PQfinish(c); }
    };

    std::string conninfo()
    {
        return std::format("host={} port={} user={} password={} dbname={}",
                           env_or("ORM_POSTGRESQL_HOST", "127.0.0.1"),
                           env_or("ORM_POSTGRESQL_PORT", "5432"),
                           env_or("ORM_POSTGRESQL_USER", "postgres"),
                           env_or("ORM_POSTGRESQL_PASSWORD", ""),
                           env_or("ORM_POSTGRESQL_DATABASE", "postgres"));
    }

    void exec(PGconn* c, const char* sql)
    {
        PGresult* r = PQexec(c, sql);
        PQclear(r);
    }

    std::string text(PGconn* c, const char* sql)
    {
        PGresult* r = PQexec(c, sql);
        std::string v = "<error>";
        if (PQresultStatus(r) == PGRES_TUPLES_OK)
            v = PQntuples(r) > 0 && !PQgetisnull(r, 0, 0) ? PQgetvalue(r, 0, 0) : "<empty>";
        PQclear(r);
        return v;
    }

    struct Snapshot
    {
        std::string items, users, columns, tables, roles;
        bool operator==(const Snapshot&) const = default;
        [[nodiscard]] bool ok() const
        {
            for (const std::string* s : {&items, &users, &columns, &tables, &roles})
                if (*s == "<error>") return false;
            return true;
        }
    };

    Snapshot snapshot(PGconn* c)
    {
        return {
            text(c, "SELECT COALESCE(string_agg(id||'|'||name, ';' ORDER BY id),'') FROM sec_items"),
            text(c, "SELECT COALESCE(string_agg(id||'|'||username||'|'||password, ';' ORDER BY id),'') FROM sec_users"),
            text(c, "SELECT COALESCE(string_agg(table_name||':'||column_name||':'||data_type, ';' "
                    "ORDER BY table_name, ordinal_position),'') FROM information_schema.columns "
                    "WHERE table_schema='public'"),
            text(c, "SELECT COALESCE(string_agg(tablename, ';' ORDER BY tablename),'') "
                    "FROM pg_tables WHERE schemaname='public'"),
            text(c, "SELECT COALESCE(string_agg(rolname, ';' ORDER BY rolname),'') FROM pg_roles"),
        };
    }

    void fresh(PGconn* c)
    {
        exec(c, "DROP TABLE IF EXISTS sec_items");
        exec(c, "DROP TABLE IF EXISTS sec_users");
        exec(c, "DROP TABLE IF EXISTS notsosecure");
        exec(c, "CREATE TABLE sec_items (id INTEGER PRIMARY KEY, name TEXT)");
        exec(c, "INSERT INTO sec_items VALUES (1,'apple'),(2,'banana'),(3,'cherry')");
        exec(c, "CREATE TABLE sec_users (id INTEGER PRIMARY KEY, username TEXT, password TEXT)");
        exec(c, "INSERT INTO sec_users VALUES (1,'admin','correct-horse')");
    }

    Run run_naive(PGconn* admin, PGconn* conn, const std::string& payload)
    {
        fresh(admin);
        const Snapshot before = snapshot(admin);
        const std::string sql = "SELECT id, name FROM sec_items WHERE name = '" + payload + "'";
        long rows = 0;
        bool errored = false;
        const Stopwatch sw;
        if (!PQsendQuery(conn, sql.c_str()))
            errored = true;
        else
            while (PGresult* r = PQgetResult(conn))
            {
                const auto st = PQresultStatus(r);
                if (st == PGRES_TUPLES_OK) rows += PQntuples(r);
                else if (st == PGRES_FATAL_ERROR || st == PGRES_BAD_RESPONSE) errored = true;
                PQclear(r);
            }
        const bool tripped = sw.ms() >= kExecGuardMs;
        return classify(tripped, !(before == snapshot(admin)), rows, errored);
    }

    Run run_library(PGconn* admin, orm::db<orm::PostgreSQLLiveDB>& db, const std::string& payload)
    {
        fresh(admin);
        const Snapshot before = snapshot(admin);
        constexpr auto q = orm::select(orm::field<&Item::id>, orm::field<&Item::name>)
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
        const bool tripped = sw.ms() >= kExecGuardMs;
        return classify(tripped, !(before == snapshot(admin)), rows, errored);
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        PgConn admin{conninfo()};
        exec(admin.c, "SET client_min_messages = warning");
        PgConn naive{conninfo()};
        exec(naive.c, kTimeout);

        orm::PostgreSQLLiveDB lib = orm::PostgreSQLLiveDB::connect(conninfo().c_str());
        orm::db<orm::PostgreSQLLiveDB> db{lib};
        exec(lib.native(), kTimeout);

        fresh(admin.c);
        if (!snapshot(admin.c).ok()) throw std::runtime_error("a snapshot query failed, the oracle would be blind");
        Backend b;
        b.name       = "postgresql";
        b.control    = "apple";
        b.server     = "PostgreSQL " + text(admin.c, "SHOW server_version");
        b.naive_desc = "string concatenation, simple query protocol (PQsendQuery), stacked statements allowed";
        b.applies    = [](const Payload& p) {
            return p.cls == "sql" && (p.dialect == "generic" || p.dialect == "postgresql");
        };
        b.naive   = [&](const std::string& p) { return run_naive(admin.c, naive.c, p); };
        b.library = [&](const std::string& p) { return run_library(admin.c, db, p); };
        return run_differential(argc, argv, b);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
