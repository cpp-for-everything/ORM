// What IN and NOT IN must mean, checked on the live server before the corpus runs.
// The corpus shows a payload has no effect. This shows the list still answers the
// question it was asked, on every backend that supports one, against the three seeded
// rows (1 apple), (2 banana), (3 cherry). Each line that fails stops the run.
#pragma once
#include "ORM/ORM.hpp"
#include <format>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace orm_security
{
    template <auto Id, auto Name, typename Db>
    void check_list_semantics(Db& db, const std::function<void()>& reset)
    {
        using Names = std::vector<std::string>;
        using Ids   = std::vector<int>;
        constexpr auto id   = orm::field<Id>;
        constexpr auto name = orm::field<Name>;
        constexpr orm::PlaceholderList<std::string> names{};
        constexpr orm::PlaceholderList<int>         ids{};

        auto expect = [&](const char* what, std::size_t want, auto q, auto&&... args)
        {
            reset();
            const std::size_t got = db.execute(q, args...).to_vector().size();
            if (got != want)
                throw std::runtime_error(std::format(
                    "list semantics: {}: {} rows, expected {}", what, got, want));
        };

        expect("IN matches the listed rows", 2,
               orm::select(id, name).where(orm::in(name, names)), Names{"apple", "cherry"});
        expect("NOT IN matches the others", 1,
               orm::select(id, name).where(orm::not_in(name, names)), Names{"apple", "cherry"});
        expect("IN over integers", 2,
               orm::select(id, name).where(orm::in(id, ids)), Ids{1, 3});
        expect("empty IN matches nothing", 0,
               orm::select(id, name).where(orm::in(name, names)), Names{});
        expect("empty NOT IN matches everything", 3,
               orm::select(id, name).where(orm::not_in(name, names)), Names{});
        expect("a scalar after a list keeps its position", 1,
               orm::select(id, name).where(orm::in(name, names))
                   .where(id == orm::Placeholder<int>{}),
               Names{"apple", "banana", "cherry"}, 2);
        expect("the same scalar with no row to match", 0,
               orm::select(id, name).where(orm::in(name, names))
                   .where(id == orm::Placeholder<int>{}),
               Names{"apple", "banana"}, 3);
        expect("a list and a scalar joined by &&", 1,
               orm::select(id, name).where(orm::in(name, names) &&
                                           id == orm::Placeholder<int>{}),
               Names{"apple", "banana"}, 2);
        expect("a list and a scalar joined by ||", 3,
               orm::select(id, name).where(orm::in(name, names) ||
                                           id == orm::Placeholder<int>{}),
               Names{"apple", "banana"}, 3);
        expect("two lists joined by &&", 1,
               orm::select(id, name).where(orm::in(id, ids) && orm::in(name, names)),
               Ids{1, 2}, Names{"banana", "cherry"});
    }
} // namespace orm_security
