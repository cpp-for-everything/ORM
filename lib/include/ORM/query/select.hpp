#pragma once
#include "ORM/query/field.hpp"
#include "ORM/query/rules.hpp"
#include "ORM/query/join_rule.hpp"
#include "ORM/query/limits.hpp"
#include "ORM/details/orm_tuple.hpp"
#include <type_traits>

namespace orm {

    struct select_query_tag {};

    template <typename T>
    concept is_select_query = std::derived_from<T, select_query_tag>;

    // ── select_query<Response, Joins, Wheres, Limits, Groups, Orders> ─────────
    template <
        typename Response,
        typename Joins        = detail::orm_tuple<>,
        typename Wheres       = detail::orm_tuple<>,
        typename Limits       = detail::orm_tuple<>,
        typename Groups       = detail::orm_tuple<>,
        typename Orders       = detail::orm_tuple<>
    >
    struct select_query : select_query_tag
    {
        using response_type = Response;
        using row_type      = projected_type<Response>;

        Response response_;
        Joins    joins_;
        Wheres   wheres_;
        Limits   limits_;
        Groups   groups_;
        Orders   orders_;

        constexpr select_query() = default;
        constexpr select_query(
            Response r, Joins j, Wheres w, Limits l, Groups g, Orders o)
            : response_(std::move(r)), joins_(std::move(j)), wheres_(std::move(w)),
              limits_(std::move(l)), groups_(std::move(g)), orders_(std::move(o)) {}

        [[nodiscard]] constexpr Response selected_properties() const { return response_; }
        [[nodiscard]] constexpr Joins    join_clauses()        const { return joins_; }
        [[nodiscard]] constexpr Wheres   where_clauses()       const { return wheres_; }
        [[nodiscard]] constexpr Limits   limit_clauses()       const { return limits_; }
        [[nodiscard]] constexpr Groups   group_clauses()       const { return groups_; }
        [[nodiscard]] constexpr Orders   order_clauses()       const { return orders_; }

        // .where(rule)
        template <typename RuleType>
            requires is_rule<RuleType>
        [[nodiscard]] constexpr auto where(RuleType r) const
        {
            using NewWheres = detail::append_type_t<Wheres, RuleType>;
            return select_query<Response, Joins, NewWheres, Limits, Groups, Orders>(
                response_, joins_,
                detail::tuple_cat(wheres_, detail::orm_tuple<RuleType>(r)),
                limits_, groups_, orders_);
        }

        // .join<Mode, Table>(rule)
        template <join::mode JMode, typename Table, typename RuleType>
            requires is_rule<RuleType>
        [[nodiscard]] constexpr auto join(RuleType r) const
        {
            using JR = JoinRule<JMode, Table, RuleType>;
            using NewJoins = detail::append_type_t<Joins, JR>;
            return select_query<Response, NewJoins, Wheres, Limits, Groups, Orders>(
                response_,
                detail::tuple_cat(joins_, detail::orm_tuple<JR>(JR{r})),
                wheres_, limits_, groups_, orders_);
        }

        // .group_by(field<&T::m>)
        template <auto Ptr>
        [[nodiscard]] constexpr auto group_by(mem_ptr<Ptr> /*f*/) const
        {
            using GB = GroupBy<Ptr>;
            using NewGroups = detail::append_type_t<Groups, GB>;
            return select_query<Response, Joins, Wheres, Limits, NewGroups, Orders>(
                response_, joins_, wheres_, limits_,
                detail::tuple_cat(groups_, detail::orm_tuple<GB>(GB{})),
                orders_);
        }

        // .order_by<Dir>(field<&T::m>)
        template <order::direction Dir, auto Ptr>
        [[nodiscard]] constexpr auto order_by(mem_ptr<Ptr> /*f*/) const
        {
            using OB = OrderBy<Dir, Ptr>;
            using NewOrders = detail::append_type_t<Orders, OB>;
            return select_query<Response, Joins, Wheres, Limits, Groups, NewOrders>(
                response_, joins_, wheres_, limits_, groups_,
                detail::tuple_cat(orders_, detail::orm_tuple<OB>(OB{})));
        }

        // .limit(Pagification)
        [[nodiscard]] constexpr auto limit(Pagification p) const
        {
            using NewLimits = detail::append_type_t<Limits, Pagification>;
            return select_query<Response, Joins, Wheres, NewLimits, Groups, Orders>(
                response_, joins_, wheres_,
                detail::tuple_cat(limits_, detail::orm_tuple<Pagification>(p)),
                groups_, orders_);
        }
    };

    // ── select(...) factory ───────────────────────────────────────────────────
    template <typename... Fields>
        requires (is_field<Fields> && ...)
    [[nodiscard]] constexpr auto select(Fields... fields)
    {
        using Response = detail::orm_tuple<Fields...>;
        return select_query<Response>(
            Response{fields...},
            detail::orm_tuple<>{},
            detail::orm_tuple<>{},
            detail::orm_tuple<>{},
            detail::orm_tuple<>{},
            detail::orm_tuple<>{});
    }

} // namespace orm
