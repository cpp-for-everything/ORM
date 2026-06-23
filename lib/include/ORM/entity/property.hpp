#pragma once
#include "ORM/details/string_literal.hpp"
#include <string_view>
#include <type_traits>
#include <utility>

namespace orm {

    // ── property<CppType, "column_name"> ──────────────────────────────────────
    // Scalar entity field. Maps a C++ type to a named database column.
    // On C++26 (ORM_HAS_REFLECTION=1): string arg is optional — inferred from
    //   the field identifier via std::meta::name_v.
    // On PFR path (ORM_HAS_REFLECTION=0): string arg is mandatory.
    template <typename T, detail::string_literal Name>
    struct property
    {
        using value_type = T;

        [[nodiscard]] static constexpr std::string_view column_name() noexcept
        {
            return Name.view();
        }

        // Storage. `engaged` distinguishes "a value was set" from "unset" so that
        // partial result hydration can leave unselected columns empty (relationship-
        // aware SELECT returns partial entities). Constructing with a value engages it;
        // a default-constructed property is unset. `value` stays public for the
        // existing direct-access call sites.
        T    value{};
        bool engaged = false;

        constexpr property() = default;
        explicit constexpr property(T v) : value(std::move(v)), engaged(true) {}

        // ── std::optional-like API (no nested std::optional needed) ────────────
        [[nodiscard]] constexpr bool has_value() const noexcept { return engaged; }
        constexpr explicit operator bool() const noexcept { return engaged; }
        constexpr void reset() noexcept { value = T{}; engaged = false; }
        constexpr property& set(T v) { value = std::move(v); engaged = true; return *this; }
        [[nodiscard]] constexpr const T& get() const noexcept { return value; }
        [[nodiscard]] constexpr T&       get()       noexcept { return value; }

        constexpr bool operator==(const property&) const noexcept = default;
    };

    // ── is_property trait ──────────────────────────────────────────────────────
    template <typename T>
    struct is_property_trait : std::false_type
    {
    };

    template <typename T, detail::string_literal N>
    struct is_property_trait<property<T, N>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_property_v = is_property_trait<T>::value;

    template <typename T>
    concept is_property = is_property_v<T>;

} // namespace orm
