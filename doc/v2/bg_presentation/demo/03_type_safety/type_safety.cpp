// ============================================================================
//  ДЕМО 3 — Типова съвместимост поле↔стойност при сравнение (ПРИ КОМПИЛАЦИЯ)
// ----------------------------------------------------------------------------
//  Операторите за сравнение изваждат C++ типа на колоната (през property<T,Name>)
//  и типа на десния операнд и ги сверяват. Сравнение на `int` колона със стойност
//  от низов тип е ОТКАЗ на компилатора. Релацията е „еднакви или конвертируеми в
//  едната посока" — затова `score > 0` (int литерал срещу double) остава валидно.
//
//  Доказва оси (а) constexpr IR + (г) валидиране при компилация.
//  (Проверката е добавена след предаването на тезата — продължаващо развитие.)
//
//  Двата режима (header-only, без драйвери):
//    PASS:   c++ -std=c++23 -I lib/include type_safety.cpp
//    REJECT: c++ -std=c++23 -I lib/include -DDEMO_REJECT type_safety.cpp
// ============================================================================
#include "ORM/ORM.hpp"

#include <iostream>
#include <string>

struct User
{
    orm::property<int,          "id">    id;
    orm::property<std::u8string, "name">  name;
    orm::property<double,       "score"> score;
};

int main()
{
    // Валидни сравнения — съвместими типове, компилират се:
    [[maybe_unused]] auto a = orm::field<&User::id>    == orm::Placeholder<int>{};
    [[maybe_unused]] auto b = orm::field<&User::name>  == orm::Placeholder<std::u8string>{};
    [[maybe_unused]] auto c = orm::field<&User::score> > 0;   // double колона срещу int литерал (конвертируемо)

#if defined(DEMO_REJECT)
    // `id` е int колона, сравнена със стойност от низов тип → static_assert.
    [[maybe_unused]] auto bad =
        orm::field<&User::id> == std::u8string{u8"not-an-int"};   // <-- compile error
#endif

    std::cout << "Всички сравнения са с типово съвместими операнди — "
                 "проверено ПРИ КОМПИЛАЦИЯ.\n";
    return 0;
}
