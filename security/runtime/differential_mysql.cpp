// Differential test on MySQL (the MariaDB server of the compose file, through
// MariaDB Connector/C). See differential_common.hpp for the outcomes and the
// failure rule, and differential_sqlite.cpp for the idea.
//
//   naive  SELECT id, name FROM sec_items WHERE name = '<payload>'
//          built by string concatenation and sent on a connection opened with
//          CLIENT_MULTI_STATEMENTS, so stacked statements run: the worst case;
//   orm    select(id, name).where(name == Placeholder<std::string>{})
//          with the payload supplied to execute() (prepared statement). The
//          library connection is switched to multi statement mode as well, so
//          the library gets no help from the client.
//
// Every payload starts from freshly created tables: sec_items with three rows and
// sec_users holding one credential. The snapshot covers both tables, the column
// catalogue, the table list, the account list and the privilege list. Both
// connections carry a statement time limit, so a sleep payload cannot hang CI.
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/MySQLDB/mysql_live.hpp"
#include "differential_common.hpp"
#include <cstdlib>

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

    constexpr const char* kTimeout = "SET SESSION max_statement_time = 6";

    struct Params
    {
        const char*  host = env_or("ORM_MYSQL_HOST", "127.0.0.1");
        unsigned int port = static_cast<unsigned int>(std::strtoul(env_or("ORM_MYSQL_PORT", "3306"), nullptr, 10));
        const char*  user = env_or("ORM_MYSQL_USER", "root");
        const char*  pass = env_or("ORM_MYSQL_PASSWORD", "orm_test_password");
        const char*  db   = env_or("ORM_MYSQL_DATABASE", "orm_test");
    };

    struct MyConn
    {
        MYSQL* c = nullptr;
        explicit MyConn(const Params& p, unsigned long flags)
        {
            c = mysql_init(nullptr);
            if (!c || !mysql_real_connect(c, p.host, p.user, p.pass, p.db, p.port, nullptr, flags))
            {
                const std::string e = c ? mysql_error(c) : "init failed";
                if (c) mysql_close(c);
                throw std::runtime_error("MySQL connection failed: " + e);
            }
        }
        MyConn(const MyConn&)            = delete;
        MyConn& operator=(const MyConn&) = delete;
        ~MyConn() { mysql_close(c); }
    };

    void exec(MYSQL* c, const char* sql)
    {
        if (mysql_query(c, sql) == 0)
            if (MYSQL_RES* r = mysql_store_result(c)) mysql_free_result(r);
    }

    std::string text(MYSQL* c, const char* sql)
    {
        if (mysql_query(c, sql) != 0) return "<error>";
        MYSQL_RES* r = mysql_store_result(c);
        if (!r) return "<error>";
        std::string v = "<empty>";
        if (MYSQL_ROW row = mysql_fetch_row(r))
            if (row[0]) v = row[0];
        mysql_free_result(r);
        return v;
    }

    struct Snapshot
    {
        std::string items, users, columns, tables, accounts, privileges;
        bool operator==(const Snapshot&) const = default;
    };

    Snapshot snapshot(MYSQL* c)
    {
        return {
            text(c, "SELECT COALESCE(GROUP_CONCAT(CONCAT(id,'|',name) ORDER BY id SEPARATOR ';'),'') FROM sec_items"),
            text(c, "SELECT COALESCE(GROUP_CONCAT(CONCAT(id,'|',username,'|',password) ORDER BY id SEPARATOR ';'),'') FROM sec_users"),
            text(c, "SELECT COALESCE(GROUP_CONCAT(CONCAT(table_name,':',column_name,':',column_type) "
                    "ORDER BY table_name, ordinal_position SEPARATOR ';'),'') "
                    "FROM information_schema.columns WHERE table_schema = DATABASE()"),
            text(c, "SELECT COALESCE(GROUP_CONCAT(table_name ORDER BY table_name SEPARATOR ';'),'') "
                    "FROM information_schema.tables WHERE table_schema = DATABASE()"),
            text(c, "SELECT COALESCE(GROUP_CONCAT(CONCAT(User,'@',Host) ORDER BY User, Host SEPARATOR ';'),'') FROM mysql.user"),
            text(c, "SELECT COALESCE(GROUP_CONCAT(CONCAT(grantee,':',privilege_type) ORDER BY grantee, privilege_type SEPARATOR ';'),'') "
                    "FROM information_schema.user_privileges"),
        };
    }

    void fresh(MYSQL* c)
    {
        exec(c, "DROP TABLE IF EXISTS sec_items");
        exec(c, "DROP TABLE IF EXISTS sec_users");
        exec(c, "CREATE TABLE sec_items (id INT PRIMARY KEY, name VARCHAR(255))");
        exec(c, "INSERT INTO sec_items VALUES (1,'apple'),(2,'banana'),(3,'cherry')");
        exec(c, "CREATE TABLE sec_users (id INT PRIMARY KEY, username VARCHAR(255), password VARCHAR(255))");
        exec(c, "INSERT INTO sec_users VALUES (1,'admin','correct-horse')");
    }

    Run run_naive(MYSQL* admin, MYSQL* conn, const std::string& payload)
    {
        fresh(admin);
        const Snapshot before = snapshot(admin);
        const std::string sql = "SELECT id, name FROM sec_items WHERE name = '" + payload + "'";
        long rows = 0;
        bool errored = false;
        const Stopwatch sw;
        if (mysql_real_query(conn, sql.c_str(), sql.size()) != 0)
            errored = true;
        else
        {
            int st = 0;
            do
            {
                if (MYSQL_RES* r = mysql_store_result(conn))
                {
                    rows += static_cast<long>(mysql_num_rows(r));
                    mysql_free_result(r);
                }
                else if (mysql_field_count(conn) != 0)
                    errored = true;
                st = mysql_next_result(conn);
            } while (st == 0);
            if (st > 0) errored = true;
        }
        const bool tripped = sw.ms() >= kExecGuardMs;
        return classify(tripped, !(before == snapshot(admin)), rows, errored);
    }

    Run run_library(MYSQL* admin, orm::db<orm::MySQLLiveDB>& db, const std::string& payload)
    {
        fresh(admin);
        const Snapshot before = snapshot(admin);
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
        return classify(tripped, !(before == snapshot(admin)), rows, errored);
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const Params p;
        MyConn admin{p, 0};
        MyConn naive{p, CLIENT_MULTI_STATEMENTS};
        exec(naive.c, kTimeout);

        orm::MySQLLiveDB lib = orm::MySQLLiveDB::connect(p.host, p.port, p.user, p.pass, p.db);
        mysql_set_server_option(lib.native(), MYSQL_OPTION_MULTI_STATEMENTS_ON);
        exec(lib.native(), kTimeout);
        orm::db<orm::MySQLLiveDB> db{lib};

        Backend b;
        b.name       = "mysql";
        b.server     = std::string("MySQL-compatible server ") + text(admin.c, "SELECT VERSION()");
        b.naive_desc = "string concatenation, mysql_real_query on a CLIENT_MULTI_STATEMENTS connection, stacked statements allowed";
        b.applies    = [](const Payload& pl) {
            return pl.cls == "sql" && (pl.dialect == "generic" || pl.dialect == "mysql");
        };
        b.naive   = [&](const std::string& s) { return run_naive(admin.c, naive.c, s); };
        b.library = [&](const std::string& s) { return run_library(admin.c, db, s); };
        return run_differential(argc, argv, b);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
