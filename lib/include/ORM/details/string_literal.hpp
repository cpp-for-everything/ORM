#pragma once
#include <algorithm>
#include <string_view>
#include <cstddef>

namespace orm::detail {

template <std::size_t N>
struct string_literal {
    char data[N]{};

    constexpr string_literal(const char (&s)[N]) noexcept {
        std::copy_n(s, N, data);
    }

    constexpr operator std::string_view() const noexcept {
        return {data, N - 1};
    }

    constexpr std::string_view view() const noexcept {
        return {data, N - 1};
    }

    constexpr bool operator==(const string_literal&) const noexcept = default;

    template <std::size_t M>
    constexpr bool operator==(const string_literal<M>&) const noexcept {
        return false;
    }
};

} // namespace orm::detail
