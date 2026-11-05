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
#include "ORM/connector/capabilities.hpp"
#include "ORM/query/rules.hpp"
#include "ORM/query/select.hpp"
#include "ORM/query/update.hpp"
#include "ORM/query/delete.hpp"
#include "ORM/query/insert.hpp"
#include "ORM/query/field.hpp"
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
        using type = std::conditional_t<is_placeholder_v<T> || is_list_placeholder_v<T>,
                                        tlist<T>, tlist<>>;
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
    struct any_list;
    template <typename... P>
    struct any_list<tlist<P...>>
    {
        static constexpr bool value = (is_list_placeholder_v<P> || ...);
    };

    // Does this query carry a PlaceholderList anywhere?
    template <typename Q>
    inline constexpr bool query_has_list_v = any_list<typename query_placeholders<Q>::type>::value;

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

    // ── literal operands ──────────────────────────────────────────────────────
    // A leaf operand that is not a placeholder, a member pointer (a column), a
    // field or nullptr is a VALUE written straight into the query, for example
    // `field<&T::col> == v` or `.set(field<&T::col>, v)`.
    //
    // No connector binds such a value. Each renders the operand as a parameter
    // marker and then binds only the arguments passed to execute(), so the
    // marker is left unbound and the driver reads it as NULL. Measured on SQLite
    // (security/probes): a SELECT returned no rows although one matched, an
    // UPDATE bound its single argument into the literal's slot and left the
    // WHERE slot NULL, and a DELETE deleted nothing. The value never became SQL
    // text, so this was not injectable, but the result was silently wrong.
    //
    // Values therefore enter only through orm::Placeholder<T> or orm::ph<T, _N>,
    // and a literal operand reaching execution is a compile error.
    template <typename T>
    inline constexpr bool is_literal_operand_v =
        !is_placeholder_v<T> && !is_list_placeholder_v<T> &&
        !std::is_member_pointer_v<T> && !is_field<T> &&
        !std::is_same_v<T, std::nullptr_t> && !is_rule<T>;

    template <typename T>
    struct has_literal : std::bool_constant<is_literal_operand_v<std::remove_cvref_t<T>>>
    {
    };
    template <typename A, string_literal Op, typename B>
    struct has_literal<Rule<A, Op, B>>
        : std::bool_constant<has_literal<A>::value || has_literal<B>::value>
    {
    };

    template <typename Tuple>
    struct literal_in_rule_tuple : std::false_type
    {
    };
    template <typename... R>
    struct literal_in_rule_tuple<orm_tuple<R...>>
        : std::bool_constant<(has_literal<R>::value || ...)>
    {
    };

    template <typename JR>
    struct literal_in_join : std::false_type
    {
    };
    template <join::mode M, typename Tbl, typename R>
    struct literal_in_join<JoinRule<M, Tbl, R>> : has_literal<R>
    {
    };
    template <typename Tuple>
    struct literal_in_join_tuple : std::false_type
    {
    };
    template <typename... J>
    struct literal_in_join_tuple<orm_tuple<J...>>
        : std::bool_constant<(literal_in_join<J>::value || ...)>
    {
    };

    template <typename S>
    struct literal_in_stmt : std::false_type
    {
    };
    template <typename Tag, typename V>
    struct literal_in_stmt<UpdateStatement<Tag, V>>
        : std::bool_constant<is_literal_operand_v<std::remove_cvref_t<V>> ||
                             is_list_placeholder_v<std::remove_cvref_t<V>>>
    {
    };
    template <typename Tuple>
    struct literal_in_stmt_tuple : std::false_type
    {
    };
    template <typename... S>
    struct literal_in_stmt_tuple<orm_tuple<S...>>
        : std::bool_constant<(literal_in_stmt<S>::value || ...)>
    {
    };

    template <typename Q>
    struct query_has_literal : std::false_type
    {
    };
    template <typename Resp, typename J, typename W, typename L, typename G, typename O>
    struct query_has_literal<select_query<Resp, J, W, L, G, O>>
        : std::bool_constant<literal_in_join_tuple<J>::value || literal_in_rule_tuple<W>::value>
    {
    };
    template <typename Table, typename S, typename W>
    struct query_has_literal<update_query<Table, S, W>>
        : std::bool_constant<literal_in_stmt_tuple<S>::value || literal_in_rule_tuple<W>::value>
    {
    };
    template <typename Table, typename W>
    struct query_has_literal<delete_query<Table, W>> : literal_in_rule_tuple<W>
    {
    };

    // ── is it a query at all? ─────────────────────────────────────────────────
    // The db entry points are templates. They used to accept any type, so text
    // such as a std::string reached connector overload resolution before it was
    // rejected, with an error about the connector rather than about the cause.
    // These four are every query kind any connector executes.
    template <typename Q>
    struct is_query_ir : std::false_type
    {
    };
    template <typename Resp, typename J, typename W, typename L, typename G, typename O>
    struct is_query_ir<select_query<Resp, J, W, L, G, O>> : std::true_type
    {
    };
    template <typename P>
    struct is_query_ir<insert_query<P>> : std::true_type
    {
    };
    template <typename Table, typename S, typename W>
    struct is_query_ir<update_query<Table, S, W>> : std::true_type
    {
    };
    template <typename Table, typename W>
    struct is_query_ir<delete_query<Table, W>> : std::true_type
    {
    };

    // ── entry point: every path that reaches a connector calls this ───────────
    // db::operator<<, db::execute, db::find_one, db::prepare,
    // prepared_query::execute, and the async_db equivalents.
    template <typename Query>
    constexpr void check_query_shape()
    {
        using Q = std::remove_cvref_t<Query>;
        static_assert(
            is_query_ir<Q>::value,
            "orm: this is not a query. execute(), operator<<, find_one() and "
            "prepare() take a query built with orm::select, orm::insert, "
            "orm::update or orm::deleteq. Query text such as a std::string is "
            "not accepted: a query assembled from text is the injection this "
            "library exists to prevent.");
        if constexpr (is_query_ir<Q>::value)
        {
            static_assert(
                !query_has_literal<Q>::value,
                "orm: a literal operand reaches execution. A value was written "
                "directly into the query, as in `field<&T::col> == value` or "
                "`.set(field<&T::col>, value)`. No connector binds such a value: "
                "it is left as an unbound parameter that the database reads as "
                "NULL, so the query silently returns wrong results. Pass the "
                "value through a typed placeholder instead, "
                "`field<&T::col> == orm::Placeholder<V>{}`, and supply it to "
                "execute(), or use orm::ph<V, _N>.");
        }
    }

    // ── entry point: paths that take no arguments ─────────────────────────────
    // db::operator<<, db::find_one and prepared_query::execute() have nothing to
    // supply a list from, so a list reaching them could only render unbound.
    template <typename Query>
    constexpr void check_no_list()
    {
        static_assert(
            !query_has_list_v<std::remove_cvref_t<Query>>,
            "orm: a list placeholder needs its argument. A query with "
            "orm::PlaceholderList must run through execute(query, values...), "
            "where the std::vector supplies the list.");
    }

    // ── entry point: connector support for lists ──────────────────────────────
    template <typename DB, typename Query>
    constexpr void check_list_capability()
    {
        static_assert(
            !query_has_list_v<std::remove_cvref_t<Query>> ||
                has_capability<DB, cap::supports_list_placeholder>,
            "orm: this connector does not support IN / NOT IN. It does not declare "
            "`using supports_list_placeholder = void;`.");
    }

    // ── entry point: called from db::execute and prepared_query::execute ──────
    template <typename Query, typename... Params>
    constexpr void check_query_params()
    {
        check_query_shape<Query>();
        using Q  = std::remove_cvref_t<Query>;
        using QP = query_placeholders<Q>;

        if constexpr (QP::checkable)
        {
            using PH = typename QP::type;
            static_assert(
                !any_list<PH>::value || all_anonymous<PH>::value,
                "orm: a list placeholder cannot share a query with indexed "
                "placeholders (orm::ph<T,_N>). A list expands to a run-time number "
                "of positions, so indexed positions would no longer line up. Use "
                "orm::Placeholder<T> for the other values.");
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
