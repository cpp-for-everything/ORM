#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MongoDB/mongodb_live.hpp"
#include <cstdlib>
#include <string>
#include <map>

// ── Test entity ───────────────────────────────────────────────────────────────

struct Document
{
    orm::property<std::string, "title">   title;
    orm::property<std::string, "content"> content;
    orm::property<int, "views">           views;
};

// ── related entities for the relationship-aware ($lookup) tests ────────────────
struct Author
{
    orm::property<int, "id">           id;
    orm::property<std::string, "name"> name;
};
struct Book
{
    orm::property<int, "id">            id;
    orm::property<std::string, "title"> title;
    orm::relationship<orm::store_as::reference<&Author::id>, "author_id"> author_id;
};

namespace orm
{
    template <>
    struct table_name_trait<Document>
    {
        static constexpr std::string_view value = "documents";
    };
    template <> struct table_name_trait<Author> { static constexpr std::string_view value = "authors"; };
    template <> struct table_name_trait<Book>   { static constexpr std::string_view value = "books"; };
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

// ── relationship-aware queries (auto-inferred $lookup → joined_row) ────────────

class MongoDBJoinTest : public ::testing::Test
{
protected:
    orm::MongoDBLive conn;
    orm::db<orm::MongoDBLive> dbc{conn};

    void drop(const char* name)
    {
        mongoc_collection_t* c = mongoc_client_get_collection(
            conn.native(), conn.default_db_.c_str(), name);
        bson_error_t e;
        mongoc_collection_drop(c, &e);
        mongoc_collection_destroy(c);
    }

    void insert_doc(const char* coll, bson_t* doc)
    {
        mongoc_collection_t* c = mongoc_client_get_collection(
            conn.native(), conn.default_db_.c_str(), coll);
        bson_error_t e;
        mongoc_collection_insert_one(c, doc, nullptr, nullptr, &e);
        mongoc_collection_destroy(c);
        bson_destroy(doc);
    }

    void SetUp() override
    {
        conn = make_connection();
        dbc.rebind(conn);
        drop("books");
        drop("authors");
        insert_doc("authors", BCON_NEW("id", BCON_INT32(1), "name", BCON_UTF8("Tolkien")));
        insert_doc("authors", BCON_NEW("id", BCON_INT32(2), "name", BCON_UTF8("Le Guin")));
        insert_doc("books", BCON_NEW("id", BCON_INT32(10), "title", BCON_UTF8("The Hobbit"),           "author_id", BCON_INT32(1)));
        insert_doc("books", BCON_NEW("id", BCON_INT32(11), "title", BCON_UTF8("A Wizard of Earthsea"), "author_id", BCON_INT32(2)));
    }

    void TearDown() override { drop("books"); drop("authors"); }
};

TEST_F(MongoDBJoinTest, AutoLookupResultTypeIsJoinedRow)
{
    constexpr auto q = orm::select(orm::field<&Book::title>, orm::field<&Author::name>);
    auto res = dbc << q;
    static_assert(std::is_same_v<decltype(res)::value_type, orm::joined_row<Book, Author>>);
    EXPECT_EQ(res.size(), 2u);
}

TEST_F(MongoDBJoinTest, AutoLookupHydratesPartialEntities)
{
    // book field + author field → $lookup books→authors inferred; each row is a
    // joined_row with partial Book and Author entities.
    constexpr auto q = orm::select(orm::field<&Book::title>, orm::field<&Author::name>);
    auto rows = (dbc << q).to_vector();
    ASSERT_EQ(rows.size(), 2u);

    std::map<std::string, std::string> title_to_author;
    for (const auto& r : rows)
    {
        EXPECT_TRUE(r.get<Book>().title.has_value());
        EXPECT_TRUE(r.get<Author>().name.has_value());
        title_to_author[r.get<Book>().title.value] = r.get<Author>().name.value;
    }
    EXPECT_EQ(title_to_author["The Hobbit"],           "Tolkien");
    EXPECT_EQ(title_to_author["A Wizard of Earthsea"], "Le Guin");
}
