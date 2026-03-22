#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MongoDB/mongodb_live.hpp"
#include <cstdlib>
#include <string>

// ── Test entity ───────────────────────────────────────────────────────────────

struct Document
{
    orm::property<std::string, "title">   title;
    orm::property<std::string, "content"> content;
    orm::property<int, "views">           views;
};

namespace orm
{
    template <>
    struct table_name_trait<Document>
    {
        static constexpr std::string_view value = "documents";
    };
} // namespace orm

// ── Connection helper ─────────────────────────────────────────────────────────

[[nodiscard]] static orm::MongoDBLive make_connection()
{
    const char* host     = std::getenv("ORM_MONGODB_HOST");
    const char* port_str = std::getenv("ORM_MONGODB_PORT");
    const char* user     = std::getenv("ORM_MONGODB_USER");
    const char* password = std::getenv("ORM_MONGODB_PASSWORD");
    const char* db       = std::getenv("ORM_MONGODB_DB");

    std::string uri = "mongodb://";
    
    // Add authentication if credentials are provided
    if (user && password)
    {
        uri += user;
        uri += ":";
        uri += password;
        uri += "@";
    }
    
    uri += (host ? host : "127.0.0.1");
    uri += ":";
    uri += (port_str ? port_str : "27017");

    return orm::MongoDBLive::connect(uri.c_str(), db ? db : "orm_test");
}

// ── Fixture ───────────────────────────────────────────────────────────────────

class MongoDBLiveTest : public ::testing::Test
{
protected:
    orm::MongoDBLive conn;
    orm::db<orm::MongoDBLive> dbc{conn};

    void SetUp() override
    {
        conn = make_connection();
        dbc.rebind(conn);

        // Drop the collection to start clean using libmongoc C API
        mongoc_collection_t* coll = mongoc_client_get_collection(
            conn.native(), conn.default_db_.c_str(), "documents");
        bson_error_t error;
        mongoc_collection_drop(coll, &error);
        mongoc_collection_destroy(coll);
    }

    void TearDown() override
    {
        mongoc_collection_t* coll = mongoc_client_get_collection(
            conn.native(), conn.default_db_.c_str(), "documents");
        bson_error_t error;
        mongoc_collection_drop(coll, &error);
        mongoc_collection_destroy(coll);
    }
};

// ── Tests ──────────────────────────────────────────────────────────────────────

TEST_F(MongoDBLiveTest, ConnectionIsOpen)
{
    EXPECT_TRUE(conn.is_open());
}

TEST_F(MongoDBLiveTest, InsertAndSelectReturnsRow)
{
    constexpr auto ins = orm::insert(
        orm::field<&Document::title>,
        orm::field<&Document::content>,
        orm::field<&Document::views>);
    dbc.execute(ins, std::string("hello"), std::string("world"), 42);

    constexpr auto sel = orm::select(
        orm::field<&Document::title>,
        orm::field<&Document::views>);
    const auto rows = (dbc << sel).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), "hello");
    EXPECT_EQ(std::get<1>(rows[0]), 42);
}

TEST_F(MongoDBLiveTest, InsertMultipleDocuments)
{
    constexpr auto ins = orm::insert(
        orm::field<&Document::title>,
        orm::field<&Document::content>,
        orm::field<&Document::views>);
    dbc.execute(ins, std::string("doc1"), std::string("content1"), 10);
    dbc.execute(ins, std::string("doc2"), std::string("content2"), 20);

    constexpr auto sel = orm::select(orm::field<&Document::title>);
    const auto rows = (dbc << sel).to_vector();
    EXPECT_EQ(rows.size(), 2u);
}

TEST_F(MongoDBLiveTest, SelectWithWhereFilter)
{
    constexpr auto ins = orm::insert(
        orm::field<&Document::title>,
        orm::field<&Document::content>,
        orm::field<&Document::views>);
    dbc.execute(ins, std::string("alpha"), std::string("aaa"), 5);
    dbc.execute(ins, std::string("beta"),  std::string("bbb"), 15);

    constexpr auto sel = orm::select(orm::field<&Document::title>)
        .where(orm::field<&Document::views> > orm::Placeholder<int>{});
    const auto rows = dbc.execute(sel, 10).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), "beta");
}

TEST_F(MongoDBLiveTest, DeleteRemovesDocuments)
{
    constexpr auto ins = orm::insert(
        orm::field<&Document::title>,
        orm::field<&Document::content>,
        orm::field<&Document::views>);
    dbc.execute(ins, std::string("to_delete"), std::string("bye"), 1);
    dbc.execute(ins, std::string("keep"),      std::string("hi"),  2);

    constexpr auto del = orm::deleteq<Document>()
        .where(orm::field<&Document::title> == orm::Placeholder<std::string>{});
    dbc.execute(del, std::string("to_delete"));

    constexpr auto sel = orm::select(orm::field<&Document::title>);
    const auto rows = (dbc << sel).to_vector();
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<0>(rows[0]), "keep");
}
