// ============================================================================
//  ДЕМО 1 — Един модел, два конектора: компилаторът е първи валидатор
// ----------------------------------------------------------------------------
//  Една и съща SELECT...JOIN заявка (един constexpr модел) се насочва към два
//  различни конектора:
//    • релационен (MySQL, рендерира SQL) → ПРИЕМА я и я материализира в SQL;
//    • документен (MongoDB) → конекторът не декларира `supports_joins`, затова
//      компилаторът ОТКАЗВА програмата с четим static_assert.
//
//  Доказва оси: (а) constexpr IR, (в) един модел над рел.+нерел., (г) договор
//  за способности с gating ПРИ КОМПИЛАЦИЯ. Това е централната уникалност.
//
//  Двата режима (без драйвери — MySQL и MongoDB мок-конекторите са header-only):
//    PASS:   c++ -std=c++23 capability_gating.cpp        → компилира, печата SQL
//    REJECT: c++ -std=c++23 -DDEMO_REJECT capability_gating.cpp → static_assert
// ============================================================================
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MySQLDB/mysql_db.hpp"
#include "ORM/db/connectors/MongoDB/mongodb_db.hpp"

#include <iostream>

// ── обектен модел ───────────────────────────────────────────────────────────
struct User
{
    orm::property<int,          "id">   id;
    orm::property<std::u8string, "name"> name;
};

struct Post
{
    orm::property<int,          "id">        id;
    orm::property<int,          "author_id"> author_id;
    orm::property<std::u8string, "body">     body;
};

int main()
{
    // SELECT User.id, User.name, Post.body
    //   FROM User INNER JOIN Post ON User.id = Post.author_id
    static constexpr auto q = orm::select(
            orm::field<&User::id>,
            orm::field<&User::name>,
            orm::field<&Post::body>)
        .join<orm::join::mode::inner, Post>(
            orm::field<&User::id> == orm::field<&Post::author_id>);

#if defined(DEMO_REJECT)
    // Документен модел: семантиката няма SQL JOIN върху външен ключ, затова
    // конекторът за MongoDB НЕ декларира `supports_joins`. Инстанцирането на
    // db<MongoDB>::operator<< с заявка, съдържаща JOIN, поражда static_assert.
    orm::MongoDB           conn;
    orm::db<orm::MongoDB>  db{conn};
    auto result = db << q;            // <-- compile error: no supports_joins
    (void)result;
#else
    // Релационен конектор (MySQL декларира `supports_joins`): СЪЩАТА заявка
    // компилира — JOIN клаузата е разрешена от договора за способности.
    orm::MySQLDB           conn;
    orm::db<orm::MySQLDB>  db{conn};
    static_assert(orm::is_select_query<decltype(q)>);
    static_assert(decltype(q.join_clauses())::size == 1,
        "заявката съдържа точно една JOIN клауза");
    auto result = db << q;          // компилира се и се изпълнява
    (void)result;
    std::cout << "[MySQL / релационен] СЪЩАТА заявка е ПРИЕТА: compile-time\n"
                 "валидна, JOIN клаузата е разрешена от способностите на конектора.\n";
#endif
    return 0;
}
