#pragma once
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace orm::detail {

    // Text form of a number for a connector that sends parameters as text.
    // Floating point uses std::to_chars: the shortest text that reads back as
    // the same value, and it does not depend on the locale. NaN and infinity
    // are rejected, because the text forms differ between servers.
    // Integers keep std::to_string.
    template <typename T>
        requires std::is_arithmetic_v<T>
    [[nodiscard]] std::string arithmetic_to_string(T v)
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            if (!std::isfinite(v))
                throw std::invalid_argument(
                    "orm: NaN and infinity cannot be sent as a parameter");
            char buf[64];
            const auto res = std::to_chars(buf, buf + sizeof buf, v);
            return std::string(buf, res.ptr);
        }
        else
            return std::to_string(v);
    }

} // namespace orm::detail
