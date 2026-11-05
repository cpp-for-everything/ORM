// Migration differential on SQLite. See migration_differential.hpp.
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/SQLite/sqlite_db.hpp"
#include "migration_differential.hpp"

namespace
{
    std::string text(sqlite3* h, const char* sql)
    {
        sqlite3_stmt* s = nullptr;
        if (sqlite3_prepare_v2(h, sql, -1, &s, nullptr) != SQLITE_OK) return "<gone>";
        std::string v = "<empty>";
        if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_text(s, 0))
            v = reinterpret_cast<const char*>(sqlite3_column_text(s, 0));
        sqlite3_finalize(s);
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
    using namespace orm_security;
    orm::SQLiteDB conn = orm::SQLiteDB::open(":memory:");
    sqlite3* h = conn.handle;

    MigrationStore s;
    s.name    = "sqlite";
    s.server  = std::string("SQLite ") + sqlite3_libversion();
    s.dialect = "sqlite";
    s.reset = [&] {
        const std::string tables = text(h,
            "SELECT group_concat('DROP TABLE \"' || replace(name,'\"','\"\"') || '\";', ' ') "
            "FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'");
        if (tables != "<empty>" && tables != "<gone>")
            sqlite3_exec(h, tables.c_str(), nullptr, nullptr, nullptr);
        sqlite3_exec(h,
            "CREATE TABLE sec_items (id INTEGER, name TEXT);"
            "INSERT INTO sec_items VALUES (1,'apple'),(2,'banana'),(3,'cherry');"
            "CREATE TABLE sec_users (id INTEGER, username TEXT, password TEXT);"
            "INSERT INTO sec_users VALUES (1,'admin','correct-horse');",
            nullptr, nullptr, nullptr);
    };
    s.protected_state = [&] {
        return text(h, "SELECT group_concat(id||'|'||username||'|'||password, ';') FROM sec_users") + "#" +
               text(h, "SELECT group_concat(id||'|'||name, ';') FROM sec_items");
    };
    s.catalogue = [&] {
        return text(h, "SELECT group_concat(name||':'||sql, ';') FROM "
                       "(SELECT name, sql FROM sqlite_master ORDER BY name)");
    };
    s.naive_exec = [&](const std::string& sql) {
        return sqlite3_exec(h, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK;
    };
    s.plant_column = [&](const std::string& name) {
        const std::string sql = "ALTER TABLE sec_items ADD COLUMN " + quoted(name) + " TEXT";
        return sqlite3_exec(h, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK;
    };
    s.migrate = [&](const orm::entity_meta& e) {
        orm::migrate<orm::SQLiteDB> m{conn};
        return m.sync(std::span<const orm::entity_meta>(&e, 1), orm::destructive_ops::allow);
    };
    return run_migration_differential(argc, argv, s);
}
