#pragma once
#include <array>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <set>
#include <cstddef>
#include <string_view>
#include <initializer_list>

namespace orm {

    namespace detail {

        template <std::size_t N>
        struct ct_string
        {
            char8_t data[N]{};

            constexpr ct_string(const char8_t (&s)[N]) noexcept
            {
                std::copy_n(s, N, data);
            }

            [[nodiscard]] constexpr std::u8string_view view() const noexcept
            {
                return {data, N - 1};
            }

            constexpr bool operator==(const ct_string&) const noexcept = default;
        };

    } // namespace detail

    // ── enum_t<"val1", "val2", ...> ───────────────────────────────────────────
    // Compile-time constrained string mapping to SQL ENUM(...).
    template <detail::ct_string... Values>
    struct enum_t
    {
        static constexpr std::array<std::u8string_view, sizeof...(Values)> allowed{
            Values.view()...};

        [[nodiscard]] static constexpr bool is_valid(std::u8string_view v) noexcept
        {
            return std::find(allowed.begin(), allowed.end(), v) != allowed.end();
        }

        explicit enum_t(std::u8string_view v)
        {
            if (!is_valid(v))
                throw std::invalid_argument("orm::enum_t: invalid value");
            value_ = std::u8string(v);
        }

        [[nodiscard]] std::u8string_view value() const noexcept { return value_; }

        bool operator==(const enum_t&) const noexcept = default;

    private:
        std::u8string value_;
    };

    // ── set_t<"val1", "val2", ...> ────────────────────────────────────────────
    // Compile-time constrained multi-value type mapping to SQL SET(...).
    template <detail::ct_string... Values>
    struct set_t
    {
        static constexpr std::array<std::u8string_view, sizeof...(Values)> allowed{
            Values.view()...};

        [[nodiscard]] static constexpr bool is_valid(std::u8string_view v) noexcept
        {
            return std::find(allowed.begin(), allowed.end(), v) != allowed.end();
        }

        explicit set_t(std::initializer_list<std::u8string_view> vals)
        {
            for (auto& v : vals)
            {
                if (!is_valid(v))
                    throw std::invalid_argument("orm::set_t: invalid value");
                values_.insert(std::u8string(v));
            }
        }

        [[nodiscard]] bool contains(std::u8string_view v) const
        {
            return values_.contains(std::u8string(v));
        }

        [[nodiscard]] const std::set<std::u8string>& values() const noexcept { return values_; }

        bool operator==(const set_t&) const noexcept = default;

    private:
        std::set<std::u8string> values_;
    };

} // namespace orm
