// ============================================================================
//  Пример 02 — Анатомия на типизирана заявка
// ============================================================================
//
//  Този пример демонстрира, че заявката НЕ е низ, а статично типизирана
//  стойност, изграждана от composable градивни единици. Цялата конструкция
//  е `constexpr` --- инструментирана е със `static_assert`, така че самият
//  компилатор проверява структурата ѝ без да изпълнява и един ред код.
//
//  Програмата не извършва I/O. Ако се компилира успешно, всички инварианти
//  на заявката са валидни.
// ============================================================================

#include "ORM/ORM.hpp"

#include <iostream>

// ── обектен модел ───────────────────────────────────────────────────────────
struct User
{
    orm::property<int,          "id">    id;
    orm::property<std::u8string,"name">  name;
    orm::property<double,       "score"> score;
};

int main()
{
    // ── 1. Поле като типизирана конструкция ───────────────────────────────
    constexpr auto id_field = orm::field<&User::id>;
    static_assert(decltype(id_field)::column_name() == "id",
        "field<&User::id> носи името на колоната по време на компилация");
    static_assert(std::is_same_v<decltype(id_field)::table_type, User>,
        "field<&User::id> познава таблицата, в която принадлежи");

    // ── 2. Празна SELECT заявка ───────────────────────────────────────────
    constexpr auto q0 = orm::select(orm::field<&User::id>,
                                    orm::field<&User::name>);
    static_assert(decltype(q0)::response_type::size == 2,
        "проекцията има точно 2 колони");

    // ── 3. Добавяме WHERE предикат с runtime заместител ───────────────────
    constexpr auto q1 = q0.where(
        orm::field<&User::score> > 0.0);
    static_assert(decltype(q1.where_clauses())::size == 1,
        "една WHERE клауза е била добавена");

    // ── 4. Добавяме ORDER BY и LIMIT ──────────────────────────────────────
    using namespace orm::literals;
    constexpr auto q2 = q1
        .order_by<orm::order::direction::desc>(orm::field<&User::score>)
        .limit(10_per_page & 1_page);
    static_assert(decltype(q2.order_clauses())::size == 1);
    static_assert(decltype(q2.limit_clauses())::size == 1);

    // ── 5. Същата структура остава compose-ваема и след добавки ───────────
    constexpr auto q3 = q2.where(
        orm::field<&User::id> == orm::Placeholder<int>{});
    static_assert(decltype(q3.where_clauses())::size == 2,
        "втората WHERE клауза --- runtime заместител на id");

    std::cout << "Всичките инварианти на заявката са проверени по време на компилация.\n";
    std::cout << "Финална структура: SELECT 2 кол. WHERE 2 ред. ORDER BY 1 ред. LIMIT 1 ред.\n";
    return 0;
}
