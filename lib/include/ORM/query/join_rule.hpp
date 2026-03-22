#pragma once
#include "ORM/query/rules.hpp"
#include "ORM/details/orm_tuple.hpp"

namespace orm {

    namespace join {
        enum class mode { inner, left, right, full };
    } // namespace join

    namespace order {
        enum class direction { asc, desc };
    } // namespace order

    // ── JoinRule<Mode, Table, RuleType> ───────────────────────────────────────
    template <join::mode Mode, typename Table, typename RuleType>
    struct JoinRule
    {
        static constexpr join::mode mode = Mode;
        using table_type = Table;
        RuleType rule_;

        explicit constexpr JoinRule(RuleType r) : rule_(std::move(r)) {}
        [[nodiscard]] constexpr const RuleType& to_rule() const noexcept { return rule_; }
    };

    template <typename T>
    struct is_join_rule_trait : std::false_type {};

    template <join::mode M, typename T, typename R>
    struct is_join_rule_trait<JoinRule<M, T, R>> : std::true_type {};

    template <typename T>
    inline constexpr bool is_join_rule_v = is_join_rule_trait<T>::value;

    // ── OrderBy<Dir, MemberPtr> ───────────────────────────────────────────────
    template <order::direction Dir, auto MemberPtr>
    struct OrderBy
    {
        static constexpr order::direction sort = Dir;
        static constexpr auto member = MemberPtr;
    };

    // ── GroupBy<MemberPtr> ────────────────────────────────────────────────────
    // Single-field GROUP BY tag, parallel to OrderBy<Dir, Ptr>.
    template <auto MemberPtr>
    struct GroupBy
    {
        static constexpr auto member = MemberPtr;
    };

    // ── GroupByRule<PropertiesTuple, [RuleType]> ──────────────────────────────
    template <typename PropertiesTuple, typename RuleType = void>
    struct GroupByRule
    {
        using Properties = PropertiesTuple;
        PropertiesTuple properties_;
        RuleType rule_;

        [[nodiscard]] static constexpr bool has_rule() noexcept { return true; }
        [[nodiscard]] constexpr const RuleType& to_rule() const noexcept { return rule_; }
    };

    template <typename PropertiesTuple>
    struct GroupByRule<PropertiesTuple, void>
    {
        using Properties = PropertiesTuple;
        PropertiesTuple properties_;

        [[nodiscard]] static constexpr bool has_rule() noexcept { return false; }
    };

} // namespace orm
