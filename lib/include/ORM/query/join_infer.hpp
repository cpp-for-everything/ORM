#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Compile-time relationship-aware JOIN inference.
//
//  Given the field list of a select(), determine the set of referenced tables and
//  synthesise the JOINs that connect them — following `relationship<reference<…>>`
//  foreign keys, multi-hop (e.g. Comment→Post→User) and many-joins (parallel),
//  pulling in unselected intermediate tables when a target is only reachable
//  through them. The join type per edge comes from the field/optional_field 2×2
//  matrix. Pure type-level metaprogramming over Boost.PFR-reflected entities.
// ─────────────────────────────────────────────────────────────────────────────
#include "ORM/details/orm_tuple.hpp"
#include "ORM/query/field.hpp"
#include "ORM/query/join_rule.hpp"        // join::mode
#include "ORM/entity/relationship.hpp"
#include <boost/pfr.hpp>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace orm::detail {

    // ── minimal type list ─────────────────────────────────────────────────────
    template <class...>
    struct tl
    {
    };

    template <class A, class B>
    struct tl_cat2;
    template <class... A, class... B>
    struct tl_cat2<tl<A...>, tl<B...>>
    {
        using type = tl<A..., B...>;
    };
    template <class A, class B>
    using tl_cat2_t = typename tl_cat2<A, B>::type;

    template <class... Ls>
    struct tl_cat
    {
        using type = tl<>;
    };
    template <class L0, class... Ls>
    struct tl_cat<L0, Ls...>
    {
        using type = tl_cat2_t<L0, typename tl_cat<Ls...>::type>;
    };

    template <class T, class L>
    struct tl_has : std::false_type
    {
    };
    template <class T, class H, class... R>
    struct tl_has<T, tl<H, R...>>
        : std::conditional_t<std::is_same_v<T, H>, std::true_type, tl_has<T, tl<R...>>>
    {
    };

    template <class T, class L>
    using tl_push_unique_t =
        std::conditional_t<tl_has<T, L>::value, L, tl_cat2_t<L, tl<T>>>;

    template <class L, class Acc = tl<>>
    struct tl_dedup
    {
        using type = Acc;
    };
    template <class H, class... T, class Acc>
    struct tl_dedup<tl<H, T...>, Acc> : tl_dedup<tl<T...>, tl_push_unique_t<H, Acc>>
    {
    };
    template <class L>
    using tl_dedup_t = typename tl_dedup<L>::type;

    // ── distinct referenced tables (base = first selected field's table) ───────
    template <class Acc, class... Fs>
    struct collect_tables
    {
        using type = Acc;
    };
    template <class Acc, class F, class... Fs>
    struct collect_tables<Acc, F, Fs...>
        : collect_tables<tl_push_unique_t<typename F::table_type, Acc>, Fs...>
    {
    };

    template <class Tuple>
    struct distinct_tables;
    template <class... Fs>
    struct distinct_tables<orm_tuple<Fs...>>
    {
        using type = typename collect_tables<tl<>, Fs...>::type;
    };
    template <class Tuple>
    using distinct_tables_t = typename distinct_tables<Tuple>::type;

    // ── per-table "required" flag: ≥1 selected required (non-optional) field ───
    template <class Table, class Tuple>
    struct table_required;
    template <class Table, class... Fs>
    struct table_required<Table, orm_tuple<Fs...>>
        : std::bool_constant<((std::is_same_v<typename Fs::table_type, Table> &&
                               !Fs::optional) ||
                              ...)>
    {
    };

    // ── relationship-graph edge: From --(Rel: fk_col → To.pk_col)--> To ────────
    template <class From, class Rel>
    struct edge
    {
        using from = From;
        using rel  = Rel;
        using to   = typename Rel::target_table;
    };

    // ── out_edges<E>: scan E's members (PFR) for reference relationships ───────
    template <class E, std::size_t I>
    struct member_edge
    {
        using M = boost::pfr::tuple_element_t<I, E>;
        using type =
            std::conditional_t<is_reference_relationship_v<M>, tl<edge<E, M>>, tl<>>;
    };
    template <class E, class Seq>
    struct out_edges_seq;
    template <class E, std::size_t... Is>
    struct out_edges_seq<E, std::index_sequence<Is...>>
    {
        using type = typename tl_cat<typename member_edge<E, Is>::type...>::type;
    };
    template <class E>
    using out_edges_t =
        typename out_edges_seq<E, std::make_index_sequence<boost::pfr::tuple_size_v<E>>>::type;

    // ── forward reachability: parent-edge map from Base ────────────────────────
    struct root_edge
    {
    };
    template <class Node, class Parent>
    struct pe
    {
        using node   = Node;
        using parent = Parent;
    };

    template <class Node, class PM>
    struct pm_has : std::false_type
    {
    };
    template <class Node, class H, class... R>
    struct pm_has<Node, tl<H, R...>>
        : std::conditional_t<std::is_same_v<Node, typename H::node>, std::true_type,
                             pm_has<Node, tl<R...>>>
    {
    };

    template <class Node, class PM>
    struct pm_get
    {
        using type = void;
    };
    template <class Node, class H, class... R>
    struct pm_get<Node, tl<H, R...>>
    {
        using type = std::conditional_t<std::is_same_v<Node, typename H::node>,
                                        typename H::parent,
                                        typename pm_get<Node, tl<R...>>::type>;
    };

    // process a node's out-edges: add unseen targets to the parent map + worklist
    template <class Edges, class PM, class WL>
    struct add_edges
    {
        using pm = PM;
        using wl = WL;
    };
    template <class Eg, class... Egs, class PM, class WL>
    struct add_edges<tl<Eg, Egs...>, PM, WL>
    {
        using To              = typename Eg::to;
        static constexpr bool seen = pm_has<To, PM>::value;
        using PM2             = std::conditional_t<seen, PM, tl_cat2_t<PM, tl<pe<To, Eg>>>>;
        using WL2             = std::conditional_t<seen, WL, tl_cat2_t<WL, tl<To>>>;
        using rec             = add_edges<tl<Egs...>, PM2, WL2>;
        using pm              = typename rec::pm;
        using wl              = typename rec::wl;
    };

    template <class WL, class PM>
    struct reach_impl
    {
        using type = PM;
    };
    template <class H, class... T, class PM>
    struct reach_impl<tl<H, T...>, PM>
    {
        using step = add_edges<out_edges_t<H>, PM, tl<T...>>;
        using type = typename reach_impl<typename step::wl, typename step::pm>::type;
    };

    template <class Base>
    using reachability_t = typename reach_impl<tl<Base>, tl<pe<Base, root_edge>>>::type;

    // ── path from Base to Target (base→target edge order) ──────────────────────
    template <class Target, class PM, class Acc, class Parent = typename pm_get<Target, PM>::type>
    struct path_back
    {
        using type =
            typename path_back<typename Parent::from, PM, tl_cat2_t<tl<Parent>, Acc>>::type;
    };
    template <class Target, class PM, class Acc>
    struct path_back<Target, PM, Acc, root_edge>
    {
        using type = Acc;
    };

    // ── join step: a renderable join (mode + the edge it came from) ────────────
    template <join::mode Mode, class Eg>
    struct join_step
    {
        static constexpr join::mode mode = Mode;
        using from = typename Eg::from;
        using rel  = typename Eg::rel;
        using to   = typename Eg::to;
    };

    [[nodiscard]] constexpr join::mode join_mode_for(bool req_base, bool req_to) noexcept
    {
        if (req_base && req_to)   return join::mode::inner;
        if (req_base && !req_to)  return join::mode::left;
        if (!req_base && req_to)  return join::mode::right;
        return join::mode::full;
    }

    // tag each edge of a target's path with the join mode for (base, target)
    template <class Edges, bool ReqBase, bool ReqTarget>
    struct tag_path;
    template <bool ReqBase, bool ReqTarget>
    struct tag_path<tl<>, ReqBase, ReqTarget>
    {
        using type = tl<>;
    };
    template <class Eg, class... Egs, bool ReqBase, bool ReqTarget>
    struct tag_path<tl<Eg, Egs...>, ReqBase, ReqTarget>
    {
        using type = tl_cat2_t<tl<join_step<join_mode_for(ReqBase, ReqTarget), Eg>>,
                               typename tag_path<tl<Egs...>, ReqBase, ReqTarget>::type>;
    };

    // ── build the full join plan for a Response ────────────────────────────────
    template <class Response, class Base, class PM, class Targets>
    struct build_plan
    {
        using type = tl<>;
    };
    template <class Response, class Base, class PM, class T0, class... Ts>
    struct build_plan<Response, Base, PM, tl<T0, Ts...>>
    {
        static_assert(pm_has<T0, PM>::value,
            "orm: a selected table is not reachable from the base table via any "
            "relationship<store_as::reference<...>>. Define an FK relationship "
            "linking them, or select a field from an intermediate table.");
        using path  = typename path_back<T0, PM, tl<>>::type;
        using steps = typename tag_path<path, table_required<Base, Response>::value,
                                        table_required<T0, Response>::value>::type;
        using type  = tl_cat2_t<steps, typename build_plan<Response, Base, PM, tl<Ts...>>::type>;
    };

    template <class Response>
    struct join_plan_impl
    {
        using tables  = distinct_tables_t<Response>;
        using type    = tl<>;
    };
    template <class Response, class Base, class... Targets>
    struct join_plan_with_base
    {
        using type = tl_dedup_t<
            typename build_plan<Response, Base, reachability_t<Base>, tl<Targets...>>::type>;
    };
    template <class Response, class Tables>
    struct join_plan_dispatch
    {
        using type = tl<>;
    };
    template <class Response, class Base, class... Targets>
    struct join_plan_dispatch<Response, tl<Base, Targets...>>
    {
        using type = typename join_plan_with_base<Response, Base, Targets...>::type;
    };

    template <class Response>
    using join_plan_t =
        typename join_plan_dispatch<Response, distinct_tables_t<Response>>::type;

    // base table of a Response (first selected field's table)
    template <class Tables>
    struct first_table
    {
        using type = void;
    };
    template <class B, class... R>
    struct first_table<tl<B, R...>>
    {
        using type = B;
    };
    template <class Response>
    using base_table_t = typename first_table<distinct_tables_t<Response>>::type;

    // how many distinct tables the Response spans
    template <class Tables>
    struct tl_size : std::integral_constant<std::size_t, 0>
    {
    };
    template <class... Ts>
    struct tl_size<tl<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)>
    {
    };
    template <class Response>
    inline constexpr bool is_multi_table = (tl_size<distinct_tables_t<Response>>::value > 1);

} // namespace orm::detail
