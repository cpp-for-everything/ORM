// Shared driver for the server differential harnesses.
//
// Each backend supplies three things: which corpus payloads apply to it, a naive
// path and a library path. The driver runs every applicable payload down both,
// writes the same CSV columns as the SQLite harness and a summary file.
//
// Outcome names, decided from observable effects only:
//
//   COMPROMISED_EXEC   the server was made to run past a time or work guard
//   COMPROMISED_STATE  data, schema, roles or configuration changed
//   COMPROMISED_ROWS   rows or replies came back that the query should not return
//   SYNTAX_ALTERED     the payload reached the grammar and broke the statement,
//                      but achieved no effect on this query
//   NO_EFFECT          the payload was handled as data
//
// Failure rule, different from SQLite on purpose. The process fails ONLY if a
// payload had an effect through the library. If the naive path was never
// compromised, the run still passes. The summary then says the comparison is
// inconclusive for that backend. A server that cannot be attacked through the
// naive construction proves nothing about the library, and saying so is the
// honest result.
#pragma once
#include "corpus.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace orm_security
{
    struct Run
    {
        std::string outcome = "NO_EFFECT";
        long        rows    = 0;
        std::string note;   // which naive construction produced the outcome, if several
    };

    // Elapsed wall time above this is taken as forced execution. A normal lookup
    // on a handful of rows takes milliseconds. The corpus time payloads ask for
    // 1 to 10 seconds.
    inline constexpr long kExecGuardMs = 2500;

    struct Stopwatch
    {
        std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
        [[nodiscard]] long ms() const
        {
            return static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0).count());
        }
    };

    // Most severe wins. Used by backends whose naive path has several realistic
    // constructions.
    inline int severity(const std::string& o)
    {
        if (o == "COMPROMISED_EXEC")  return 4;
        if (o == "COMPROMISED_STATE") return 3;
        if (o == "COMPROMISED_ROWS")  return 2;
        if (o == "SYNTAX_ALTERED")    return 1;
        return 0;
    }

    inline Run classify(bool tripped, bool state_changed, long rows, bool errored)
    {
        Run r;
        r.rows = rows;
        if (tripped)            r.outcome = "COMPROMISED_EXEC";
        else if (state_changed) r.outcome = "COMPROMISED_STATE";
        else if (rows > 0)      r.outcome = "COMPROMISED_ROWS";
        else if (errored)       r.outcome = "SYNTAX_ALTERED";
        else                    r.outcome = "NO_EFFECT";
        return r;
    }

    struct Backend
    {
        std::string                               name;
        std::string                               server;       // server version, read from the server
        std::string                               naive_desc;   // how the naive path is built
        std::string                               control;      // benign payload that must return exactly one row on both paths
        std::function<bool(const Payload&)>       applies;
        std::function<Run(const std::string&)>    naive;
        std::function<Run(const std::string&)>    library;
        // The same question through IN. Left empty by a backend without list support.
        std::function<Run(const std::string&)>    library_list;
    };

    // The list the IN path sends: the payload between two values no row holds, so
    // that it sits at a position that only exists at run time.
    inline std::vector<std::string> as_list(const std::string& payload)
    {
        return {"orm-no-such-value-1", payload, "orm-no-such-value-2"};
    }

    inline int run_differential(int argc, char** argv, const Backend& b)
    {
        if (argc < 4)
        {
            std::fprintf(stderr, "usage: %s <payloads.csv> <out.csv> <summary.txt>\n", argv[0]);
            return 2;
        }
        const auto corpus = load_corpus(argv[1]);

        // Control. A benign lookup of a seeded value must find exactly one row on both
        // paths. Otherwise the seed or the query is broken and every NO_EFFECT below
        // would mean nothing.
        long control_naive = -1, control_library = -1, control_list = -1;
        if (!b.control.empty())
        {
            control_naive   = b.naive(b.control).rows;
            control_library = b.library(b.control).rows;
            if (b.library_list) control_list = b.library_list(b.control).rows;
            std::printf("control %s: naive rows %ld, library rows %ld, list rows %ld\n",
                        b.control.c_str(), control_naive, control_library, control_list);
            if (control_naive != 1 || control_library != 1 ||
                (b.library_list && control_list != 1))
            {
                std::ofstream cs(argv[3], std::ios::binary);
                cs << "backend: " << b.name << "\nserver: " << b.server << "\n"
                   << "CONTROL FAILED: a benign lookup of " << b.control << " returned naive rows "
                   << control_naive << ", library rows " << control_library
                   << " and list rows " << control_list << ", expected 1 on each path\n";
                std::fprintf(stderr, "FAIL: control lookup did not return one row on both paths\n");
                return 1;
            }
        }

        std::ofstream out(argv[2], std::ios::binary);
        out << "id,dialect,technique,template,naive_outcome,naive_rows,orm_outcome,orm_rows,"
               "orm_list_outcome,orm_list_rows\n";

        std::map<std::string, int> naive_n, orm_n, list_n, naive_by_note;
        int applicable = 0, orm_effect = 0, list_effect = 0, naive_compromised = 0;
        for (const auto& p : corpus)
        {
            if (!b.applies(p)) continue;
            ++applicable;
            const Run n = b.naive(p.payload);
            const Run o = b.library(p.payload);
            Run l;
            if (b.library_list)
            {
                l = b.library_list(p.payload);
                ++list_n[l.outcome];
                if (l.outcome != "NO_EFFECT") ++list_effect;
            }
            ++naive_n[n.outcome];
            ++orm_n[o.outcome];
            if (o.outcome != "NO_EFFECT") ++orm_effect;
            if (n.outcome.rfind("COMPROMISED", 0) == 0)
            {
                ++naive_compromised;
                if (!n.note.empty()) ++naive_by_note[n.note];
            }
            out << p.id << ',' << p.dialect << ',' << p.technique << ','
                << (p.is_template ? "yes" : "no") << ',' << n.outcome << ',' << n.rows << ','
                << o.outcome << ',' << o.rows << ','
                << (b.library_list ? l.outcome : std::string("-")) << ','
                << (b.library_list ? std::to_string(l.rows) : std::string("-")) << '\n';
            std::printf("%-10s %-16s naive=%-18s%s%s orm=%s list=%s\n",
                        p.id.c_str(), p.technique.c_str(), n.outcome.c_str(),
                        n.note.empty() ? "" : " via ", n.note.c_str(), o.outcome.c_str(),
                        b.library_list ? l.outcome.c_str() : "-");
            std::fflush(stdout);
        }

        std::ofstream sum(argv[3], std::ios::binary);
        sum << "backend: " << b.name << "\n";
        sum << "server: " << b.server << "\n";
        sum << "naive path: " << b.naive_desc << "\n";
        if (!b.control.empty())
            sum << "control (benign lookup of " << b.control << "): naive rows " << control_naive
                << ", library rows " << control_library << ", list rows " << control_list << "\n";
        sum << "applicable payloads: " << applicable << "\n";
        sum << "naive compromised: " << naive_compromised << "\n";
        sum << "orm with any effect: " << orm_effect << "\n";
        if (b.library_list)
            sum << "orm list with any effect: " << list_effect << "\n";
        else
            sum << "orm list: not supported by this connector\n";
        sum << "\n";
        for (const auto& [k, v] : naive_n) sum << "naive " << k << ": " << v << "\n";
        for (const auto& [k, v] : orm_n)   sum << "orm   " << k << ": " << v << "\n";
        for (const auto& [k, v] : list_n)  sum << "list  " << k << ": " << v << "\n";
        for (const auto& [k, v] : naive_by_note) sum << "naive compromised via " << k << ": " << v << "\n";
        if (naive_compromised == 0)
            sum << "\ncomparison inconclusive for this backend: the naive path was never compromised\n";

        std::printf("\nbackend %s: applicable %d, naive compromised %d, orm with any effect %d\n",
                    b.name.c_str(), applicable, naive_compromised, orm_effect);
        if (applicable == 0)
        {
            std::fprintf(stderr, "FAIL: no applicable payloads in the corpus\n");
            return 1;
        }
        if (naive_compromised == 0)
            std::printf("comparison inconclusive for this backend: the naive path was never compromised\n");
        if (orm_effect != 0 || list_effect != 0)
        {
            std::fprintf(stderr, "FAIL: %d payload(s) had an observable effect through the ORM, "
                                 "%d through a list\n", orm_effect, list_effect);
            return 1;
        }
        return 0;
    }

    inline const char* env_or(const char* name, const char* fallback)
    {
        const char* v = std::getenv(name);
        return v ? v : fallback;
    }
} // namespace orm_security
