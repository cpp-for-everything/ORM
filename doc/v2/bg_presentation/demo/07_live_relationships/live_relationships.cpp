// ============================================================================
//  ДЕМО 7 — Една заявка, четири живи хранилища (relationship → JOIN / $lookup)
// ----------------------------------------------------------------------------
//  ЕДНИЯТ И СЪЩ модел и ЕДНАТА И СЪЩА заявка:
//
//      struct Book { ... relationship<reference<&Author::id>, "author_id"> ...; };
//      orm::select(orm::field<&Book::title>, orm::field<&Author::name>)
//
//  се изпълнява срещу РЕАЛНА база. Конекторът превежда изведения при компилация
//  JOIN към диалекта на хранилището — SQL `INNER JOIN` за релационните бази,
//  `$lookup` агрегация за MongoDB — и хидратира резултата в един и същ
//  `joined_row<Book, Author>` с частични обекти.
//
//  Backend-ът се избира при компилация:
//    -DORM_DEMO_SQLITE | -DORM_DEMO_POSTGRESQL | -DORM_DEMO_MYSQL | -DORM_DEMO_MONGODB
//
//  Стартира се "out of the box" през docker-compose (виж run_live.sh) — без
//  локални инсталации, идентично поведение на macOS / Linux / Windows.
// ============================================================================
#include "ORM/ORM.hpp"
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

// ── Един модел, споделен от всички backend-и ────────────────────────────────
struct Author
{
    orm::property<int, "id">           id;
    orm::property<std::string, "name"> name;
};
struct Book
{
    orm::property<int, "id">            id;
    orm::property<std::string, "title"> title;
    orm::relationship<orm::store_as::reference<&Author::id>, "author_id"> author_id;  // FK → Author
};
template <> struct orm::table_name_trait<Author> { static constexpr std::string_view value = "authors"; };
template <> struct orm::table_name_trait<Book>   { static constexpr std::string_view value = "books"; };

[[nodiscard]] static const char* env_or(const char* k, const char* d)
{
    const char* v = std::getenv(k);
    return (v && *v) ? v : d;
}

// ── Backend-специфична настройка (връзка + схема + данни) ────────────────────
#if defined(ORM_DEMO_SQLITE)
#include "ORM/db/connectors/SQLite/sqlite_db.hpp"
static constexpr const char* kBackend  = "SQLite  (вграден, in-process)";
static constexpr const char* kStrategy = "SQL: ... INNER JOIN authors ON books.author_id = authors.id";
using DemoDB = orm::SQLiteDB;
static DemoDB connect_and_seed()
{
    DemoDB db = DemoDB::open(":memory:");
    auto x = [&](const char* s){ sqlite3_exec(db.handle, s, nullptr, nullptr, nullptr); };
    x("CREATE TABLE authors (id INTEGER PRIMARY KEY, name TEXT)");
    x("CREATE TABLE books (id INTEGER PRIMARY KEY, title TEXT, author_id INTEGER)");
    x("INSERT INTO authors VALUES (1,'Tolkien'),(2,'Le Guin')");
    x("INSERT INTO books   VALUES (10,'The Hobbit',1),(11,'A Wizard of Earthsea',2)");
    return db;
}

#elif defined(ORM_DEMO_POSTGRESQL)
#include "ORM/db/connectors/PostgreSQLDB/postgresql_live.hpp"
static constexpr const char* kBackend  = "PostgreSQL  (клиент-сървър)";
static constexpr const char* kStrategy = "SQL: ... INNER JOIN authors ON books.author_id = authors.id";
using DemoDB = orm::PostgreSQLLiveDB;
static DemoDB connect_and_seed()
{
    DemoDB db = DemoDB::connect(("host=" + std::string(env_or("ORM_POSTGRESQL_HOST", "127.0.0.1")) +
        " port=" + env_or("ORM_POSTGRESQL_PORT", "5432") +
        " user=" + env_or("ORM_POSTGRESQL_USER", "postgres") +
        " password=" + env_or("ORM_POSTGRESQL_PASSWORD", "") +
        " dbname=" + env_or("ORM_POSTGRESQL_DB", "postgres")).c_str());
    auto x = [&](const char* s){ PGresult* r = PQexec(db.native(), s); PQclear(r); };
    x("DROP TABLE IF EXISTS books"); x("DROP TABLE IF EXISTS authors");
    x("CREATE TABLE authors (id INTEGER PRIMARY KEY, name TEXT)");
    x("CREATE TABLE books (id INTEGER PRIMARY KEY, title TEXT, author_id INTEGER)");
    x("INSERT INTO authors VALUES (1,'Tolkien'),(2,'Le Guin')");
    x("INSERT INTO books   VALUES (10,'The Hobbit',1),(11,'A Wizard of Earthsea',2)");
    return db;
}

#elif defined(ORM_DEMO_MYSQL)
#include "ORM/db/connectors/MySQLDB/mysql_live.hpp"
static constexpr const char* kBackend  = "MySQL / MariaDB  (клиент-сървър)";
static constexpr const char* kStrategy = "SQL: ... INNER JOIN authors ON books.author_id = authors.id";
using DemoDB = orm::MySQLLiveDB;
static DemoDB connect_and_seed()
{
    DemoDB db = DemoDB::connect(
        env_or("ORM_MYSQL_HOST", "127.0.0.1"),
        static_cast<unsigned int>(std::stoul(env_or("ORM_MYSQL_PORT", "3306"))),
        env_or("ORM_MYSQL_USER", "root"),
        env_or("ORM_MYSQL_PASSWORD", "orm_test_password"),
        env_or("ORM_MYSQL_DATABASE", "orm_test"));
    auto x = [&](const char* s){ mysql_query(db.native(), s); };
    x("DROP TABLE IF EXISTS books"); x("DROP TABLE IF EXISTS authors");
    x("CREATE TABLE authors (id INT PRIMARY KEY, name VARCHAR(255))");
    x("CREATE TABLE books (id INT PRIMARY KEY, title VARCHAR(255), author_id INT)");
    x("INSERT INTO authors VALUES (1,'Tolkien'),(2,'Le Guin')");
    x("INSERT INTO books   VALUES (10,'The Hobbit',1),(11,'A Wizard of Earthsea',2)");
    return db;
}

#elif defined(ORM_DEMO_MONGODB)
#include "ORM/db/connectors/MongoDB/mongodb_live.hpp"
static constexpr const char* kBackend  = "MongoDB  (документна, NoSQL)";
static constexpr const char* kStrategy = "Aggregation: $lookup { from: authors, localField: author_id, foreignField: id } + $unwind";
using DemoDB = orm::MongoDBLive;
static DemoDB connect_and_seed()
{
    std::string uri = "mongodb://";
    const char* user = std::getenv("ORM_MONGODB_USER");
    const char* pass = std::getenv("ORM_MONGODB_PASSWORD");
    if (user && pass) uri += std::string(user) + ":" + pass + "@";
    uri += std::string(env_or("ORM_MONGODB_HOST", "127.0.0.1")) + ":" + env_or("ORM_MONGODB_PORT", "27017");
    DemoDB db = DemoDB::connect(uri.c_str(), env_or("ORM_MONGODB_DB", "orm_test"));
    auto drop = [&](const char* c){ mongoc_collection_t* x = mongoc_client_get_collection(db.native(), db.default_db_.c_str(), c); bson_error_t e; mongoc_collection_drop(x, &e); mongoc_collection_destroy(x); };
    auto ins  = [&](const char* c, bson_t* d){ mongoc_collection_t* x = mongoc_client_get_collection(db.native(), db.default_db_.c_str(), c); bson_error_t e; mongoc_collection_insert_one(x, d, nullptr, nullptr, &e); mongoc_collection_destroy(x); bson_destroy(d); };
    drop("books"); drop("authors");
    ins("authors", BCON_NEW("id", BCON_INT32(1), "name", BCON_UTF8("Tolkien")));
    ins("authors", BCON_NEW("id", BCON_INT32(2), "name", BCON_UTF8("Le Guin")));
    ins("books", BCON_NEW("id", BCON_INT32(10), "title", BCON_UTF8("The Hobbit"),           "author_id", BCON_INT32(1)));
    ins("books", BCON_NEW("id", BCON_INT32(11), "title", BCON_UTF8("A Wizard of Earthsea"), "author_id", BCON_INT32(2)));
    return db;
}
#else
#error "Дефинирай един backend: -DORM_DEMO_SQLITE | _POSTGRESQL | _MYSQL | _MONGODB"
#endif

int main()
{
    DemoDB conn = connect_and_seed();
    orm::db<DemoDB> db{conn};

    // ⭐ ЕДНАТА И СЪЩА заявка за всичките четири хранилища:
    constexpr auto q = orm::select(orm::field<&Book::title>, orm::field<&Author::name>);
    auto rows = (db << q).to_vector();

    // Резултатът е joined_row<Book, Author> — частични обекти, независимо от backend-а.
    static_assert(std::is_same_v<decltype(rows)::value_type, orm::joined_row<Book, Author>>);

    std::map<std::string, std::string> by_title;
    for (const auto& r : rows)
        by_title[r.get<Book>().title.value] = r.get<Author>().name.value;

    std::cout << "── " << kBackend << " ──\n";
    std::cout << "   " << kStrategy << "\n";
    for (const auto& [title, author] : by_title)
        std::cout << "   joined_row{ Book.title=\"" << title << "\", Author.name=\"" << author << "\" }\n";
    std::cout << "   → " << rows.size() << " реда, хидратирани в joined_row<Book, Author>\n";
    return 0;
}
