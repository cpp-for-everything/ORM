#pragma once
#include <functional>
#include <type_traits>

namespace orm {

    // ── Anonymous placeholder: Placeholder<T>
    // ── Indexed placeholder:   Placeholder<T, std::placeholders::_N>
    //
    // The indexed form binds to the N-th argument of db.execute().
    // The same index can appear multiple times in a query to reuse one argument.
    // Mixing anonymous and indexed placeholders in the same query is forbidden.

    template <typename T>
    struct Placeholder
    {
        using value_type = T;
    };

    template <typename T, auto N>
        requires (std::is_placeholder<decltype(N)>::value > 0)
    struct IndexedPlaceholder
    {
        using value_type = T;
        static constexpr int index = std::is_placeholder<decltype(N)>::value; // 1-based
    };

    // ── ph<T, std::placeholders::_N> — short variable template ───────────────
    template <typename T, auto N>
        requires (std::is_placeholder<decltype(N)>::value > 0)
    inline constexpr IndexedPlaceholder<T, N> ph{};

    // ── is_placeholder_trait ──────────────────────────────────────────────────
    template <typename T>
    struct is_placeholder_trait : std::false_type {};

    template <typename T>
    struct is_placeholder_trait<Placeholder<T>> : std::true_type {};

    template <typename T, auto N>
        requires (std::is_placeholder<decltype(N)>::value > 0)
    struct is_placeholder_trait<IndexedPlaceholder<T, N>> : std::true_type {};

    template <typename T>
    inline constexpr bool is_placeholder_v = is_placeholder_trait<T>::value;

    template <typename T>
    concept is_placeholder = is_placeholder_v<T>;

    // ── placeholder_index<T> — 0 for anonymous, 1-based N for indexed ─────────
    template <typename T>
    struct placeholder_index : std::integral_constant<int, 0> {};

    template <typename T, auto N>
        requires (std::is_placeholder<decltype(N)>::value > 0)
    struct placeholder_index<IndexedPlaceholder<T, N>>
        : std::integral_constant<int, std::is_placeholder<decltype(N)>::value> {};

    template <typename T>
    inline constexpr int placeholder_index_v = placeholder_index<T>::value;

} // namespace orm
