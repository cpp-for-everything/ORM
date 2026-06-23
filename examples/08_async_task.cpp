// ============================================================================
//  Пример 04 — Корутинна задача (Task<T>) и nested co_await
// ============================================================================
//
//  `orm::Task<T>` е централната абстракция на асинхронния слой. Тя:
//
//    - стартира мързеливо --- тялото на корутината не започва, докато
//      някой не я await-не или не извика `start_detached()` / `sync_wait()`;
//
//    - е композируема --- едно `Task` може да `co_await`-ва друго, без да
//      губи типизиране и без callback hell;
//
//    - безопасно прехвърля изключения от корутината към извикващия чрез
//      запазен `std::exception_ptr`;
//
//    - предоставя `sync_wait()` като контролиран мост към некорутинен код
//      (полезно за тестове и за интеграция със синхронни системи).
//
//  Примерът демонстрира трите модела на употреба: void задача, задача с
//  върната стойност, и nested композиране.
// ============================================================================

#include "ORM/async/task.hpp"

#include <iostream>
#include <string>

// ── 1. void задача със страничен ефект ──────────────────────────────────────
auto greet() -> orm::Task<void>
{
    std::cout << "[greet] Здравей!\n";
    co_return;
}

// ── 2. задача, връщаща стойност ─────────────────────────────────────────────
auto compute_answer() -> orm::Task<int>
{
    co_return 42;
}

// ── 3. композиция: outer awaits inner ──────────────────────────────────────
auto make_message() -> orm::Task<std::string>
{
    int answer = co_await compute_answer();
    co_return "Отговорът е " + std::to_string(answer);
}

// ── 4. безопасно прехвърляне на изключения ─────────────────────────────────
auto might_throw() -> orm::Task<int>
{
    throw std::runtime_error("умишлена грешка от корутина");
    co_return 0; // never reached
}

int main()
{
    // void задача
    greet().sync_wait();

    // задача със стойност
    const int n = compute_answer().sync_wait();
    std::cout << "[main] compute_answer() = " << n << '\n';

    // nested композиция
    const auto msg = make_message().sync_wait();
    std::cout << "[main] " << msg << '\n';

    // прехвърляне на изключения
    try
    {
        (void)might_throw().sync_wait();
        std::cout << "[main] (не би трябвало да се стигне дотук)\n";
    }
    catch (const std::exception& e)
    {
        std::cout << "[main] хванато изключение: " << e.what() << '\n';
    }

    return 0;
}
