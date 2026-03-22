#pragma once
#include <array>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <algorithm>

namespace orm {

    // ── fixed_string<N> ───────────────────────────────────────────────────────
    // Fixed-length UTF-8 string wrapper. Maps to SQL CHAR(N).
    template <std::size_t N>
    struct fixed_string
    {
        static constexpr std::size_t max_size = N;

        constexpr fixed_string() noexcept { data_.fill(0); }

        explicit constexpr fixed_string(std::u8string_view sv) noexcept
        {
            data_.fill(0);
            len_ = std::min(sv.size(), N);
            std::copy_n(sv.data(), len_, data_.data());
        }

        [[nodiscard]] constexpr std::size_t size() const noexcept { return len_; }

        [[nodiscard]] constexpr std::u8string_view as_u8string_view() const noexcept
        {
            return {data_.data(), len_};
        }

        constexpr bool operator==(const fixed_string&) const noexcept = default;

    private:
        std::array<char8_t, N> data_{};
        std::size_t len_{0};
    };

    // ── binary<N> ─────────────────────────────────────────────────────────────
    // Fixed-size binary buffer. Maps to SQL BINARY(N).
    template <std::size_t N>
    struct binary
    {
        static constexpr std::size_t fixed_size = N;

        constexpr binary() noexcept { data_.fill(0); }

        explicit constexpr binary(const std::array<uint8_t, N>& arr) noexcept : data_(arr) {}

        explicit constexpr binary(std::span<const uint8_t, N> s) noexcept
        {
            std::copy_n(s.data(), N, data_.data());
        }

        [[nodiscard]] constexpr std::size_t size() const noexcept { return N; }
        [[nodiscard]] constexpr const uint8_t* data() const noexcept { return data_.data(); }
        [[nodiscard]] constexpr uint8_t* data() noexcept { return data_.data(); }

        constexpr bool operator==(const binary&) const noexcept = default;

    private:
        std::array<uint8_t, N> data_{};
    };

    // ── varbinary<N> ──────────────────────────────────────────────────────────
    // Variable-size binary up to N bytes. Maps to SQL VARBINARY(N).
    template <std::size_t N>
    struct varbinary
    {
        static constexpr std::size_t max_size = N;

        varbinary() = default;

        explicit varbinary(std::span<const uint8_t> s)
        {
            auto len = std::min(s.size(), N);
            data_.assign(s.data(), s.data() + len);
        }

        explicit varbinary(const std::vector<uint8_t>& v)
        {
            auto len = std::min(v.size(), N);
            data_.assign(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(len));
        }

        [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }
        [[nodiscard]] const uint8_t* data() const noexcept { return data_.data(); }
        [[nodiscard]] uint8_t* data() noexcept { return data_.data(); }

        bool operator==(const varbinary&) const noexcept = default;

    private:
        std::vector<uint8_t> data_;
    };

} // namespace orm
