#pragma once
#include <string_view>
#include <type_traits>
#include <vector>
#include <list>
#include "ORM/entity/property.hpp"

namespace orm {

    // ── store_as enum ─────────────────────────────────────────────────────────
    enum class store_as
    {
        reference,  // FK join (SQL) or $lookup (NoSQL)
        embed,      // embedded document / denormalized JSON column
    };

    // ── inferred_kind ─────────────────────────────────────────────────────────
    enum class inferred_kind
    {
        one_to_one,
        one_to_many,
    };

    // ── infer_relationship_trait<T> ───────────────────────────────────────────
    template <typename T>
    struct infer_relationship_trait
    {
        static constexpr inferred_kind value = inferred_kind::one_to_one;
    };

    template <typename T, typename A>
    struct infer_relationship_trait<std::vector<T, A>>
    {
        static constexpr inferred_kind value = inferred_kind::one_to_many;
    };

    template <typename T, typename A>
    struct infer_relationship_trait<std::list<T, A>>
    {
        static constexpr inferred_kind value = inferred_kind::one_to_many;
    };

    template <typename T>
    inline constexpr inferred_kind infer_relationship_v =
        infer_relationship_trait<T>::value;

    // ── element_type_trait<T> ─────────────────────────────────────────────────
    template <typename T>
    struct element_type_trait
    {
        using type = T;
    };

    template <typename T, typename A>
    struct element_type_trait<std::vector<T, A>>
    {
        using type = T;
    };

    template <typename T, typename A>
    struct element_type_trait<std::list<T, A>>
    {
        using type = T;
    };

    // ── relationship<strategy, T, "name"> ────────────────────────────────────
    template <store_as Strategy, typename T, detail::string_literal Name>
    struct relationship
    {
        static constexpr store_as strategy = Strategy;
        using related_type = T;
        using element_type = typename element_type_trait<T>::type;
        static constexpr bool is_collection =
            (infer_relationship_v<T> == inferred_kind::one_to_many);

        [[nodiscard]] static constexpr std::string_view field_name() noexcept
        {
            return Name.view();
        }
    };

    // ── is_relationship trait ──────────────────────────────────────────────────
    template <typename T>
    struct is_relationship_trait : std::false_type
    {
    };

    template <store_as S, typename T, detail::string_literal N>
    struct is_relationship_trait<relationship<S, T, N>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_relationship_v = is_relationship_trait<T>::value;

} // namespace orm
