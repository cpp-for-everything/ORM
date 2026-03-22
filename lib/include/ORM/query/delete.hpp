#pragma once
#include "ORM/query/rules.hpp"
#include "ORM/query/join_rule.hpp"
#include "ORM/query/limits.hpp"
#include "ORM/details/orm_tuple.hpp"
#include <type_traits>

namespace orm {

    struct delete_query_tag {};

    template <typename T>
    concept is_delete_query = std::derived_from<T, delete_query_tag>;

    // ── delete_query<Table, Wheres> ───────────────────────────────────────────
    template <
        typename Table,
        typename Wheres = detail::orm_tuple<>
    >
    struct delete_query : delete_query_tag
    {
        using table = Table;

        Wheres wheres_;

        constexpr delete_query() = default;
        explicit constexpr delete_query(Wheres w) : wheres_(std::move(w)) {}

        // .where(rule)
        template <typename RuleType>
            requires is_rule<RuleType>
        [[nodiscard]] constexpr auto where(RuleType r) const
        {
            using NewWheres = detail::append_type_t<Wheres, RuleType>;
            return delete_query<Table, NewWheres>(
                detail::tuple_cat(wheres_, detail::orm_tuple<RuleType>(r)));
        }

        [[nodiscard]] constexpr Wheres wheres() const { return wheres_; }
    };

    // ── deleteq<Table>() factory ──────────────────────────────────────────────
    template <typename Table>
    [[nodiscard]] constexpr auto deleteq()
    {
        return delete_query<Table>{};
    }

} // namespace orm
