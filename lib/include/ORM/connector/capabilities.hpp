#pragma once
#include <type_traits>

namespace orm {

    // ── connector_trait<DB> primary template ──────────────────────────────────
    // Must be specialised by every connector author.
    // An unspecialized instantiation is a hard compile error.
    template <typename DB>
    struct connector_trait
    {
        static_assert(sizeof(DB) == 0,
            "orm::connector_trait<DB> has not been specialised for this DB type. "
            "Provide a `template<> struct orm::connector_trait<YourDB> { ... };` "
            "specialisation. See docs/superpowers/specs/2026-03-28-orm-v2-architecture.md §2.");
    };

    // ── Capability tag structs ────────────────────────────────────────────────
    // Presence of a nested `using tag = void;` in connector_trait<DB> = supported.
    namespace cap {

        struct supports_joins {};
        struct supports_transactions {};
        struct supports_aggregation {};
        struct supports_embedding {};
        struct supports_upsert {};
        struct supports_bulk_insert {};
        struct supports_async {};

    } // namespace cap

    // ── has_capability<DB, Cap> ───────────────────────────────────────────────
    namespace detail {

        template <typename DB, typename Cap>
        struct capability_check : std::false_type {};

        template <typename DB>
        struct capability_check<DB, cap::supports_joins>
            : std::bool_constant<requires { typename connector_trait<DB>::supports_joins; }> {};

        template <typename DB>
        struct capability_check<DB, cap::supports_transactions>
            : std::bool_constant<requires { typename connector_trait<DB>::supports_transactions; }> {};

        template <typename DB>
        struct capability_check<DB, cap::supports_aggregation>
            : std::bool_constant<requires { typename connector_trait<DB>::supports_aggregation; }> {};

        template <typename DB>
        struct capability_check<DB, cap::supports_embedding>
            : std::bool_constant<requires { typename connector_trait<DB>::supports_embedding; }> {};

        template <typename DB>
        struct capability_check<DB, cap::supports_upsert>
            : std::bool_constant<requires { typename connector_trait<DB>::supports_upsert; }> {};

        template <typename DB>
        struct capability_check<DB, cap::supports_bulk_insert>
            : std::bool_constant<requires { typename connector_trait<DB>::supports_bulk_insert; }> {};

        template <typename DB>
        struct capability_check<DB, cap::supports_async>
            : std::bool_constant<requires { typename connector_trait<DB>::supports_async; }> {};

    } // namespace detail

    template <typename DB, typename Cap>
    inline constexpr bool has_capability = detail::capability_check<DB, Cap>::value;

} // namespace orm
