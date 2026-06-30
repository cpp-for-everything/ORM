#pragma once
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include <list>
#include "ORM/entity/property.hpp"
#include "ORM/details/member_pointer.hpp"

namespace orm {

    // ── store_as strategy tags ────────────────────────────────────────────────
    // `reference<&Target::pk>` — a foreign-key relationship that points at Target::pk.
    // `embed`                  — the related entity/collection lives inside the parent.
    namespace store_as {

        template <auto TargetPtr>
        struct reference
        {
            static constexpr auto target_ptr = TargetPtr;
        };

        struct embed
        {
        };

    } // namespace store_as

    // ── inferred_kind ─────────────────────────────────────────────────────────
    enum class inferred_kind
    {
        one_to_one,
        one_to_many,
    };

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
    inline constexpr inferred_kind infer_relationship_v = infer_relationship_trait<T>::value;

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

    // ── relationship<Strategy, "name", Related = void> ────────────────────────
    template <typename Strategy, detail::string_literal Name, typename Related = void>
    struct relationship;

    // ── reference form ────────────────────────────────────────────────────────
    // relationship<store_as::reference<&User::id>, "user_id">
    // Doubles as the FK column: column_name()="user_id", value_type = FK C++ type
    // (the target's underlying type), plus join metadata (target table/column).
    template <auto TargetPtr, detail::string_literal Name, typename Related>
    struct relationship<store_as::reference<TargetPtr>, Name, Related>
    {
        using strategy = store_as::reference<TargetPtr>;
        static constexpr bool is_reference = true;
        static constexpr bool is_embed     = false;

        using target_table    = typename detail::i_mem_ptr<TargetPtr>::table_type;    // User
        using target_property = typename detail::i_mem_ptr<TargetPtr>::value_type;    // property<int,"id">
        using value_type      = typename target_property::value_type;                // int (FK type)

        static constexpr auto target_ptr = TargetPtr;

        [[nodiscard]] static constexpr std::string_view column_name() noexcept { return Name.view(); }
        [[nodiscard]] static constexpr std::string_view target_column() noexcept
        {
            return target_property::column_name();
        }

        value_type value{};
        constexpr relationship() = default;
        explicit constexpr relationship(value_type v) : value(std::move(v)) {}
        constexpr bool operator==(const relationship&) const noexcept = default;
    };

    // ── embed form ────────────────────────────────────────────────────────────
    // relationship<store_as::embed, "posts", std::vector<Post>>
    template <detail::string_literal Name, typename Related>
    struct relationship<store_as::embed, Name, Related>
    {
        using strategy = store_as::embed;
        static constexpr bool is_reference = false;
        static constexpr bool is_embed     = true;

        using related_type = Related;
        using element_type = typename element_type_trait<Related>::type;
        static constexpr bool is_collection =
            (infer_relationship_v<Related> == inferred_kind::one_to_many);

        [[nodiscard]] static constexpr std::string_view field_name() noexcept { return Name.view(); }

        Related value{};
        constexpr relationship() = default;
        constexpr bool operator==(const relationship&) const noexcept = default;
    };

    // ── traits ────────────────────────────────────────────────────────────────
    template <typename T>
    struct is_relationship_trait : std::false_type
    {
    };
    template <typename S, detail::string_literal N, typename R>
    struct is_relationship_trait<relationship<S, N, R>> : std::true_type
    {
    };
    template <typename T>
    inline constexpr bool is_relationship_v = is_relationship_trait<T>::value;

    template <typename T>
    struct is_reference_relationship_trait : std::false_type
    {
    };
    template <auto P, detail::string_literal N, typename R>
    struct is_reference_relationship_trait<relationship<store_as::reference<P>, N, R>>
        : std::true_type
    {
    };
    template <typename T>
    inline constexpr bool is_reference_relationship_v = is_reference_relationship_trait<T>::value;

    template <typename T>
    struct is_embed_relationship_trait : std::false_type
    {
    };
    template <detail::string_literal N, typename R>
    struct is_embed_relationship_trait<relationship<store_as::embed, N, R>> : std::true_type
    {
    };
    template <typename T>
    inline constexpr bool is_embed_relationship_v = is_embed_relationship_trait<T>::value;

} // namespace orm
