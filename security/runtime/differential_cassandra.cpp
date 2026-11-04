// Differential test on Cassandra. See differential_common.hpp for the outcomes
// and the failure rule, and differential_sqlite.cpp for the idea.
//
//   naive  SELECT id, name FROM sec_items WHERE id = '<payload>'
//          built by string concatenation and sent as an unprepared statement with
//          no bound values. CQL accepts one statement per request, so stacked
//          statements are not possible here. That is a property of the language,
//          and it limits what a payload can do on the naive path too.
//   orm    select(id, name).where(id == Placeholder<std::string>{}) with the
//          payload supplied to execute() (a bound value).
//
// The lookup is by primary key, because Cassandra does not filter on other
// columns without ALLOW FILTERING or an index. Every payload starts from
// truncated tables: sec_items with three rows and sec_users holding one
// credential. The snapshot covers both tables, the table and column catalogue of
// the keyspace and the keyspace list.
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/CassandraDB/cassandra_live.hpp"
#include "differential_common.hpp"
#include <algorithm>
#include <cstdlib>
#include <vector>

struct Item
{
    orm::property<std::string, "id">   id;
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

    constexpr const char* kKeyspace = "orm_security";

    struct Outcome
    {
        bool                     ok = false;
        std::vector<std::string> rows;   // each row: columns joined with '|'
    };

    // Runs one unprepared statement and reads every column as text.
    Outcome run_cql(CassSession* s, const std::string& cql)
    {
        Outcome o;
        CassStatement* st = cass_statement_new_n(cql.data(), cql.size(), 0);
        CassFuture* f     = cass_session_execute(s, st);
        cass_statement_free(st);
        cass_future_wait(f);
        if (cass_future_error_code(f) == CASS_OK)
        {
            o.ok = true;
            if (const CassResult* res = cass_future_get_result(f))
            {
                CassIterator* it = cass_iterator_from_result(res);
                const std::size_t n = cass_result_column_count(res);
                while (cass_iterator_next(it))
                {
                    const CassRow* row = cass_iterator_get_row(it);
                    std::string line;
                    for (std::size_t c = 0; c < n; ++c)
                    {
                        const char* p = nullptr;
                        std::size_t len = 0;
                        if (cass_value_get_string(cass_row_get_column(row, c), &p, &len) == CASS_OK)
                            line.append(p, len);
                        else
                            line += "<n/a>";
                        line += '|';
                    }
                    o.rows.push_back(std::move(line));
                }
                cass_iterator_free(it);
                cass_result_free(res);
            }
        }
        cass_future_free(f);
        return o;
    }

    std::string joined(CassSession* s, const std::string& cql)
    {
        Outcome o = run_cql(s, cql);
        if (!o.ok) return "<error>";
        std::sort(o.rows.begin(), o.rows.end());
        std::string out;
        for (const auto& r : o.rows) out += r + ";";
        return out;
    }

    struct Snapshot
    {
        std::string items, users, tables, columns, keyspaces;
        bool operator==(const Snapshot&) const = default;
    };

    Snapshot snapshot(CassSession* s)
    {
        const std::string ks = kKeyspace;
        return {
            joined(s, "SELECT id, name FROM " + ks + ".sec_items"),
            joined(s, "SELECT id, username, password FROM " + ks + ".sec_users"),
            joined(s, "SELECT table_name FROM system_schema.tables WHERE keyspace_name = '" + ks + "'"),
            joined(s, "SELECT table_name, column_name, type FROM system_schema.columns WHERE keyspace_name = '" + ks + "'"),
            joined(s, "SELECT keyspace_name FROM system_schema.keyspaces"),
        };
    }

    void ddl(CassSession* s, const std::string& cql) { (void)run_cql(s, cql); }

    void fresh(CassSession* s)
    {
        const std::string ks = kKeyspace;
        ddl(s, "TRUNCATE " + ks + ".sec_items");
        ddl(s, "TRUNCATE " + ks + ".sec_users");
        ddl(s, "INSERT INTO " + ks + ".sec_items (id, name) VALUES ('1', 'apple')");
        ddl(s, "INSERT INTO " + ks + ".sec_items (id, name) VALUES ('2', 'banana')");
        ddl(s, "INSERT INTO " + ks + ".sec_items (id, name) VALUES ('3', 'cherry')");
        ddl(s, "INSERT INTO " + ks + ".sec_users (id, username, password) VALUES ('1', 'admin', 'correct-horse')");
    }

    Run run_naive(CassSession* admin, CassSession* victim, const std::string& payload)
    {
        fresh(admin);
        const Snapshot before = snapshot(admin);
        const std::string cql = "SELECT id, name FROM sec_items WHERE id = '" + payload + "'";
        const Stopwatch sw;
        const Outcome o = run_cql(victim, cql);
        const bool tripped = sw.ms() >= kExecGuardMs;
        return classify(tripped, !(before == snapshot(admin)), static_cast<long>(o.rows.size()), !o.ok);
    }

    Run run_library(CassSession* admin, orm::db<orm::CassandraLiveDB>& db, const std::string& payload)
    {
        fresh(admin);
        const Snapshot before = snapshot(admin);
        constexpr auto q = orm::select(orm::field<&Item::id>, orm::field<&Item::name>)
                               .where(orm::field<&Item::id> == orm::Placeholder<std::string>{});
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

    struct Session
    {
        CassCluster* cluster = cass_cluster_new();
        CassSession* session = cass_session_new();
        Session(const char* points, int port, const char* keyspace)
        {
            cass_cluster_set_contact_points(cluster, points);
            cass_cluster_set_port(cluster, port);
            CassFuture* f = keyspace ? cass_session_connect_keyspace(session, cluster, keyspace)
                                     : cass_session_connect(session, cluster);
            cass_future_wait(f);
            const bool ok = cass_future_error_code(f) == CASS_OK;
            cass_future_free(f);
            if (!ok) throw std::runtime_error("Cassandra connect failed");
        }
        Session(const Session&)            = delete;
        Session& operator=(const Session&) = delete;
        ~Session()
        {
            CassFuture* f = cass_session_close(session);
            cass_future_wait(f);
            cass_future_free(f);
            cass_session_free(session);
            cass_cluster_free(cluster);
        }
    };
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const char* points = env_or("ORM_CASSANDRA_CONTACT_POINTS", "127.0.0.1");
        const int   port   = std::atoi(env_or("ORM_CASSANDRA_PORT", "9042"));

        Session admin{points, port, nullptr};
        const std::string ks = kKeyspace;
        ddl(admin.session, "CREATE KEYSPACE IF NOT EXISTS " + ks +
            " WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
        ddl(admin.session, "CREATE TABLE IF NOT EXISTS " + ks + ".sec_items (id TEXT PRIMARY KEY, name TEXT)");
        ddl(admin.session, "CREATE TABLE IF NOT EXISTS " + ks +
            ".sec_users (id TEXT PRIMARY KEY, username TEXT, password TEXT)");

        Session victim{points, port, kKeyspace};
        orm::CassandraLiveDB lib = orm::CassandraLiveDB::connect(points, kKeyspace, static_cast<unsigned>(port));
        orm::db<orm::CassandraLiveDB> db{lib};

        Backend b;
        b.name       = "cassandra";
        b.server     = "Cassandra " + joined(admin.session, "SELECT release_version FROM system.local");
        b.naive_desc = "CQL built by concatenation and sent as an unprepared statement, one statement per request";
        b.applies    = [](const Payload& p) { return p.cls == "cql"; };
        b.naive      = [&](const std::string& p) { return run_naive(admin.session, victim.session, p); };
        b.library    = [&](const std::string& p) { return run_library(admin.session, db, p); };
        return run_differential(argc, argv, b);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
