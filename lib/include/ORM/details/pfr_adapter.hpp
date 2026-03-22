#pragma once
#include <boost/pfr.hpp>
#include <string_view>
#include <cstddef>

namespace orm::detail::pfr {

    template <typename T, std::size_t I>
    [[nodiscard]] constexpr std::string_view field_name()
    {
        return boost::pfr::get_name<I, T>();
    }

} // namespace orm::detail::pfr
