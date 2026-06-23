#pragma once
#include "ORM/details/member_pointer.hpp"
#include "ORM/details/orm_tuple.hpp"
#include "ORM/entity/property.hpp"
#include <string_view>
#include <tuple>
#include <type_traits>

namespace orm {

    // ── mem_ptr<Ptr> ──────────────────────────────────────────────────────────
    // Zero-overhead wrapper around a member pointer enabling operator overloads
    // to produce typed Rule<> expression objects.
    template <auto Ptr>
    struct mem_ptr
    {
        using table_type = typename detail::i_mem_ptr<Ptr>::table_type;
        using value_type = typename detail::i_mem_ptr<Ptr>::value_type;
        using ptr_t      = typename detail::i_mem_ptr<Ptr>::ptr_t;

        // Whether selecting this field allows its row to be absent (drives LEFT/RIGHT/FULL).
        static constexpr bool optional = false;

        [[nodiscard]] static constexpr ptr_t get() noexcept { return Ptr; }

        // The member's column name. Works for both `property<T,Name>` and the
        // reference form of `relationship<>` (which doubles as an FK column);
        // both expose a static `column_name()`.
        [[nodiscard]] static constexpr std::string_view column_name() noexcept
        {
            if constexpr (requires { value_type::column_name(); })
                return value_type::column_name();
            else
                return {};
        }
    };

    // ── field<&T::m> factory variable template ─────────────────────────────
    // Usage: field<&User::id>
    template <auto Ptr>
    inline constexpr mem_ptr<Ptr> field{};

    // ── optional_field<&T::m> — same column, marked optional ───────────────────
    // A table all of whose selected columns are optional may be absent in a join
    // (→ LEFT/RIGHT/FULL); see query/join_infer.hpp.
    template <auto Ptr>
    struct opt_mem_ptr : mem_ptr<Ptr>
    {
        static constexpr bool optional = true;
    };

    template <auto Ptr>
    inline constexpr opt_mem_ptr<Ptr> optional_field{};

    // ── is_field concept ───────────────────────────────────────────────────────
    template <typename T>
    concept is_field = detail::is_mem_ptr_t<T>;

    // ── projected_type<FieldTuple> ────────────────────────────────────────────
    // Maps an orm_tuple<mem_ptr<Ptr>...> to std::tuple<CppType...>.
    // Each mem_ptr<Ptr>::value_type is property<T,Name>; we unwrap to T.
    namespace detail {

        template <typename MemPtrTag>
        struct field_cpp_type
        {
            using type = typename MemPtrTag::value_type::value_type;
        };

        template <typename FieldTuple>
        struct projected_type_impl;

        template <typename... Fields>
        struct projected_type_impl<orm_tuple<Fields...>>
        {
            using type = std::tuple<typename field_cpp_type<Fields>::type...>;
        };

    } // namespace detail

    template <typename FieldTuple>
    using projected_type = typename detail::projected_type_impl<FieldTuple>::type;

} // namespace orm
