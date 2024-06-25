/**
 *  @file   select.hpp
 *  @brief  Functional-programming-like select queries
 *  @author Alex Tsvetanov
 *  @date   2023-08-31
 ***********************************************/

#pragma once

#include <utility>
#include <tuple>
#include "../details/result_type.hpp"
#include "utils.hpp"
#include "../join_rule.hpp"

namespace webframe::ORM::CRUD {
    template<webframe::ORM::modes::select row_way, typename Result, typename Rules, typename Filters, typename Limits, typename Grouping, typename Ordering>
    class select_query : public details::select_query_t {
    protected:
        Result properties_to_select_query;
        Rules joins;
        Filters filters;
        Limits limits;
        Grouping groupings;
        Ordering ordering;
    public:
        static constexpr webframe::ORM::modes::select mode = row_way;
        using properties = typename webframe::ORM::details::get_placeholders<decltype(std::tuple_cat(
                        typename webframe::ORM::details::get_properties<Rules>::properties(),
                        typename webframe::ORM::details::get_properties<Filters>::properties(),
                        typename webframe::ORM::details::get_properties<Limits>::properties()
                    ))>::orm_placeholders;
        using Response = Result;
        using Joins = Rules;
        using Wheres = Filters;
        using Limitations = Limits;
        using Groupings = Grouping;
        using OrderBy = Ordering;

        constexpr select_query(Result _properties_to_select_query, Rules _joins, Filters _filters, Limits _limits, Grouping _groupings, Ordering _ordering) :
            properties_to_select_query (_properties_to_select_query),
            joins (_joins),
            filters (_filters),
            limits (_limits),
            groupings (_groupings),
            ordering (_ordering) {}

        template<webframe::ORM::modes::join, typename, typename... Ts>
        constexpr inline auto join(Ts...) const;

        template<typename... Ts>
        constexpr inline auto where(Ts...) const;

        template<typename... Ts>
        constexpr inline auto limit(Ts...) const;

        template<auto... Ps>
        constexpr inline auto group_by(auto) const;
        
        template<auto... Ps>
        constexpr inline auto group_by() const;

    protected:
        template<auto, auto..., size_t... inds>
        constexpr inline auto order_by_impl(std::index_sequence<inds...>) const;

        template<auto, auto...>
        constexpr inline auto order_by_helper1() const;

        template<auto, webframe::ORM::modes::order, auto...>
        constexpr inline auto order_by_helper2() const;

    public:
        template<auto...>
        constexpr inline auto order_by() const;

        constexpr Response selected_properties() const { return properties_to_select_query; }
        constexpr Joins join_clauses() const { return joins; }
        constexpr Wheres where_clauses() const { return filters; }
        constexpr Limits limit_clauses() const { return limits; }
        constexpr Grouping group_clauses() const { return groupings; }
        constexpr Ordering order_clauses() const { return ordering; }

        template<webframe::ORM::modes::select, typename, typename, typename, typename, typename, typename>
        friend class select_query;
    };
}
namespace webframe::ORM {
    template <webframe::ORM::modes::select mode, auto... args>
    static constexpr auto select = CRUD::select_query<mode, 
        decltype(webframe::ORM::details::result_t<webframe::ORM::details::result_all, decltype(args)...>::create(args...)), // properties to select
        webframe::ORM::details::orm_tuple<>, // Join clauses
        webframe::ORM::details::orm_tuple<>, // Where clauses
        webframe::ORM::details::orm_tuple<>, // Limit clauses
        webframe::ORM::details::orm_tuple<>, // Group-by clauses
        webframe::ORM::details::orm_tuple<>  // Order-by clauses
        >( 
            webframe::ORM::details::result_t<webframe::ORM::details::result_all, decltype(args)...>::create(args...), // properties to select
            webframe::ORM::details::orm_tuple<>(), // Join clauses
            webframe::ORM::details::orm_tuple<>(), // Where clauses
            webframe::ORM::details::orm_tuple<>(), // Limit clauses
            webframe::ORM::details::orm_tuple<>(), // Group-by clauses
            webframe::ORM::details::orm_tuple<>()  // Order-by clauses
        );
}

#include "../../../src/ORM/CRUD/select.cpp"