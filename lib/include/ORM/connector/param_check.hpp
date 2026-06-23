#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Compile-time validation of runtime parameters against query placeholders.
//
//  When execute(query, params...) is called, the number and types of params are
//  checked against the placeholders declared in the query's clauses — turning a
//  "wrong number/type of bound parameters" mistake into a compile error.
//
//  SCOPE — deliberately limited to the case that is unambiguous across every
//  connector: queries whose placeholders are ALL anonymous (orm::Placeholder<T>),
//  for SELECT / UPDATE / DELETE. There the binding is strictly 1:1 positional in
//  clause order on every backend, so the check is universally correct.
//
//  The check stands down (does nothing) when:
//    • the query uses INDEXED placeholders (orm::ph<T, _N>) — arity is then
//      connector-defined (PostgreSQL reuses $1 natively → 1 arg; MySQL duplicates
//      each ? → 1 arg per occurrence), so it belongs to the connector contract,
//      not the shared query IR;
//    • the query is an INSERT — parameter expansion is connector-specific
//      (e.g. Redis binds extra values per field).
// ─────────────────────────────────────────────────────────────────────────────
#include "ORM/query/rules.hpp"
#include "ORM/query/select.hpp"
#include "ORM/query/update.hpp"
#include "ORM/query/delete.hpp"
#include "ORM/details/orm_tuple.hpp"
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace orm::detail {

    // ── minimal compile-time type list ────────────────────────────────────────
    template <typename...>
    struct tlist
    {
    };

    template <typename A, typename B>
    struct tl_cat2;
    template <typename... A, typename... B>
    struct tl_cat2<tlist<A...>, tlist<B...>>
    {
        using type = tlist<A..., B...>;
    };

    template <typename... Ls>
    struct tl_cat_all
    {
        using type = tlist<>;
    };
    template <typename L0, typename... Ls>
    struct tl_cat_all<L0, Ls...>
    {
        using type = typename tl_cat2<L0, typename tl_cat_all<Ls...>::type>::type;
    };

    template <typename TL>
    struct tl_to_tuple;
    template <typename... P>
    struct tl_to_tuple<tlist<P...>>
    {
        using type = std::tuple<P...>;
    };

    // ── collect placeholder types from a Rule tree / operand ──────────────────
    // A leaf operand is a placeholder, a member pointer, a literal or NULL; only
    // placeholders are collected. A Rule<…> is walked recursively, left to right.
    template <typename T>
    struct phs_in
    {
        using type = std::conditional_t<is_placeholder_v<T>, tlist<T>, tlist<>>;
    };
    template <typename A, string_literal Op, typename B>
    struct phs_in<Rule<A, Op, B>>
    {
        using type =
            typename tl_cat2<typename phs_in<A>::type, typename phs_in<B>::type>::type;
    };

    // ── collect from an orm_tuple of WHERE rules (in order) ───────────────────
    template <typename Tuple>
    struct phs_in_rule_tuple;
    template <typename... R>
    struct phs_in_rule_tuple<orm_tuple<R...>>
    {
        using type = typename tl_cat_all<typename phs_in<R>::type...>::type;
    };

    // ── collect from an orm_tuple of JOIN rules (each carries an ON rule) ──────
    template <typename JR>
    struct phs_in_join
    {
        using type = tlist<>;
    };
    template <join::mode M, typename Tbl, typename R>
    struct phs_in_join<JoinRule<M, Tbl, R>>
    {
        using type = typename phs_in<R>::type;
    };
    template <typename Tuple>
    struct phs_in_join_tuple;
    template <typename... J>
    struct phs_in_join_tuple<orm_tuple<J...>>
    {
        using type = typename tl_cat_all<typename phs_in_join<J>::type...>::type;
    };

    // ── collect from an orm_tuple of UPDATE SET statements ────────────────────
    template <typename S>
    struct phs_in_stmt
    {
        using type = tlist<>;
    };
    template <typename Tag, typename V>
    struct phs_in_stmt<UpdateStatement<Tag, V>>
    {
        using type = std::conditional_t<is_placeholder_v<V>, tlist<V>, tlist<>>;
    };
    template <typename Tuple>
    struct phs_in_stmt_tuple;
    template <typename... S>
    struct phs_in_stmt_tuple<orm_tuple<S...>>
    {
        using type = typename tl_cat_all<typename phs_in_stmt<S>::type...>::type;
    };

    // ── placeholders of a query, in binding order ─────────────────────────────
    // Primary template: not checkable (INSERT and any other query kind).
    template <typename Q>
    struct query_placeholders
    {
        using type = tlist<>;
        static constexpr bool checkable = false;
    };
    // SELECT — JOIN ON-rules, then WHERE clauses.
    template <typename Resp, typename J, typename W, typename L, typename G, typename O>
    struct query_placeholders<select_query<Resp, J, W, L, G, O>>
    {
        using type = typename tl_cat2<typename phs_in_join_tuple<J>::type,
                                      typename phs_in_rule_tuple<W>::type>::type;
        static constexpr bool checkable = true;
    };
    // UPDATE — SET statements, then WHERE clauses.
    template <typename Table, typename S, typename W>
    struct query_placeholders<update_query<Table, S, W>>
    {
        using type = typename tl_cat2<typename phs_in_stmt_tuple<S>::type,
                                      typename phs_in_rule_tuple<W>::type>::type;
        static constexpr bool checkable = true;
    };
    // DELETE — WHERE clauses.
    template <typename Table, typename W>
    struct query_placeholders<delete_query<Table, W>>
    {
        using type = typename phs_in_rule_tuple<W>::type;
        static constexpr bool checkable = true;
    };

    // ── predicates over the collected placeholder list ────────────────────────
    template <typename TL>
    struct all_anonymous;
    template <typename... P>
    struct all_anonymous<tlist<P...>>
    {
        static constexpr bool value = ((placeholder_index_v<P> == 0) && ...);
    };

    template <typename TL>
    struct tl_size;
    template <typename... P>
    struct tl_size<tlist<P...>>
    {
        static constexpr std::size_t value = sizeof...(P);
    };

    // ── positional type compatibility: i-th placeholder vs i-th parameter ─────
    template <typename PhTuple, typename ParamTuple, std::size_t... Is>
    constexpr bool positional_types_ok(std::index_sequence<Is...>)
    {
        return (comparable_value_types<
                    typename std::tuple_element_t<Is, PhTuple>::value_type,
                    std::decay_t<std::tuple_element_t<Is, ParamTuple>>> &&
                ...);
    }

    // ── entry point: called from db::execute and prepared_query::execute ──────
    template <typename Query, typename... Params>
    constexpr void check_query_params()
    {
        using Q  = std::remove_cvref_t<Query>;
        using QP = query_placeholders<Q>;

        if constexpr (QP::checkable)
        {
            using PH = typename QP::type;
            if constexpr (all_anonymous<PH>::value)
            {
                constexpr std::size_t n_ph = tl_size<PH>::value;
                static_assert(
                    n_ph == sizeof...(Params),
                    "orm: wrong number of runtime parameters passed to execute(). "
                    "The argument count does not match the number of anonymous "
                    "placeholders (orm::Placeholder<T>) in the query's clauses. "
                    "Note: queries with no placeholders should be executed via "
                    "`db << query`, and indexed placeholders (orm::ph<T,_N>) are "
                    "validated by the connector.");
                if constexpr (n_ph == sizeof...(Params))
                {
                    static_assert(
                        positional_types_ok<typename tl_to_tuple<PH>::type,
                                            std::tuple<Params...>>(
                            std::make_index_sequence<n_ph>{}),
                        "orm: a runtime parameter type is incompatible with the "
                        "placeholder it binds to. Check the argument types passed "
                        "to execute() against the Placeholder<T> types declared in "
                        "the query (parameters bind positionally in clause order).");
                }
            }
        }
    }

} // namespace orm::detail
