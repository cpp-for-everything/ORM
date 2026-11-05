// Differential test on Neo4j. See differential_common.hpp for the outcomes and the
// failure rule, and differential_sqlite.cpp for the idea.
//
// The corpus holds Cypher in several shapes: a fragment that closes a string, whole
// statements, and procedure calls. No single naive construction fits all of them, so
// the naive path builds the statement in the three ways applications really do, runs
// the payload through each and keeps the most severe outcome. The summary says which
// construction produced each naive compromise.
//
//   string_value  MATCH (n:sec_items) WHERE n.name = '<payload>' RETURN n.id, n.name
//   number_value  MATCH (n:sec_items) WHERE n.id = <payload> RETURN n.id, n.name
//   statement     <payload>           the request body is the statement
//
// orm  select(id, name).where(name == Placeholder<std::string>{}) with the payload
//      supplied to execute(), so it travels as a Bolt parameter.
//
// Every run starts from fresh nodes: three sec_items and one credential node. That
// node also carries the labels User and user, the ones the corpus payloads name,
// because a payload that matches nothing never reaches its LOAD CSV. The snapshot covers every node with its labels and properties, so a
// statement that changes any of them is seen. The server carries a transaction
// timeout, set in docker-compose, so a slow payload cannot hang CI.
//
// A forced outcome is decided from the server's own error code where it gives one.
// ExternalResourceFailed means LOAD CSV ran and tried the attacker's address, which
// is the compromise. SyntaxError means the statement never ran. The address in the
// corpus is private and unreachable from the test network, so nothing leaves.
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/Neo4jDB/neo4j_live.hpp"
#include "differential_common.hpp"
#include "list_semantics.hpp"
#include <algorithm>
#include <format>
#include <functional>
#include <string_view>
#include <vector>

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

    using Connect = std::function<orm::Neo4jLiveDB()>;

    struct Outcome
    {
        long rows     = 0;
        bool errored  = false;
        bool rejected = false;   // SyntaxError: the server refused it, so nothing ran
        bool forced   = false;   // ExternalResourceFailed: the server tried to reach out
    };

    // Runs one statement with no parameters and counts the rows it returns.
    Outcome run_text(neo4j_connection_t* c, const std::string& cypher)
    {
        Outcome o;
        neo4j_result_stream_t* s = neo4j_run(c, cypher.c_str(), neo4j_null);
        if (!s)
        {
            o.errored = true;
            return o;
        }
        while (neo4j_fetch_next(s) != nullptr) ++o.rows;
        if (neo4j_check_failure(s) != 0)
        {
            o.errored = true;
            const auto* d = neo4j_failure_details(s);
            const std::string_view code = d != nullptr && d->code != nullptr ? d->code : "";
            o.rejected = code.find("SyntaxError") != std::string_view::npos;
            o.forced   = code.find("ExternalResourceFailed") != std::string_view::npos;
        }
        neo4j_close_results(s);
        return o;
    }

    std::string first_text(neo4j_connection_t* c, const char* cypher)
    {
        std::string out = "<error>";
        neo4j_result_stream_t* s = neo4j_run(c, cypher, neo4j_null);
        if (!s) return out;
        if (neo4j_result_t* r = neo4j_fetch_next(s))
        {
            std::vector<char> buf(1 << 16);
            neo4j_tostring(neo4j_result_field(r, 0), buf.data(), buf.size());
            out = buf.data();
        }
        if (neo4j_check_failure(s) != 0) out = "<error>";
        neo4j_close_results(s);
        return out;
    }

    // Every node, sorted, so that the order the server returns them in is no change.
    std::string snapshot(neo4j_connection_t* c)
    {
        std::vector<std::string> rows;
        neo4j_result_stream_t* s = neo4j_run(
            c, "MATCH (n) RETURN labels(n), properties(n)", neo4j_null);
        if (!s) return "<error>";
        std::vector<char> a(4096), b(4096);
        while (neo4j_result_t* r = neo4j_fetch_next(s))
        {
            neo4j_tostring(neo4j_result_field(r, 0), a.data(), a.size());
            neo4j_tostring(neo4j_result_field(r, 1), b.data(), b.size());
            rows.push_back(std::string(a.data()) + std::string(b.data()));
        }
        const bool failed = neo4j_check_failure(s) != 0;
        neo4j_close_results(s);
        if (failed) return "<error>";
        std::sort(rows.begin(), rows.end());
        std::string out;
        for (const auto& row : rows) out += row + ";";
        return out;
    }

    void fresh(neo4j_connection_t* c)
    {
        (void)run_text(c, "MATCH (n) DETACH DELETE n");
        (void)run_text(c, "CREATE (:sec_items {id: 1, name: 'apple'}), "
                          "(:sec_items {id: 2, name: 'banana'}), "
                          "(:sec_items {id: 3, name: 'cherry'}), "
                          "(:sec_users:User:user {id: 1, name: 'admin', username: 'admin', "
                          "password: 'correct-horse'})");
    }

    // One naive attempt, on a connection of its own. A payload that blocks the server
    // can outlive the client's wait, and the reply then arrives during the next
    // statement. On a shared connection that stale reply was first read as the next
    // payload's outcome.
    Run run_one(neo4j_connection_t* admin, const Connect& connect, const std::string& cypher)
    {
        orm::Neo4jLiveDB conn = connect();
        fresh(admin);
        const std::string before = snapshot(admin);
        const Stopwatch sw;
        const Outcome o = run_text(conn.conn_, cypher);
        // Elapsed time alone is not execution: a statement the server refused to
        // compile did not run, however long the refusal took.
        const bool tripped = o.forced || (sw.ms() >= kExecGuardMs && !o.rejected);
        return classify(tripped, before != snapshot(admin), o.rows, o.errored);
    }

    Run run_naive(neo4j_connection_t* admin, const Connect& connect, const std::string& p)
    {
        const std::pair<const char*, std::string> forms[] = {
            {"string_value", "MATCH (n:sec_items) WHERE n.name = '" + p + "' RETURN n.id, n.name"},
            {"number_value", "MATCH (n:sec_items) WHERE n.id = " + p + " RETURN n.id, n.name"},
            {"statement", p},
        };
        Run worst;
        for (const auto& [note, cypher] : forms)
        {
            Run r = run_one(admin, connect, cypher);
            if (severity(r.outcome) > severity(worst.outcome))
            {
                worst      = r;
                worst.note = note;
            }
        }
        return worst;
    }

    template <typename Query, typename Value>
    Run run_query(neo4j_connection_t* admin, orm::db<orm::Neo4jLiveDB>& db, const Query& q,
                  const Value& value)
    {
        fresh(admin);
        const std::string before = snapshot(admin);
        long rows = 0;
        bool errored = false;
        const Stopwatch sw;
        try
        {
            rows = static_cast<long>(db.execute(q, value).to_vector().size());
        }
        catch (const std::exception&)
        {
            errored = true;
        }
        const bool tripped = sw.ms() >= kExecGuardMs;
        return classify(tripped, before != snapshot(admin), rows, errored);
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const std::string url = std::format("bolt://{}:{}", env_or("ORM_NEO4J_HOST", "127.0.0.1"),
                                            env_or("ORM_NEO4J_PORT", "7687"));
        const char* user = env_or("ORM_NEO4J_USER", "neo4j");
        const char* pass = env_or("ORM_NEO4J_PASSWORD", "orm_test_password");

        orm::Neo4jLiveDB admin = orm::Neo4jLiveDB::connect(url.c_str(), user, pass);
        const Connect connect = [&] { return orm::Neo4jLiveDB::connect(url.c_str(), user, pass); };
        orm::Neo4jLiveDB lib   = orm::Neo4jLiveDB::connect(url.c_str(), user, pass);
        orm::db<orm::Neo4jLiveDB> db{lib};

        fresh(admin.conn_);
        if (snapshot(admin.conn_) == "<error>")
            throw std::runtime_error("a snapshot query failed, the oracle would be blind");

        constexpr auto scalar = orm::select(orm::field<&Item::id>, orm::field<&Item::name>)
                                    .where(orm::field<&Item::name> == orm::Placeholder<std::string>{});
        constexpr auto list = orm::select(orm::field<&Item::id>, orm::field<&Item::name>)
                                  .where(orm::in(orm::field<&Item::name>, orm::PlaceholderList<std::string>{}));

        check_list_semantics<&Item::id, &Item::name>(db, [&] { fresh(admin.conn_); });
        std::puts("list semantics: passed");

        Backend b;
        b.name       = "neo4j";
        b.control    = "apple";
        b.server     = "Neo4j " + first_text(admin.conn_,
            "CALL dbms.components() YIELD versions RETURN versions[0]");
        b.naive_desc = "string concatenation into Cypher in three constructions: string value, "
                       "number value, whole statement; no Bolt parameters";
        b.applies    = [](const Payload& p) { return p.cls == "cypher"; };
        b.naive      = [&](const std::string& p) {
            if (p == "apple")   // the control asks the ordinary question only
                return run_one(admin.conn_, connect,
                    "MATCH (n:sec_items) WHERE n.name = 'apple' RETURN n.id, n.name");
            return run_naive(admin.conn_, connect, p);
        };
        b.library      = [&](const std::string& p) { return run_query(admin.conn_, db, scalar, p); };
        b.library_list = [&](const std::string& p) {
            return run_query(admin.conn_, db, list, std::vector<std::string>{p});
        };
        return run_differential(argc, argv, b);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
