#pragma once
#include <string_view>
#include <type_traits>

namespace orm::detail {

    template <auto MemberPtr>
    struct i_mem_ptr;

    template <typename T, typename Class, T Class::* Ptr>
    struct i_mem_ptr<Ptr>
    {
        using table_type = Class;
        using value_type = T;
        using ptr_t = T Class::*;
    };

    template <typename T>
    concept is_mem_ptr_t = requires {
        typename T::table_type;
        typename T::value_type;
        typename T::ptr_t;
    };

    // ── is_raw_mem_ptr<T> ─────────────────────────────────────────────────────
    // Detects raw member-pointer types (Prop Class::*) as stored in Rule<T1,...>
    // by ORM_FIELD_OP — T1 = decltype(MemberPtr) = Prop Class::*.
    template <typename T>
    struct is_raw_mem_ptr_impl : std::false_type {};

    template <typename Prop, typename Class>
    struct is_raw_mem_ptr_impl<Prop Class::*> : std::true_type
    {
        using value_t = Prop;
    };

    template <typename T>
    inline constexpr bool is_raw_mem_ptr = is_raw_mem_ptr_impl<T>::value;

    // ── column_name_of<T> ──────────────────────────────────────────────────────
    // Returns the column_name() of the property pointed to by a raw member pointer.
    // Requires is_raw_mem_ptr<T> && is_property_v<typename is_raw_mem_ptr_impl<T>::value_t>.
    template <typename T>
    [[nodiscard]] constexpr std::string_view column_name_of() noexcept
        requires (is_raw_mem_ptr<T>)
    {
        using Prop = typename is_raw_mem_ptr_impl<T>::value_t;
        if constexpr (requires { Prop::column_name(); })
            return Prop::column_name();
        else
            return {};
    }

} // namespace orm::detail
