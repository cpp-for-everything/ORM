// ============================================================================
//  ДЕМО 2 — Анатомия на заявката: тя е ТИПИЗИРАНА СТОЙНОСТ, а не низ
// ----------------------------------------------------------------------------
//  Заявката се изгражда от composable градивни единици и е изцяло `constexpr`.
//  Всеки инвариант е проверен със `static_assert` — компилаторът проверява
//  структурата ѝ, без да изпълни и един ред код. Програмата не прави I/O;
//  ако се компилира, всички инварианти са валидни.
//
//  Доказва ос (а): constexpr междинно представяне на заявката. Контраст: SOCI
//  изгражда низ при изпълнение; sqlpp11 е типизиран, но остава само за SQL.
//
//  Команда:  c++ -std=c++23 -I lib/include query_anatomy.cpp -o anatomy && ./anatomy
// ============================================================================
#include "ORM/ORM.hpp"

#include <iostream>

struct User
{
    orm::property<int,          "id">    id;
    orm::property<std::u8string, "name">  name;
    orm::property<double,       "score"> score;
};

int main()
{
    // 1. Полето е типизирана конструкция, носеща име на колона и таблица.
    constexpr auto id_field = orm::field<&User::id>;
    static_assert(decltype(id_field)::column_name() == "id",
        "field<&User::id> носи името на колоната ПРИ КОМПИЛАЦИЯ");
    static_assert(std::is_same_v<decltype(id_field)::table_type, User>,
        "field<&User::id> познава таблицата, в която принадлежи");

    // 2. SELECT заявка — проекцията е част от типа.
    constexpr auto q0 = orm::select(orm::field<&User::id>,
                                    orm::field<&User::name>);
    static_assert(decltype(q0)::response_type::size == 2,
        "проекцията има точно 2 колони");

    // 3. WHERE предикат с runtime заместител — структурата остава compile-time.
    constexpr auto q1 = q0.where(orm::field<&User::score> > 0.0);
    static_assert(decltype(q1.where_clauses())::size == 1,
        "една WHERE клауза е добавена");

    // 4. ORDER BY + LIMIT — заявката остава composable стойност.
    using namespace orm::literals;
    constexpr auto q2 = q1
        .order_by<orm::order::direction::desc>(orm::field<&User::score>)
        .limit(10_per_page & 1_page);
    static_assert(decltype(q2.order_clauses())::size == 1);
    static_assert(decltype(q2.limit_clauses())::size == 1);

    // 5. Допълнителен WHERE с типизиран placeholder.
    constexpr auto q3 = q2.where(
        orm::field<&User::id> == orm::Placeholder<int>{});
    static_assert(decltype(q3.where_clauses())::size == 2,
        "втора WHERE клауза — runtime заместител на id");

    std::cout << "Всички инварианти на заявката са проверени ПРИ КОМПИЛАЦИЯ.\n"
                 "Заявката е типизирана стойност, не низ: "
                 "SELECT 2 кол. WHERE 2 ред. ORDER BY 1 ред. LIMIT 1 ред.\n";
    return 0;
}
