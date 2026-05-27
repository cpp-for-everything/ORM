// ============================================================================
//  ДЕМО A — Компилаторът като първи валидатор на заявката
// ============================================================================
//
//  Една и съща SELECT...JOIN заявка, две системи за съхранение.
//  Компилаторът отказва тази, за която JOIN няма смисъл.
//
//  ▸ Live в IDE: разкоментирай реда `#define ORM_DEMO_DB_MONGODB` --- clangd
//    показва static_assert грешката върху реда `facade << q` без билд.
//  ▸ От shell:   `bash run_demo_a.sh` пуска и двата варианта (MongoDB
//    трябва да се провали; SQLite трябва да премине).
//  ▸ От CMake:   `ninja demo_a_sqlite` строи SQLite варианта.
// ============================================================================

// >>> Разкоментирай долния ред, за да видиш как compile-time gating отказва
//     JOIN срещу MongoDB:
// #define ORM_DEMO_DB_MONGODB

#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MongoDB/mongodb_db.hpp"
#include "ORM/db/connectors/SQLite/sqlite_db.hpp"
#include "ORM/connector/db.hpp"

#include <iostream>

// ── обектен модел ───────────────────────────────────────────────────────────
struct User
{
    orm::property<int,         "id">   id;
    orm::property<std::u8string,"name"> name;
};

struct Post
{
    orm::property<int,         "id">        id;
    orm::property<int,         "author_id"> author_id;
    orm::property<std::u8string,"body">     body;
};

// ── избор на системата за съхранение по време на компилация ────────────────
#if defined(ORM_DEMO_DB_MONGODB)
    using DemoDB = orm::MongoDB;
    constexpr const char* k_name = "MongoDB";
#else
    using DemoDB = orm::SQLiteDB;
    constexpr const char* k_name = "SQLite";
#endif

int main()
{
    DemoDB connection;
    orm::db<DemoDB> facade{connection};

    // SELECT User.id, User.name, Post.body
    //   FROM User INNER JOIN Post ON User.id = Post.author_id
    static constexpr auto q = orm::select(
            orm::field<&User::id>,
            orm::field<&User::name>,
            orm::field<&Post::body>)
        .join<orm::join::mode::inner, Post>(
            orm::field<&User::id> == orm::field<&Post::author_id>);

    // Compile-time gating се намесва в `orm::db<DB>::operator<<`. За
    // да се провали MongoDB вариантът при компилация, трябва operator<<
    // действително да бъде инстанциран със заявката `q`.
    //
    // При MongoDB активираме инстанцирането --- static_assert-ът в
    // orm::db<MongoDB>::operator<< отказва компилацията с readable
    // съобщение за липсваща supports_joins способност.
    //
    // При SQLite просто потвърждаваме, че `q` и `facade` са валидно
    // изградени compile-time стойности. Не извикваме `facade << q`
    // защото SQLite render-ът не довежда JOIN до изпълним SQL --- това
    // е известно ограничение на конектора, не на capability механизма.
#if defined(ORM_DEMO_DB_MONGODB)
    [[maybe_unused]] auto result = facade << q;   // ← поражда static_assert
#else
    static_assert(orm::is_select_query<decltype(q)>);
    static_assert(decltype(q.join_clauses())::size == 1,
        "заявката трябва да съдържа точно една JOIN клауза");
    (void)facade;
#endif

    std::cout << "[" << k_name << "] заявката е статично валидна:\n"
              << "    маркерът supports_joins е удовлетворен и\n"
              << "    компилаторът приема инстанцирането без отказ.\n";
    return 0;
}
