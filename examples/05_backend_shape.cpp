// ============================================================================
//  ДЕМО 5 — Формата на заявката, наложена от backend-а (ПРИ КОМПИЛАЦИЯ)
// ----------------------------------------------------------------------------
//  Договорът на конектора не само разрешава/забранява операции, но и налага
//  СТРУКТУРАТА на допустимата заявка. Redis е хранилище ключ-стойност: достъпът
//  е само по първичен ключ. Конекторът проверява това с `consteval` предикат и
//  `static_assert` — заявка с не-PK предикат изобщо не се компилира.
//
//  Доказва оси (в) един модел над нерелационни системи + (г) backend-специфично
//  ограничение, наложено ПРИ КОМПИЛАЦИЯ — вместо рънтайм грешка от драйвера.
//  (Аналогично: Cassandra изисква предикат по partition key.)
//
//  Режими (RedisDB е header-only mock — без hiredis):
//    PASS:   c++ -std=c++23 -I lib/include backend_shape.cpp
//    REJECT: c++ -std=c++23 -I lib/include -DDEMO_REJECT backend_shape.cpp
// ============================================================================
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/RedisDB/redis_db.hpp"

#include <iostream>

struct Session
{
    orm::property<int,          "id">    id;
    orm::property<std::u8string, "token"> token;
};

int main()
{
    orm::RedisDB           conn;
    orm::db<orm::RedisDB>  db{conn};

    // Достъп по първичен ключ (една WHERE клауза по ключа) — компилира се:
    auto ok = orm::select(orm::field<&Session::token>)
                  .where(orm::field<&Session::id> == orm::Placeholder<int>{});
    db << ok;

#if defined(DEMO_REJECT)
    // Не-PK предикат (втора WHERE клауза) — Redis няма заявки по произволно поле.
    auto bad = orm::select(orm::field<&Session::token>)
                   .where(orm::field<&Session::id>    == orm::Placeholder<int>{})
                   .where(orm::field<&Session::token> == orm::Placeholder<std::u8string>{});
    db << bad;                  // <-- compile error: non-PK predicate
#endif

    std::cout << "Redis приема само достъп по първичен ключ — "
                 "наложено ПРИ КОМПИЛАЦИЯ, не при изпълнение.\n";
    return 0;
}
