// Probe: what happens to a runtime literal placed directly in a where() clause,
// instead of a typed placeholder? Measured against SQLite in memory.
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/SQLite/sqlite_db.hpp"
#include <cstdio>
#include <string>

struct Item
{
    orm::property<int, "id">           id;
    orm::property<std::string, "name"> name;
};
namespace orm
{
    template <> struct table_name_trait<Item> { static constexpr std::string_view value = "items"; };
}

static int count_rows(sqlite3* h, const char* sql)
{
    sqlite3_stmt* s = nullptr;
    sqlite3_prepare_v2(h, sql, -1, &s, nullptr);
    sqlite3_step(s);
    int n = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
    return n;
}

int main(int argc, char** argv)
{
    orm::SQLiteDB conn = orm::SQLiteDB::open(":memory:");
    orm::db<orm::SQLiteDB> db{conn};
    sqlite3_exec(conn.handle, "CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT);", nullptr, nullptr, nullptr);
    sqlite3_exec(conn.handle, "INSERT INTO items VALUES (1,'apple'),(2,'banana'),(3,'cherry');", nullptr, nullptr, nullptr);

    // Runtime value, so the compiler cannot treat it as a constant.
    std::string benign  = (argc > 99) ? argv[1] : std::string("banana");
    std::string attack  = (argc > 99) ? argv[1] : std::string("x' OR '1'='1");

    // Case A: typed placeholder, the documented path.
    {
        constexpr auto q = orm::select(orm::field<&Item::id>)
                               .where(orm::field<&Item::name> == orm::Placeholder<std::string>{});
        auto rows = db.execute(q, benign).to_vector();
        std::printf("A placeholder, benign  'banana'      -> %zu rows\n", rows.size());
        auto rows2 = db.execute(q, attack).to_vector();
        std::printf("A placeholder, attack  \"x' OR '1'='1\" -> %zu rows\n", rows2.size());
    }
    // Case B: runtime literal embedded directly in the IR, no placeholder.
    {
        auto q = orm::select(orm::field<&Item::id>)
                     .where(orm::field<&Item::name> == benign);
        auto rows = db.execute(q).to_vector();
        std::printf("B literal,     benign  'banana'      -> %zu rows\n", rows.size());
    }
    {
        auto q = orm::select(orm::field<&Item::id>)
                     .where(orm::field<&Item::name> == attack);
        auto rows = db.execute(q).to_vector();
        std::printf("B literal,     attack  \"x' OR '1'='1\" -> %zu rows\n", rows.size());
    }
    std::printf("table still has %d rows\n", count_rows(conn.handle, "SELECT COUNT(*) FROM items"));
    return 0;
}
