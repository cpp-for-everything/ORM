#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/Neo4jDB/neo4j_db.hpp"

// ── Test entity ───────────────────────────────────────────────────────────────

struct GraphNode
{
    orm::property<int, "id">              id;
    orm::property<std::u8string, "name"> name;
    orm::property<int, "age">            age;
};

// ── is_connector concept ──────────────────────────────────────────────────────

TEST(Neo4jConnector, SatisfiesIsConnector)
{
    static_assert(orm::is_connector<orm::Neo4jDB>,
        "connector_trait<Neo4jDB> must satisfy is_connector<Neo4jDB>");
}

// ── Capability tags ───────────────────────────────────────────────────────────

TEST(Neo4jConnector, NoAggregationCapability)
{
    static_assert(!orm::has_capability<orm::Neo4jDB, orm::cap::supports_aggregation>,
        "Neo4jDB must NOT declare supports_aggregation");
}

TEST(Neo4jConnector, SupportsTransactions)
{
    static_assert(orm::has_capability<orm::Neo4jDB, orm::cap::supports_transactions>);
}

// ── Simple Cypher SELECT ──────────────────────────────────────────────────────

TEST(Neo4jConnector, SimpleCypherContainsMatchAndReturn)
{
    orm::Neo4jDB db;
    orm::db<orm::Neo4jDB> dbc{db};

    dbc << orm::select(orm::field<&GraphNode::id>, orm::field<&GraphNode::name>);

    EXPECT_NE(db.conn.last_cypher.find("MATCH"),  std::string::npos)
        << "Cypher must contain MATCH clause";
    EXPECT_NE(db.conn.last_cypher.find("RETURN"), std::string::npos)
        << "Cypher must contain RETURN clause";
    EXPECT_NE(db.conn.last_cypher.find("n.id"),   std::string::npos);
    EXPECT_NE(db.conn.last_cypher.find("n.name"), std::string::npos);
}

// ── WHERE clause in Cypher ────────────────────────────────────────────────────

TEST(Neo4jConnector, WhereClauseRendered)
{
    orm::Neo4jDB db;
    orm::db<orm::Neo4jDB> dbc{db};

    const auto q = orm::select(orm::field<&GraphNode::id>)
                       .where(orm::field<&GraphNode::age> == orm::Placeholder<int>{});
    dbc.execute(q, 30);

    EXPECT_NE(db.conn.last_cypher.find("WHERE"), std::string::npos);
    EXPECT_NE(db.conn.last_cypher.find("n.age"), std::string::npos);
    EXPECT_NE(db.conn.last_cypher.find("$p1"),   std::string::npos)
        << "Named parameter $p1 must appear in Cypher string";
}

// ── Named parameters in params map ───────────────────────────────────────────

TEST(Neo4jConnector, NamedParamInMap)
{
    orm::Neo4jDB db;
    orm::db<orm::Neo4jDB> dbc{db};

    const auto q = orm::select(orm::field<&GraphNode::id>)
                       .where(orm::field<&GraphNode::age> == orm::Placeholder<int>{});
    dbc.execute(q, 25);

    ASSERT_TRUE(db.conn.last_params_map.count("p1") > 0)
        << "Parameter map must contain key 'p1'";
    EXPECT_EQ(db.conn.last_params_map.at("p1"), "25");
}

// ── neo4j_close_results (RAII) ────────────────────────────────────────────────

TEST(Neo4jConnector, CloseResultsCalledOnce)
{
    orm::Neo4jDB db;
    orm::db<orm::Neo4jDB> dbc{db};

    EXPECT_EQ(db.conn.close_results_count, 0);
    dbc << orm::select(orm::field<&GraphNode::id>);
    EXPECT_EQ(db.conn.close_results_count, 1)
        << "neo4j_close_results must be called exactly once per execute()";
}

// ── Two params ────────────────────────────────────────────────────────────────

TEST(Neo4jConnector, TwoNamedParams)
{
    orm::Neo4jDB db;
    orm::db<orm::Neo4jDB> dbc{db};

    const auto q = orm::select(orm::field<&GraphNode::id>)
                       .where(orm::field<&GraphNode::id>  == orm::Placeholder<int>{})
                       .where(orm::field<&GraphNode::age> == orm::Placeholder<int>{});
    dbc.execute(q, 1, 20);

    EXPECT_NE(db.conn.last_cypher.find("$p1"), std::string::npos);
    EXPECT_NE(db.conn.last_cypher.find("$p2"), std::string::npos);
    EXPECT_EQ(db.conn.last_params_map.at("p1"), "1");
    EXPECT_EQ(db.conn.last_params_map.at("p2"), "20");
}
