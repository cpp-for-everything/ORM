// ============================================================================
//  ДЕМО A — Компилаторът като първи валидатор на заявката
// ============================================================================
//
//  Една и съща SELECT...JOIN заявка, две системи за съхранение.
//  Компилаторът отказва тази, за която JOIN няма смисъл.
//
//  ▸ Този пример е живият отговор на ВЪПРОС 2 от рецензията: „Какви грешки
//    могат да бъдат открити по време на компилация вместо при изпълнение?"
//    Неподдържана операция за дадено хранилище (тук JOIN срещу MongoDB) е
//    отказ на компилатора, а не изключение при изпълнение.
//    В края на main() има изключено меню (#if 0) с още класове compile-time
//    откази. Пълните отговори: doc/v2/bg_presentation/revizia_otgovori.md
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

    // ════════════════════════════════════════════════════════════════════════
    //  МЕНЮ ОТ COMPILE-TIME ОТКАЗИ (за въпросите от рецензията)
    // ────────────────────────────────────────────────────────────────────────
    //  По подразбиране ИЗКЛЮЧЕНО (#if 0) — staged демото не се променя. За жива
    //  демонстрация на още класове грешки смени отделен блок на `#if 1` и
    //  прекомпилирай (или, в редактора, разкоментирай реда и виж squiggle).
    //  Всеки блок е ОТДЕЛЕН клас грешка, уловена при компилация вместо изпълнение.
    // ════════════════════════════════════════════════════════════════════════

    // ── Q2 / клас „неспециализиран конектор" ─────────────────────────────────
    //  orm::db<DB> е ограничен с `requires is_connector<DB>`; за неспециализиран
    //  тип удря четимия static_assert от connector_trait<DB>.
#if 0
    struct NotAConnector {};
    NotAConnector nc;
    orm::db<NotAConnector> bad{nc};   // ← static_assert: "...has not been specialised..."
    (void)bad;
#endif

    // ── Q2 / клас „невалиден индекс на placeholder" ──────────────────────────
    //  ph<T, N> изисква N да е std::placeholders::_K; константата 0 не е такава →
    //  ограничението requires(std::is_placeholder<decltype(N)>::value > 0) се проваля.
#if 0
    auto bad_ph = orm::ph<int, 0>;    // ← не се компилира: невалиден placeholder
    (void)bad_ph;
#endif

    // ── Q2 / клас „несъвместим тип на сравнение" ─────────────────────────────
    //  Сравнение на int колона със стойност от низов тип вече е ОТКАЗ на
    //  компилатора: операторите в rules.hpp изваждат C++ типа на колоната (през
    //  property<T,Name>) и типа на десния операнд и ги сверяват за съвместимост.
#if 0
    auto type_mismatch =
        orm::field<&User::id> == std::u8string{u8"not-an-int"};   // ← static_assert: incompatible operand types
    (void)type_mismatch;
#endif

    // ── Q1/Q2 / клас „грешен брой аргументи към execute" ─────────────────────
    //  Заявка с N анонимни placeholder-а, извикана с != N аргумента, е отказ на
    //  компилатора (param_check.hpp). Свързването е 1:1 позиционно за анонимни ph;
    //  аритетът при индексирани placeholder-и е отговорност на конектора.
#if 0
    auto pq = orm::select(orm::field<&User::id>)
        .where(orm::field<&User::id> == orm::Placeholder<int>{});
    facade.execute(pq);   // ← static_assert: wrong number of runtime parameters (0 vs 1)
#endif

    return 0;
}
