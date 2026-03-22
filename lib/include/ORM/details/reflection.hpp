#pragma once
#include <string_view>
#include <cstddef>

#if ORM_HAS_REFLECTION
    #include <meta>
#else
    #include "ORM/details/pfr_adapter.hpp"
#endif

namespace orm::detail {

#if ORM_HAS_REFLECTION

    template <auto MemberPtr>
    [[nodiscard]] consteval std::string_view field_name_of()
    {
        return std::meta::name_v<^^[:std::meta::reflect_value(MemberPtr):]>;
    }

    template <typename T>
    [[nodiscard]] consteval std::string_view type_name_of()
    {
        return std::meta::name_v<^^T>;
    }

#else

    // PFR path: index lookup via address-of comparison in a constexpr context.
    // Each member pointer is matched against PFR's index by comparing offsets.
    template <auto MemberPtr>
    struct member_ptr_index_helper
    {
    private:
        using Table = typename decltype([]<typename T, typename C>(T C::*) -> i_mem_ptr<MemberPtr> {}(MemberPtr))::table_type;
        // We rely on PFR name lookup by index; the index is determined at
        // compile time by matching the member pointer against PFR field order.
        // Since C++ doesn't provide constexpr offsetof for arbitrary types,
        // we use __builtin_offsetof (GCC/Clang extension) via a helper.

        static constexpr std::size_t compute_index()
        {
            // Compare member pointer identity against each PFR-reflected member pointer.
            // This requires that the struct is an aggregate (standard-layout or simple aggregate).
            constexpr std::size_t n = boost::pfr::tuple_size_v<Table>;
            for (std::size_t i = 0; i < n; ++i) {
                // Not directly feasible without reflection; fallback:
                // Users must provide string arg in property<T, "name"> on PFR path.
                // This helper is only used when string is omitted — only valid on C++26 path.
                (void)i;
            }
            return 0; // unreachable on PFR path when string arg is provided
        }

    public:
        static constexpr std::size_t value = 0;
    };

    template <auto MemberPtr>
    [[nodiscard]] constexpr std::string_view field_name_of()
    {
        // On the PFR path, field_name_of<> is only called from property<>
        // when no string arg is given. Since that usage requires C++26,
        // this path is never reached in practice. Return empty to satisfy
        // the compiler on this translation unit.
        return {};
    }

    template <typename T>
    [[nodiscard]] constexpr std::string_view type_name_of()
    {
        // On PFR path, users must specialise table_name_trait<T>.
        return {};
    }

#endif

} // namespace orm::detail
