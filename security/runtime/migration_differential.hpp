// Differential test of the migration path, level 2 of the threat model: the attacker
// controls an identifier, and the statement being built is DDL.
//
// Every applicable input goes through four positions, on two paths:
//
//   table        the name of a table the migration creates
//   column       the name of a column the migration adds
//   type         the type of a column the migration adds
//   live_column  SECOND ORDER. The input is first planted as the name of a real
//                column in the live database, the way anyone with CREATE rights could.
//                The migration then reads it back and drops it, with its own rights.
//
//   naive  the DDL as this library built it before the checks existed: plain string
//          concatenation, run with a call that accepts stacked statements.
//   orm    orm::migrate<DB>, with DROP COLUMN allowed, so that the identifier check
//          alone stands between the input and the statement.
//
// Inputs are the SQL payloads of corpus/payloads.csv, which have a published source,
// and corpus/identifier_cases.csv, which were CONSTRUCTED for this test: the published
// payloads target value positions, and identifier position has no comparable corpus.
// The constructed cases are never counted as corpus payloads.
//
// Outcomes:
//
//   COMPROMISED_STATE  sec_users or the rows of sec_items changed
//   SYNTAX_ALTERED     the input reached the grammar and the statement failed
//   NO_EFFECT          nothing protected changed and nothing failed
//   REFUSED            the library refused it, and the whole catalogue is unchanged
//   SKIPPED            the server would not accept the input as a real column name,
//                      so the second order case cannot be set up
//
// The process fails if the library path ever changes protected data, or applies an
// input that is not a plain identifier, or if the control below does not hold. If the
// naive path is never compromised the run passes and says the comparison is
// inconclusive, like the server harnesses.
#pragma once
#include "ORM/db/migration/migration.hpp"
#include "corpus.hpp"
#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace orm_security
{
    struct MigrationStore
    {
        std::string name;
        std::string server;
        std::string dialect;                                     // corpus dialect, for applicability
        std::function<void()>                       reset;       // fresh sec_items and sec_users
        std::function<std::string()>                protected_state;   // sec_users and sec_items rows
        std::function<std::string()>                catalogue;   // every table and column
        std::function<bool(const std::string&)>     naive_exec;  // stacked statements allowed
        std::function<bool(const std::string&)>     plant_column;// add a column of exactly this name
        std::function<int(const orm::entity_meta&)> migrate;     // orm::migrate<DB>::sync, drops allowed
    };

    struct MigrationRun
    {
        std::string outcome;
    };

    inline const char* const kPositions[] = {"table", "column", "type", "live_column"};

    inline orm::entity_meta items_with(std::vector<std::pair<std::string, std::string>> extra)
    {
        orm::entity_meta m{"sec_items", {{"id", "INTEGER"}, {"name", "TEXT"}}};
        for (auto& c : extra) m.columns.push_back(std::move(c));
        return m;
    }

    inline std::string naive_ddl(const std::string& position, const std::string& p)
    {
        if (position == "table")  return "CREATE TABLE " + p + " (id INTEGER, name TEXT)";
        if (position == "column") return "ALTER TABLE sec_items ADD COLUMN " + p + " TEXT";
        if (position == "type")   return "ALTER TABLE sec_items ADD COLUMN extra " + p;
        return "ALTER TABLE sec_items DROP COLUMN " + p;
    }

    inline orm::entity_meta library_entity(const std::string& position, const std::string& p)
    {
        if (position == "table")  return {p, {{"id", "INTEGER"}, {"name", "TEXT"}}};
        if (position == "column") return items_with({{p, "TEXT"}});
        if (position == "type")   return items_with({{"extra", p}});
        return items_with({});
    }

    inline MigrationRun run_naive(const MigrationStore& s, const std::string& position,
                                  const std::string& p)
    {
        s.reset();
        if (position == "live_column" && !s.plant_column(p)) return {"SKIPPED"};
        const std::string before = s.protected_state();
        const bool ok = s.naive_exec(naive_ddl(position, p));
        if (s.protected_state() != before) return {"COMPROMISED_STATE"};
        return {ok ? "NO_EFFECT" : "SYNTAX_ALTERED"};
    }

    inline MigrationRun run_library(const MigrationStore& s, const std::string& position,
                                    const std::string& p, bool& unsafe)
    {
        s.reset();
        if (position == "live_column" && !s.plant_column(p)) return {"SKIPPED"};
        const std::string before    = s.protected_state();
        const std::string catalogue = s.catalogue();
        bool refused = false;
        try
        {
            (void)s.migrate(library_entity(position, p));
        }
        catch (const orm::migration_refused&)
        {
            refused = true;
        }
        catch (const std::exception&)
        {
            // The server rejected a statement that passed the checks. Reported below
            // as a change or not, like any other outcome.
        }
        if (s.protected_state() != before) { unsafe = true; return {"COMPROMISED_STATE"}; }
        if (refused)
        {
            if (s.catalogue() != catalogue) { unsafe = true; return {"COMPROMISED_STATE"}; }
            return {"REFUSED"};
        }
        // Not refused: acceptable only if the input really was a plain name or type.
        const bool plain = position == "type"
            ? orm::ddl::upper(p) == p && orm::ddl::is_varchar(p)
            : orm::ddl::is_safe_identifier(p);
        if (!plain) unsafe = true;
        return {plain ? "NO_EFFECT" : "COMPROMISED_STATE"};
    }

    // The migration must still do its job: add a column, create a table, handle a name
    // that is a keyword, and find nothing to do the second time.
    inline bool control(const MigrationStore& s, std::string& why)
    {
        try
        {
            s.reset();
            const orm::entity_meta keyword = items_with({{"order", "TEXT"}, {"extra", "INTEGER"}});
            s.migrate(keyword);
            const std::string after = s.catalogue();
            if (after.find("order") == std::string::npos || after.find("extra") == std::string::npos)
                { why = "columns were not added"; return false; }
            s.migrate(keyword);
            if (s.catalogue() != after) { why = "a second run changed the schema"; return false; }
            s.migrate({"sec_created", {{"id", "INTEGER"}, {"note", "VARCHAR(40)"}}});
            if (s.catalogue().find("sec_created") == std::string::npos)
                { why = "the table was not created"; return false; }
            s.naive_exec("DROP TABLE sec_created");
            return true;
        }
        catch (const std::exception& e)
        {
            why = e.what();
            return false;
        }
    }

    inline int run_migration_differential(int argc, char** argv, const MigrationStore& s)
    {
        if (argc < 5)
        {
            std::fprintf(stderr, "usage: %s <payloads.csv> <identifier_cases.csv> <out.csv> <summary.txt>\n", argv[0]);
            return 2;
        }
        std::string why;
        if (!control(s, why))
        {
            std::fprintf(stderr, "FAIL: migration control: %s\n", why.c_str());
            return 1;
        }

        std::vector<Payload> inputs;
        for (const auto& p : load_corpus(argv[1]))
            if (p.cls == "sql" && (p.dialect == "generic" || p.dialect == s.dialect))
                inputs.push_back(p);
        const std::size_t sourced = inputs.size();
        for (const auto& p : load_corpus(argv[2])) inputs.push_back(p);

        std::ofstream out(argv[3], std::ios::binary);
        out << "id,class,position,naive_outcome,orm_outcome\n";

        std::map<std::string, std::map<std::string, int>> naive_n, orm_n;
        int runs = 0, compromised = 0, compromised_constructed = 0;
        bool unsafe = false;
        for (const auto& p : inputs)
            for (const char* position : kPositions)
            {
                const MigrationRun n = run_naive(s, position, p.payload);
                const MigrationRun o = run_library(s, position, p.payload, unsafe);
                ++runs;
                ++naive_n[position][n.outcome];
                ++orm_n[position][o.outcome];
                if (n.outcome == "COMPROMISED_STATE")
                {
                    ++compromised;
                    if (p.cls == "identifier") ++compromised_constructed;
                }
                out << p.id << ',' << p.cls << ',' << position << ',' << n.outcome << ',' << o.outcome << '\n';
                std::printf("%-10s %-11s naive=%-18s orm=%s\n", p.id.c_str(), position,
                            n.outcome.c_str(), o.outcome.c_str());
                std::fflush(stdout);
            }

        std::ofstream sum(argv[4], std::ios::binary);
        sum << "backend: " << s.name << "\nserver: " << s.server << "\n";
        sum << "control (add columns, keyword name, idempotent, create table): passed\n";
        sum << "inputs: " << inputs.size() << " (" << sourced << " corpus, "
            << inputs.size() - sourced << " constructed)\n";
        sum << "runs: " << runs << " (inputs times 4 positions)\n";
        sum << "naive compromised: " << compromised << " (" << compromised_constructed
            << " by constructed inputs)\n";
        sum << "orm unsafe: " << (unsafe ? "yes" : "no") << "\n\n";
        for (const auto& [pos, m] : naive_n)
            for (const auto& [k, v] : m) sum << "naive " << pos << " " << k << ": " << v << "\n";
        for (const auto& [pos, m] : orm_n)
            for (const auto& [k, v] : m) sum << "orm   " << pos << " " << k << ": " << v << "\n";
        if (compromised == 0)
            sum << "\ncomparison inconclusive for this backend: the naive path was never compromised\n";

        std::printf("\nmigration %s: runs %d, naive compromised %d, orm unsafe %s\n",
                    s.name.c_str(), runs, compromised, unsafe ? "yes" : "no");
        if (unsafe)
        {
            std::fprintf(stderr, "FAIL: an input had an effect through the migration path\n");
            return 1;
        }
        return 0;
    }
} // namespace orm_security
