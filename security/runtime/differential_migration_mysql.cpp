// Migration differential on MariaDB through the MySQL connector. See
// migration_differential.hpp. The naive path uses a CLIENT_MULTI_STATEMENTS connection,
// which runs stacked statements. The library path uses a connection without that flag.
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/MySQLDB/mysql_live.hpp"
#include "differential_common.hpp"
#include "migration_differential.hpp"
#include <cstdlib>

namespace
{
    using namespace orm_security;

    const char* host() { return env_or("ORM_MYSQL_HOST", "127.0.0.1"); }
    unsigned int port() { return static_cast<unsigned int>(std::strtoul(env_or("ORM_MYSQL_PORT", "3306"), nullptr, 10)); }
    const char* user() { return env_or("ORM_MYSQL_USER", "root"); }
    const char* pass() { return env_or("ORM_MYSQL_PASSWORD", "orm_test_password"); }
    const char* base() { return env_or("ORM_MYSQL_DATABASE", "orm_test"); }

    bool exec(MYSQL* c, const std::string& sql)
    {
        bool ok = mysql_real_query(c, sql.c_str(), sql.size()) == 0;
        // Drain every result, so that a stacked statement has run before anyone looks.
        for (;;)
        {
            if (MYSQL_RES* r = mysql_store_result(c)) mysql_free_result(r);
            const int next = mysql_next_result(c);
            if (next > 0) ok = false;
            if (next != 0) break;
        }
        return ok;
    }

    std::string text(MYSQL* c, const char* sql)
    {
        if (mysql_query(c, sql) != 0) return "<gone>";
        MYSQL_RES* r = mysql_store_result(c);
        if (!r) return "<gone>";
        MYSQL_ROW row = mysql_fetch_row(r);
        std::string v = row && row[0] ? row[0] : "<empty>";
        mysql_free_result(r);
        return v;
    }

    std::string quoted(const std::string& name)
    {
        std::string out = "`";
        for (char c : name) { if (c == '`') out += '`'; out += c; }
        return out + "`";
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        MYSQL* c = mysql_init(nullptr);
        if (!mysql_real_connect(c, host(), user(), pass(), base(), port(), nullptr, CLIENT_MULTI_STATEMENTS))
            throw std::runtime_error(mysql_error(c));
        exec(c, "SET SESSION max_statement_time = 6");
        exec(c, "SET SESSION group_concat_max_len = 1000000");

        orm::MySQLLiveDB lib = orm::MySQLLiveDB::connect(host(), port(), user(), pass(), base());

        MigrationStore s;
        s.name    = "mysql";
        s.server  = std::string("MySQL-compatible server ") + mysql_get_server_info(c);
        s.dialect = "mysql";
        s.reset = [&] {
            const std::string drops = text(c,
                "SELECT GROUP_CONCAT(CONCAT('DROP TABLE `', REPLACE(TABLE_NAME,'`','``'), '`;') SEPARATOR ' ') "
                "FROM information_schema.TABLES WHERE TABLE_SCHEMA = DATABASE()");
            if (drops != "<empty>" && drops != "<gone>") exec(c, drops);
            exec(c, "CREATE TABLE sec_items (id INTEGER, name TEXT);"
                    "INSERT INTO sec_items VALUES (1,'apple'),(2,'banana'),(3,'cherry');"
                    "CREATE TABLE sec_users (id INTEGER, username TEXT, password TEXT);"
                    "INSERT INTO sec_users VALUES (1,'admin','correct-horse')");
        };
        s.protected_state = [&] {
            return text(c, "SELECT GROUP_CONCAT(CONCAT(id,'|',username,'|',password) ORDER BY id SEPARATOR ';') FROM sec_users") + "#" +
                   text(c, "SELECT GROUP_CONCAT(CONCAT(id,'|',name) ORDER BY id SEPARATOR ';') FROM sec_items");
        };
        s.catalogue = [&] {
            return text(c, "SELECT GROUP_CONCAT(CONCAT(TABLE_NAME,':',COLUMN_NAME,':',DATA_TYPE) "
                           "ORDER BY TABLE_NAME, ORDINAL_POSITION SEPARATOR ';') "
                           "FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE()");
        };
        s.naive_exec   = [&](const std::string& sql) { return exec(c, sql); };
        s.plant_column = [&](const std::string& name) {
            return exec(c, "ALTER TABLE sec_items ADD COLUMN " + quoted(name) + " TEXT");
        };
        s.migrate = [&](const orm::entity_meta& e) {
            orm::migrate<orm::MySQLLiveDB> m{lib};
            return m.sync(std::span<const orm::entity_meta>(&e, 1), orm::destructive_ops::allow);
        };
        const int rc = run_migration_differential(argc, argv, s);
        mysql_close(c);
        return rc;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
