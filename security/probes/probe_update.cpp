// Probe 2: a runtime literal in update().set(field, value), and in a JOIN-free delete.
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/SQLite/sqlite_db.hpp"
#include <cstdio>
#include <string>
struct Item { orm::property<int,"id"> id; orm::property<std::string,"name"> name; };
namespace orm { template <> struct table_name_trait<Item> { static constexpr std::string_view value = "items"; }; }
static std::string name_of(sqlite3* h, int id) {
    sqlite3_stmt* s=nullptr; sqlite3_prepare_v2(h,"SELECT COALESCE(name,'<NULL>') FROM items WHERE id=?",-1,&s,nullptr);
    sqlite3_bind_int(s,1,id); std::string r="<no row>";
    if (sqlite3_step(s)==SQLITE_ROW) r=reinterpret_cast<const char*>(sqlite3_column_text(s,0));
    sqlite3_finalize(s); return r; }
static int count(sqlite3* h){ sqlite3_stmt* s=nullptr; sqlite3_prepare_v2(h,"SELECT COUNT(*) FROM items",-1,&s,nullptr);
    sqlite3_step(s); int n=sqlite3_column_int(s,0); sqlite3_finalize(s); return n; }
int main(int argc, char** argv) {
    orm::SQLiteDB conn = orm::SQLiteDB::open(":memory:");
    orm::db<orm::SQLiteDB> db{conn};
    sqlite3_exec(conn.handle,"CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT);",nullptr,nullptr,nullptr);
    sqlite3_exec(conn.handle,"INSERT INTO items VALUES (1,'apple'),(2,'banana'),(3,'cherry');",nullptr,nullptr,nullptr);
    std::string v = (argc > 99) ? argv[1] : std::string("grape");
    { auto q = orm::update<Item>().set(orm::field<&Item::name>, orm::Placeholder<std::string>{})
                   .where(orm::field<&Item::id> == orm::Placeholder<int>{});
      db.execute(q, v, 1);
      std::printf("U1 set via placeholder 'grape', id=1   -> name now %s\n", name_of(conn.handle,1).c_str()); }
    { auto q = orm::update<Item>().set(orm::field<&Item::name>, v)
                   .where(orm::field<&Item::id> == orm::Placeholder<int>{});
      db.execute(q, 2);
      std::printf("U2 set via literal     'grape', id=2   -> name now %s\n", name_of(conn.handle,2).c_str()); }
    { int target = (argc > 99) ? 0 : 3;
      auto q = orm::deleteq<Item>().where(orm::field<&Item::id> == target);
      db.execute(q);
      std::printf("D1 delete via literal id==3            -> rows left %d (3 means nothing deleted)\n", count(conn.handle)); }
    return 0;
}
