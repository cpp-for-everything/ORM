// Differential test on MongoDB. See differential_common.hpp for the outcomes and
// the failure rule, and differential_sqlite.cpp for the idea.
//
// The corpus mixes payloads written for different injection contexts: whole JSON
// filters, operator fragments, and JavaScript for $where. No single naive
// construction fits all of them. The naive path therefore builds the filter in
// five ways that applications really use, runs the payload through each, and
// keeps the most severe outcome. The summary says which construction produced
// each naive compromise.
//
//   json_string   {"name": "<payload>"}            text concatenation, parsed with
//                                                  bson_new_from_json
//   json_value    {"name": <payload>}              the same, payload in value position
//   json_filter   <payload>                        the request body is the filter
//   where_string  {"$where": "this.name == '<payload>'"}   JavaScript concatenation
//   where_whole   {"$where": "<payload>"}          the expression is the payload
//
// orm  select(id, name).where(name == Placeholder<std::string>{}) with the payload
//      supplied to execute().
//
// Every run starts from freshly dropped collections: sec_items with three
// documents and sec_users holding one credential. The snapshot covers both
// collections in canonical extended JSON and the list of sec_ collections. The
// naive find carries maxTimeMS, so a busy loop in $where cannot hang CI.
//
// Limit of this setup: the documents carry only id and name. Payloads written for
// a schema with username and password fields match only through the absence of
// those fields, so the naive compromise count is a lower bound.
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/MongoDB/mongodb_live.hpp"
#include "differential_common.hpp"
#include <algorithm>
#include <cstdlib>
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

    constexpr int kMaxTimeExpired = 50;   // server error code MaxTimeMSExpired

    struct Bson
    {
        bson_t* b = nullptr;
        Bson() = default;
        explicit Bson(bson_t* p) : b(p) {}
        Bson(Bson&& o) noexcept : b(o.b) { o.b = nullptr; }
        Bson(const Bson&)            = delete;
        Bson& operator=(const Bson&) = delete;
        ~Bson() { if (b) bson_destroy(b); }
    };

    std::string db_name() { return env_or("ORM_MONGODB_DB", "orm_test"); }

    mongoc_collection_t* coll(mongoc_client_t* c, const char* name)
    {
        return mongoc_client_get_collection(c, db_name().c_str(), name);
    }

    void insert_json(mongoc_collection_t* c, const char* json)
    {
        bson_error_t err;
        Bson d{bson_new_from_json(reinterpret_cast<const uint8_t*>(json), -1, &err)};
        if (d.b) mongoc_collection_insert_one(c, d.b, nullptr, nullptr, &err);
    }

    void fresh(mongoc_client_t* cl)
    {
        bson_error_t err;
        for (const char* n : {"sec_items", "sec_users"})
        {
            mongoc_collection_t* c = coll(cl, n);
            mongoc_collection_drop(c, &err);
            mongoc_collection_destroy(c);
        }
        mongoc_collection_t* items = coll(cl, "sec_items");
        insert_json(items, R"({"_id":1,"id":1,"name":"apple"})");
        insert_json(items, R"({"_id":2,"id":2,"name":"banana"})");
        insert_json(items, R"({"_id":3,"id":3,"name":"cherry"})");
        mongoc_collection_destroy(items);
        mongoc_collection_t* users = coll(cl, "sec_users");
        insert_json(users, R"({"_id":1,"username":"admin","password":"correct-horse"})");
        mongoc_collection_destroy(users);
    }

    std::string dump(mongoc_client_t* cl, const char* name)
    {
        mongoc_collection_t* c = coll(cl, name);
        Bson filter{bson_new()};
        Bson opts{BCON_NEW("sort", "{", "_id", BCON_INT32(1), "}")};
        mongoc_cursor_t* cur = mongoc_collection_find_with_opts(c, filter.b, opts.b, nullptr);
        std::string out;
        const bson_t* d;
        while (mongoc_cursor_next(cur, &d))
        {
            char* s = bson_as_canonical_extended_json(d, nullptr);
            out += s;
            out += ";";
            bson_free(s);
        }
        mongoc_cursor_destroy(cur);
        mongoc_collection_destroy(c);
        return out;
    }

    struct Snapshot
    {
        std::string items, users, names;
        bool operator==(const Snapshot&) const = default;
    };

    Snapshot snapshot(mongoc_client_t* cl)
    {
        Snapshot s{dump(cl, "sec_items"), dump(cl, "sec_users"), {}};
        bson_error_t err;
        mongoc_database_t* d = mongoc_client_get_database(cl, db_name().c_str());
        if (char** names = mongoc_database_get_collection_names_with_opts(d, nullptr, &err))
        {
            std::vector<std::string> v;
            for (char** p = names; *p; ++p)
                if (std::string(*p).rfind("sec_", 0) == 0) v.emplace_back(*p);
            bson_strfreev(names);
            std::sort(v.begin(), v.end());
            for (const auto& n : v) s.names += n + ";";
        }
        mongoc_database_destroy(d);
        return s;
    }

    // Runs one filter against sec_items with a server side time limit.
    // `filter` null means the construction did not even parse.
    Run find_with(mongoc_client_t* cl, const bson_t* filter, const Snapshot& before)
    {
        if (!filter) return classify(false, false, 0, true);
        mongoc_collection_t* c = coll(cl, "sec_items");
        Bson opts{BCON_NEW("maxTimeMS", BCON_INT64(3000))};
        const Stopwatch sw;
        mongoc_cursor_t* cur = mongoc_collection_find_with_opts(c, filter, opts.b, nullptr);
        long rows = 0;
        const bson_t* d;
        while (mongoc_cursor_next(cur, &d)) ++rows;
        bson_error_t err;
        bool errored = false, tripped = false;
        if (mongoc_cursor_error(cur, &err))
        {
            errored = true;
            if (err.code == kMaxTimeExpired) tripped = true;
        }
        mongoc_cursor_destroy(cur);
        mongoc_collection_destroy(c);
        if (sw.ms() >= kExecGuardMs) tripped = true;
        return classify(tripped, !(before == snapshot(cl)), rows, errored);
    }

    Bson parse(const std::string& json)
    {
        bson_error_t err;
        return Bson{bson_new_from_json(reinterpret_cast<const uint8_t*>(json.data()),
                                       static_cast<ssize_t>(json.size()), &err)};
    }

    Bson where_doc(const std::string& js)
    {
        Bson d{bson_new()};
        bson_append_utf8(d.b, "$where", -1, js.c_str(), static_cast<int>(js.size()));
        return d;
    }

    Run run_naive(mongoc_client_t* cl, const std::string& payload)
    {
        struct Shape
        {
            const char* name;
            Bson (*build)(const std::string&);
        };
        static const Shape shapes[] = {
            {"json_string",  [](const std::string& p) { return parse("{\"name\": \"" + p + "\"}"); }},
            {"json_value",   [](const std::string& p) { return parse("{\"name\": " + p + "}"); }},
            {"json_filter",  [](const std::string& p) { return parse(p); }},
            {"where_string", [](const std::string& p) { return where_doc("this.name == '" + p + "'"); }},
            {"where_whole",  [](const std::string& p) { return where_doc(p); }},
        };
        Run worst;
        worst.note.clear();
        for (const auto& s : shapes)
        {
            fresh(cl);
            const Snapshot before = snapshot(cl);
            const Bson f = s.build(payload);
            Run r = find_with(cl, f.b, before);
            if (severity(r.outcome) > severity(worst.outcome))
            {
                worst = r;
                worst.note = s.name;
            }
        }
        return worst;
    }

    Run run_library(mongoc_client_t* cl, orm::db<orm::MongoDBLive>& db, const std::string& payload)
    {
        fresh(cl);
        const Snapshot before = snapshot(cl);
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
        return classify(tripped, !(before == snapshot(cl)), rows, errored);
    }

    std::string server_version(mongoc_client_t* cl)
    {
        bson_error_t err;
        Bson cmd{BCON_NEW("buildInfo", BCON_INT32(1))};
        bson_t reply;
        std::string v = "unknown";
        if (mongoc_client_command_simple(cl, "admin", cmd.b, nullptr, &reply, &err))
        {
            bson_iter_t it;
            if (bson_iter_init_find(&it, &reply, "version") && BSON_ITER_HOLDS_UTF8(&it))
                v = bson_iter_utf8(&it, nullptr);
        }
        bson_destroy(&reply);
        return v;
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        std::string uri = "mongodb://";
        const char* user = std::getenv("ORM_MONGODB_USER");
        const char* pass = std::getenv("ORM_MONGODB_PASSWORD");
        if (user && pass) uri += std::string(user) + ":" + pass + "@";
        uri += std::string(env_or("ORM_MONGODB_HOST", "127.0.0.1")) + ":" + env_or("ORM_MONGODB_PORT", "27017");

        orm::MongoDBLive lib = orm::MongoDBLive::connect(uri.c_str(), db_name().c_str());
        orm::db<orm::MongoDBLive> db{lib};
        mongoc_client_t* cl = lib.native();

        Backend b;
        b.name       = "mongodb";
        b.server     = "MongoDB " + server_version(cl);
        b.naive_desc = "filter built by concatenation and parsed with bson_new_from_json, or a $where expression; "
                       "five constructions, the most severe outcome is kept";
        b.applies    = [](const Payload& p) { return p.cls == "nosql"; };
        b.naive      = [&](const std::string& p) { return run_naive(cl, p); };
        b.library    = [&](const std::string& p) { return run_library(cl, db, p); };
        return run_differential(argc, argv, b);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
