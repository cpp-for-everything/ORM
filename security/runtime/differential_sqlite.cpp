// Differential test on SQLite.
//
// Every applicable payload in the corpus goes down two paths that ask the same
// question of the same database:
//
//   naive  SELECT id, name FROM items WHERE name = '<payload>'
//          built by string concatenation and run with sqlite3_exec, which
//          allows stacked statements: the worst realistic case;
//   orm    select(id, name).where(name == Placeholder<std::string>{})
//          with the payload supplied to execute().
//
// The query has two columns so that two column UNION payloads, the common form
// in the corpus, have a fair chance. The correct answer is always zero rows:
// no item is named after a payload. Each run uses a fresh in memory database
// with the `items` table and a `users` table holding one credential, so that a
// UNION payload has something to steal.
//
// Outcomes, decided from observable effects only:
//
//   COMPROMISED_EXEC   the engine was made to run until the work guard stopped it
//   COMPROMISED_STATE  the database changed: rows, tables or schema
//   COMPROMISED_ROWS   rows came back that the query should not return
//   SYNTAX_ALTERED     the payload broke the statement. It reached the grammar,
//                      but this payload achieved no effect on this query
//   NO_EFFECT          the payload was handled as data
//
// This oracle can only see effects. A blind boolean payload that alters the
// logic but still yields zero rows is recorded as NO_EFFECT or SYNTAX_ALTERED.
// The naive compromise count is therefore a LOWER bound, which biases the
// comparison against the result it is meant to show, not towards it.
//
// The process fails if any ORM outcome is not NO_EFFECT, and also if the naive
// path is never compromised, because then the comparison would prove nothing.
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/SQLite/sqlite_db.hpp"
#include "corpus.hpp"
#include <cstdio>
#include <fstream>
#include <map>
#include <string>

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
        static constexpr std::string_view value = "items";
    };
} // namespace orm

namespace
{
    // The progress callback fires every 1000 virtual machine steps. A normal
    // query on three rows uses a few hundred steps in total.
    constexpr long kWorkLimit = 200000;

    struct Guard
    {
        long ticks   = 0;
        bool tripped = false;
    };

    int on_progress(void* p)
    {
        auto* g = static_cast<Guard*>(p);
        if (++g->ticks > kWorkLimit)
        {
            g->tripped = true;
            return 1;
        }
        return 0;
    }

    std::string text(sqlite3* h, const char* sql)
    {
        sqlite3_stmt* s = nullptr;
        if (sqlite3_prepare_v2(h, sql, -1, &s, nullptr) != SQLITE_OK) return "<error>";
        std::string v = "<empty>";
        if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_text(s, 0))
            v = reinterpret_cast<const char*>(sqlite3_column_text(s, 0));
        sqlite3_finalize(s);
        return v;
    }

    struct Snapshot
    {
        std::string items, users, schema;
        bool operator==(const Snapshot&) const = default;
    };

    Snapshot snapshot(sqlite3* h)
    {
        return {
            text(h, "SELECT group_concat(id||'|'||name, ';') FROM (SELECT * FROM items ORDER BY id)"),
            text(h, "SELECT group_concat(id||'|'||username||'|'||password, ';') FROM (SELECT * FROM users ORDER BY id)"),
            text(h, "SELECT group_concat(type||':'||name||':'||COALESCE(sql,''), ';') FROM (SELECT * FROM sqlite_master ORDER BY name)"),
        };
    }

    orm::SQLiteDB fresh()
    {
        orm::SQLiteDB c = orm::SQLiteDB::open(":memory:");
        sqlite3_exec(c.handle,
            "CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT);"
            "INSERT INTO items VALUES (1,'apple'),(2,'banana'),(3,'cherry');"
            "CREATE TABLE users (id INTEGER PRIMARY KEY, username TEXT, password TEXT);"
            "INSERT INTO users VALUES (1,'admin','correct-horse');",
            nullptr, nullptr, nullptr);
        return c;
    }

    struct Run
    {
        std::string outcome;
        long rows = 0;
    };

    Run classify(int rc, long rows, bool tripped, const Snapshot& before, const Snapshot& after)
    {
        Run r;
        r.rows = rows;
        if (tripped)                 r.outcome = "COMPROMISED_EXEC";
        else if (!(before == after)) r.outcome = "COMPROMISED_STATE";
        else if (rows > 0)           r.outcome = "COMPROMISED_ROWS";
        else if (rc != SQLITE_OK)    r.outcome = "SYNTAX_ALTERED";
        else                         r.outcome = "NO_EFFECT";
        return r;
    }

    int count_row(void* p, int, char**, char**)
    {
        ++*static_cast<long*>(p);
        return 0;
    }

    Run run_naive(const std::string& payload)
    {
        orm::SQLiteDB c = fresh();
        const Snapshot before = snapshot(c.handle);
        Guard g;
        sqlite3_progress_handler(c.handle, 1000, on_progress, &g);
        const std::string sql = "SELECT id, name FROM items WHERE name = '" + payload + "'";
        long rows = 0;
        char* err = nullptr;
        const int rc = sqlite3_exec(c.handle, sql.c_str(), count_row, &rows, &err);
        sqlite3_free(err);
        sqlite3_progress_handler(c.handle, 0, nullptr, nullptr);
        return classify(rc, rows, g.tripped, before, snapshot(c.handle));
    }

    Run run_orm(const std::string& payload)
    {
        orm::SQLiteDB c = fresh();
        orm::db<orm::SQLiteDB> db{c};
        const Snapshot before = snapshot(c.handle);
        Guard g;
        sqlite3_progress_handler(c.handle, 1000, on_progress, &g);
        constexpr auto q = orm::select(orm::field<&Item::id>, orm::field<&Item::name>)
                               .where(orm::field<&Item::name> == orm::Placeholder<std::string>{});
        long rows = 0;
        int rc    = SQLITE_OK;
        try
        {
            rows = static_cast<long>(db.execute(q, payload).to_vector().size());
        }
        catch (const std::exception&)
        {
            rc = SQLITE_ERROR;
        }
        sqlite3_progress_handler(c.handle, 0, nullptr, nullptr);
        return classify(rc, rows, g.tripped, before, snapshot(c.handle));
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::fprintf(stderr, "usage: %s <payloads.csv> <out.csv> <summary.txt>\n", argv[0]);
        return 2;
    }
    const auto corpus = orm_security::load_corpus(argv[1]);

    std::ofstream out(argv[2], std::ios::binary);
    out << "id,dialect,technique,template,naive_outcome,naive_rows,orm_outcome,orm_rows\n";

    std::map<std::string, int> naive_n, orm_n;
    int applicable = 0, orm_effect = 0, naive_compromised = 0;
    for (const auto& p : corpus)
    {
        if (p.cls != "sql" || (p.dialect != "generic" && p.dialect != "sqlite")) continue;
        ++applicable;
        const Run n = run_naive(p.payload);
        const Run o = run_orm(p.payload);
        ++naive_n[n.outcome];
        ++orm_n[o.outcome];
        if (o.outcome != "NO_EFFECT") ++orm_effect;
        if (n.outcome.rfind("COMPROMISED", 0) == 0) ++naive_compromised;
        out << p.id << ',' << p.dialect << ',' << p.technique << ','
            << (p.is_template ? "yes" : "no") << ',' << n.outcome << ',' << n.rows << ','
            << o.outcome << ',' << o.rows << '\n';
        std::printf("%-8s %-16s naive=%-18s orm=%s\n",
                    p.id.c_str(), p.technique.c_str(), n.outcome.c_str(), o.outcome.c_str());
    }

    std::ofstream sum(argv[3], std::ios::binary);
    sum << "backend: sqlite\n";
    sum << "applicable payloads: " << applicable << "\n";
    sum << "naive compromised: " << naive_compromised << "\n";
    sum << "orm with any effect: " << orm_effect << "\n\n";
    for (const auto& [k, v] : naive_n) sum << "naive " << k << ": " << v << "\n";
    for (const auto& [k, v] : orm_n)   sum << "orm   " << k << ": " << v << "\n";

    std::printf("\napplicable %d, naive compromised %d, orm with any effect %d\n",
                applicable, naive_compromised, orm_effect);
    if (applicable == 0)
    {
        std::fprintf(stderr, "FAIL: no applicable payloads in the corpus\n");
        return 1;
    }
    if (naive_compromised == 0)
    {
        std::fprintf(stderr, "FAIL: the naive path was never compromised, so the comparison proves nothing\n");
        return 1;
    }
    if (orm_effect != 0)
    {
        std::fprintf(stderr, "FAIL: %d payload(s) had an observable effect through the ORM\n", orm_effect);
        return 1;
    }
    return 0;
}
