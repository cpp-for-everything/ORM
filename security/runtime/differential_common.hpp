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
        std::function<bool(const Payload&)>       applies;
        std::function<Run(const std::string&)>    naive;
        std::function<Run(const std::string&)>    library;
    };

    inline int run_differential(int argc, char** argv, const Backend& b)
    {
        if (argc < 4)
        {
            std::fprintf(stderr, "usage: %s <payloads.csv> <out.csv> <summary.txt>\n", argv[0]);
            return 2;
        }
        const auto corpus = load_corpus(argv[1]);

        std::ofstream out(argv[2], std::ios::binary);
        out << "id,dialect,technique,template,naive_outcome,naive_rows,orm_outcome,orm_rows\n";

        std::map<std::string, int> naive_n, orm_n, naive_by_note;
        int applicable = 0, orm_effect = 0, naive_compromised = 0;
        for (const auto& p : corpus)
        {
            if (!b.applies(p)) continue;
            ++applicable;
            const Run n = b.naive(p.payload);
            const Run o = b.library(p.payload);
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
                << o.outcome << ',' << o.rows << '\n';
            std::printf("%-10s %-16s naive=%-18s%s%s orm=%s\n",
                        p.id.c_str(), p.technique.c_str(), n.outcome.c_str(),
                        n.note.empty() ? "" : " via ", n.note.c_str(), o.outcome.c_str());
            std::fflush(stdout);
        }

        std::ofstream sum(argv[3], std::ios::binary);
        sum << "backend: " << b.name << "\n";
        sum << "server: " << b.server << "\n";
        sum << "naive path: " << b.naive_desc << "\n";
        sum << "applicable payloads: " << applicable << "\n";
        sum << "naive compromised: " << naive_compromised << "\n";
        sum << "orm with any effect: " << orm_effect << "\n\n";
        for (const auto& [k, v] : naive_n) sum << "naive " << k << ": " << v << "\n";
        for (const auto& [k, v] : orm_n)   sum << "orm   " << k << ": " << v << "\n";
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
        if (orm_effect != 0)
        {
            std::fprintf(stderr, "FAIL: %d payload(s) had an observable effect through the ORM\n", orm_effect);
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
