#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MongoDB/mongodb_db.hpp"

// ── Test entity ───────────────────────────────────────────────────────────────

struct Document
{
    orm::property<int, "id">              id;
    orm::property<std::u8string, "name"> name;
    orm::property<int, "age">            age;
};

// ── is_connector concept ──────────────────────────────────────────────────────

TEST(MongoConnector, SatisfiesIsConnector)
{
    static_assert(orm::is_connector<orm::MongoDB>,
        "connector_trait<MongoDB> must satisfy is_connector<MongoDB>");
}

// ── No capability tags ────────────────────────────────────────────────────────

TEST(MongoConnector, NoJoinsCapability)
{
    static_assert(!orm::has_capability<orm::MongoDB, orm::cap::supports_joins>,
        "MongoDB must NOT declare supports_joins");
}

TEST(MongoConnector, NoTransactionsCapability)
{
    static_assert(!orm::has_capability<orm::MongoDB, orm::cap::supports_transactions>,
        "MongoDB must NOT declare supports_transactions");
}

TEST(MongoConnector, NoAggregationCapability)
{
    static_assert(!orm::has_capability<orm::MongoDB, orm::cap::supports_aggregation>,
        "MongoDB must NOT declare supports_aggregation");
}

// ── EQ filter rendering ───────────────────────────────────────────────────────

TEST(MongoConnector, EqFilterRendering)
{
    orm::MongoDB db;
    orm::db<orm::MongoDB> dbc{db};

    const auto q = orm::select(orm::field<&Document::id>)
                       .where(orm::field<&Document::id> == orm::Placeholder<int>{});
    dbc.execute(q, 1);

    EXPECT_NE(db.coll.last_filter.find("$eq"), std::string::npos)
        << "EQ predicate must render as $eq in BSON filter";
    EXPECT_NE(db.coll.last_filter.find("id"), std::string::npos);
}

// ── GT filter rendering ───────────────────────────────────────────────────────

TEST(MongoConnector, GtFilterRendering)
{
    orm::MongoDB db;
    orm::db<orm::MongoDB> dbc{db};

    const auto q = orm::select(orm::field<&Document::id>)
                       .where(orm::field<&Document::age> > orm::Placeholder<int>{});
    dbc.execute(q, 18);

    EXPECT_NE(db.coll.last_filter.find("$gt"), std::string::npos)
        << "GT predicate must render as $gt in BSON filter";
}

// ── AND filter rendering ──────────────────────────────────────────────────────

TEST(MongoConnector, AndFilterRendering)
{
    orm::MongoDB db;
    orm::db<orm::MongoDB> dbc{db};

    const auto q = orm::select(orm::field<&Document::id>)
                       .where(orm::field<&Document::id>  == orm::Placeholder<int>{})
                       .where(orm::field<&Document::age> >  orm::Placeholder<int>{});
    dbc.execute(q, 1, 18);

    EXPECT_NE(db.coll.last_filter.find("$and"), std::string::npos)
        << "Two WHERE predicates must be wrapped in $and";
}

// ── Projection with _id suppression ──────────────────────────────────────────

TEST(MongoConnector, ProjectionSuppressId)
{
    orm::MongoDB db;
    orm::db<orm::MongoDB> dbc{db};

    const auto q = orm::select(orm::field<&Document::id>, orm::field<&Document::name>);
    dbc << q;

    EXPECT_NE(db.coll.last_projection.find("\"id\":1"),   std::string::npos);
    EXPECT_NE(db.coll.last_projection.find("\"name\":1"), std::string::npos);
    EXPECT_NE(db.coll.last_projection.find("\"_id\":0"),  std::string::npos)
        << "Projection must suppress _id when not in select list";
}

// ── Cursor destroy count (RAII) ───────────────────────────────────────────────

TEST(MongoConnector, CursorDestroyCalledOnce)
{
    orm::MongoDB db;
    orm::db<orm::MongoDB> dbc{db};

    EXPECT_EQ(db.coll.cursor_destroy_count, 0);
    dbc << orm::select(orm::field<&Document::id>);
    EXPECT_EQ(db.coll.cursor_destroy_count, 1)
        << "Cursor must be created and destroyed exactly once per execute()";
}
