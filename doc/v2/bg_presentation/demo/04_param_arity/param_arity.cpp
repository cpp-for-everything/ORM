// ============================================================================
//  ДЕМО 4 — Брой и тип на параметрите спрямо placeholder-ите (ПРИ КОМПИЛАЦИЯ)
// ----------------------------------------------------------------------------
//  `execute(query, params...)` събира анонимните placeholder-и от клаузите на
//  заявката и `static_assert`-ва, че броят и типовете на аргументите съвпадат.
//  Подаване на грешен брой (или тип) аргументи е ОТКАЗ на компилатора.
//
//  Обхват: проверката е за АНОНИМНИ placeholder-и (1:1 позиционно при всеки
//  конектор). Аритетът при ИНДЕКСИРАНИ placeholder-и (ph<T,_N>) е по дефиниция
//  отговорност на конектора (PostgreSQL преизползва $1; MySQL дублира всяко ?) —
//  затова там проверката съзнателно се изключва. Това е архитектурна находка.
//
//  Доказва ос (г). (Добавена след предаването на тезата — продължаващо развитие.)
//
//  Режими (MockDB, header-only):
//    PASS:   c++ -std=c++23 -I lib/include param_arity.cpp
//    REJECT: c++ -std=c++23 -I lib/include -DDEMO_REJECT param_arity.cpp
// ============================================================================
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MockDB/mock_db.hpp"

#include <iostream>

struct User
{
    orm::property<int,          "id">    id;
    orm::property<std::u8string, "name">  name;
    orm::property<double,       "score"> score;
};

int main()
{
    orm::MockDB           conn;
    orm::db<orm::MockDB>  db{conn};

    // Заявка с ДВА анонимни placeholder-а (id и score).
    auto q = orm::select(orm::field<&User::id>)
                 .where((orm::field<&User::id>    == orm::Placeholder<int>{}) &&
                        (orm::field<&User::score> >  orm::Placeholder<double>{}));

    // Коректно извикване — два аргумента от съвместими типове:
    db.execute(q, 7, 3.5);

#if defined(DEMO_REJECT)
    // Грешен брой: заявката иска 2 аргумента, подаден е 1 → static_assert.
    db.execute(q, 7);            // <-- compile error: 1 arg, needs 2
#endif

    std::cout << "Параметрите съвпадат с placeholder-ите по брой и тип — "
                 "проверено ПРИ КОМПИЛАЦИЯ.\n";
    return 0;
}
