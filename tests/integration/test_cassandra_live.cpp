#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/CassandraDB/cassandra_live.hpp"
#include <cstdlib>
#include <string>

// ── Test entity ───────────────────────────────────────────────────────────────

struct Event
{
    orm::property<std::string, "id">          id;
    orm::property<std::string, "description"> description;
    orm::property<int, "priority">            priority;
};

namespace orm
{
    template <>
    struct table_name_trait<Event>
    {
        static constexpr std::string_view value = "events";
    };
} // namespace orm

// ── Connection helper ─────────────────────────────────────────────────────────

[[nodiscard]] static orm::CassandraLiveDB make_connection()
{
    const char* contact_points = std::getenv("ORM_CASSANDRA_CONTACT_POINTS");
    const char* port_str       = std::getenv("ORM_CASSANDRA_PORT");
    const char* keyspace       = std::getenv("ORM_CASSANDRA_KEYSPACE");
    return orm::CassandraLiveDB::connect(
        contact_points ? contact_points : "127.0.0.1",
        keyspace       ? keyspace : "orm_test",
        static_cast<unsigned int>(port_str ? std::stoul(port_str) : 9042u));
}

// ── Keyspace and table setup helper ───────────────────────────────────────────

static void exec_raw(CassSession* session, const char* cql)
{
    CassStatement* stmt = cass_statement_new(cql, 0);
    CassFuture* future  = cass_session_execute(session, stmt);
    cass_statement_free(stmt);
    cass_future_wait(future);
    cass_future_free(future);
}

// ── Fixture ───────────────────────────────────────────────────────────────────

class CassandraLiveTest : public ::testing::Test
{
protected:
    orm::CassandraLiveDB conn;
    orm::db<orm::CassandraLiveDB> dbc{conn};

    void SetUp() override
    {
        // Connect without a keyspace first to create it.
        const char* contact_points = std::getenv("ORM_CASSANDRA_CONTACT_POINTS");
        const char* port_str       = std::getenv("ORM_CASSANDRA_PORT");
        const char* keyspace       = std::getenv("ORM_CASSANDRA_KEYSPACE");
        std::string ks = keyspace ? keyspace : "orm_test";

        CassCluster* cluster = cass_cluster_new();
        CassSession* session = cass_session_new();

        cass_cluster_set_contact_points(cluster, contact_points ? contact_points : "127.0.0.1");
        cass_cluster_set_port(cluster, static_cast<int>(
            port_str ? std::stoul(port_str) : 9042u));

        CassFuture* f = cass_session_connect(session, cluster);
        cass_future_wait(f);
        cass_future_free(f);

        exec_raw(session,
            std::format("CREATE KEYSPACE IF NOT EXISTS {} "
                       "WITH replication = {{'class': 'SimpleStrategy', 'replication_factor': 1}}", ks).c_str());

        exec_raw(session,
            std::format("DROP TABLE IF EXISTS {}.events", ks).c_str());

        exec_raw(session,
            std::format("CREATE TABLE {}.events ("
            "  id          TEXT PRIMARY KEY,"
            "  description TEXT,"
            "  priority    INT"
            ")", ks).c_str());

        CassFuture* close_f = cass_session_close(session);
        cass_future_wait(close_f);
        cass_future_free(close_f);
        cass_session_free(session);
        cass_cluster_free(cluster);

        conn = make_connection();
        dbc.rebind(conn);
    }

    void TearDown() override
    {
        const char* keyspace = std::getenv("ORM_CASSANDRA_KEYSPACE");
        std::string ks = keyspace ? keyspace : "orm_test";
        exec_raw(conn.session_, std::format("TRUNCATE {}.events", ks).c_str());
    }
};

// ── Tests ──────────────────────────────────────────────────────────────────────

TEST_F(CassandraLiveTest, ConnectionIsOpen)
{
    EXPECT_TRUE(conn.is_open());
}

TEST_F(CassandraLiveTest, InsertAndSelectById)
{
    constexpr auto ins = orm::insert(
        orm::field<&Event::id>,
        orm::field<&Event::description>,
        orm::field<&Event::priority>);
    dbc.execute(ins, std::string("evt1"), std::string("first event"), 1);

    constexpr auto sel = orm::select(
        orm::field<&Event::id>,
        orm::field<&Event::description>)
        .where(orm::field<&Event::id> == orm::Placeholder<std::string>{});
    const auto rows = dbc.execute(sel, std::string("evt1")).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), "evt1");
    EXPECT_EQ(std::get<1>(rows[0]), "first event");
}

TEST_F(CassandraLiveTest, InsertMultipleRows)
{
    constexpr auto ins = orm::insert(
        orm::field<&Event::id>,
        orm::field<&Event::description>,
        orm::field<&Event::priority>);
    dbc.execute(ins, std::string("e1"), std::string("alpha"), 1);
    dbc.execute(ins, std::string("e2"), std::string("beta"),  2);

    constexpr auto sel = orm::select(orm::field<&Event::id>)
        .where(orm::field<&Event::id> == orm::Placeholder<std::string>{});

    const auto r1 = dbc.execute(sel, std::string("e1")).to_vector();
    const auto r2 = dbc.execute(sel, std::string("e2")).to_vector();
    EXPECT_EQ(r1.size(), 1u);
    EXPECT_EQ(r2.size(), 1u);
}

TEST_F(CassandraLiveTest, DeleteRemovesRow)
{
    constexpr auto ins = orm::insert(
        orm::field<&Event::id>,
        orm::field<&Event::description>,
        orm::field<&Event::priority>);
    dbc.execute(ins, std::string("del_evt"), std::string("to be deleted"), 0);

    constexpr auto del = orm::deleteq<Event>()
        .where(orm::field<&Event::id> == orm::Placeholder<std::string>{});
    dbc.execute(del, std::string("del_evt"));

    constexpr auto sel = orm::select(orm::field<&Event::id>)
        .where(orm::field<&Event::id> == orm::Placeholder<std::string>{});
    const auto rows = dbc.execute(sel, std::string("del_evt")).to_vector();
    EXPECT_TRUE(rows.empty());
}

TEST_F(CassandraLiveTest, SelectPriorityColumn)
{
    constexpr auto ins = orm::insert(
        orm::field<&Event::id>,
        orm::field<&Event::description>,
        orm::field<&Event::priority>);
    dbc.execute(ins, std::string("p_evt"), std::string("prio test"), 42);

    constexpr auto sel = orm::select(
        orm::field<&Event::id>,
        orm::field<&Event::priority>)
        .where(orm::field<&Event::id> == orm::Placeholder<std::string>{});
    const auto rows = dbc.execute(sel, std::string("p_evt")).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<1>(rows[0]), 42);
}
