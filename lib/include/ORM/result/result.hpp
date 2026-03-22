#pragma once
#include "ORM/details/orm_tuple.hpp"
#include "ORM/query/field.hpp"
#include <vector>
#include <optional>
#include <tuple>
#include <cstddef>
#include <type_traits>
#include <string_view>
#include <utility>

namespace orm {

    // ── compile-time field tag → tuple index search ────────────────────────────
    namespace detail {

        // mem_ptr<Ptr> is a unique type per member pointer — use type identity.
        // Returns the index of TargetTag in [Fields...], or sizeof...(Fields) on miss.
        template <typename TargetTag, typename... Fields>
        consteval std::size_t find_field_index() noexcept
        {
            constexpr bool matches[] = {
                std::is_same_v<std::remove_cvref_t<Fields>, std::remove_cvref_t<TargetTag>>...
            };
            for (std::size_t i = 0; i < sizeof...(Fields); ++i)
                if (matches[i]) return i;
            return sizeof...(Fields);
        }

        template <typename TargetTag, typename OrmTupleT>
        struct field_index_in;

        template <typename TargetTag, typename... Fields>
        struct field_index_in<TargetTag, orm_tuple<Fields...>>
        {
            static constexpr std::size_t value = find_field_index<TargetTag, Fields...>();
        };

        // Passes Idx as a true template parameter to std::get<>.
        template <std::size_t Idx, typename Row>
        constexpr decltype(auto) get_at_index(const Row& row) noexcept
        {
            return std::get<Idx>(row);
        }

    } // namespace detail

    // ── orm::result<Row, FieldTuple> ──────────────────────────────────────────
    // Row        = std::tuple<CppType...>       materialised row type
    // FieldTuple = detail::orm_tuple<mem_ptr<>...>  carries column names
    //
    // FieldTuple = void  →  get<MemPtr>() disabled (INSERT/UPDATE/DELETE results)
    template <typename Row, typename FieldTuple = void>
    struct result
    {
        using value_type  = Row;
        using field_tuple = FieldTuple;

        result() = default;
        explicit result(std::vector<Row> rows) : rows_(std::move(rows)) {}

        [[nodiscard]] auto begin() const noexcept { return rows_.cbegin(); }
        [[nodiscard]] auto end()   const noexcept { return rows_.cend();   }
        [[nodiscard]] auto begin()       noexcept { return rows_.begin();  }
        [[nodiscard]] auto end()         noexcept { return rows_.end();    }

        [[nodiscard]] bool        empty() const noexcept { return rows_.empty(); }
        [[nodiscard]] std::size_t size()  const noexcept { return rows_.size(); }

        [[nodiscard]] std::vector<Row> to_vector() const& { return rows_; }
        [[nodiscard]] std::vector<Row> to_vector() &&     { return std::move(rows_); }

        // ── get<I>(row) — tuple element by positional index ───────────────────
        template <std::size_t I>
        [[nodiscard]] static constexpr decltype(auto) get(const Row& row) noexcept
            requires requires { std::get<I>(row); }
        {
            return std::get<I>(row);
        }

        // ── get_field<&Table::field>(row) — element by member pointer ──────────
        // Compile-time search: finds the index in FieldTuple whose column_name()
        // matches mem_ptr<MemPtr>::column_name(). Available only for SELECT.
        template <auto MemPtr>
        [[nodiscard]] static constexpr decltype(auto) get_field(const Row& row) noexcept
            requires (!std::is_void_v<FieldTuple>)
        {
            using Tag = mem_ptr<MemPtr>;
            static constexpr std::size_t idx = detail::field_index_in<Tag, FieldTuple>::value;
            static_assert(idx < std::tuple_size_v<Row>,
                "orm::result::get_field<&T::m>: field not found in this select() projection");
            return detail::get_at_index<idx>(row);
        }

    private:
        std::vector<Row> rows_;
    };

    // ── orm::optional_result<T> ───────────────────────────────────────────────
    template <typename T>
    struct optional_result
    {
        optional_result() = default;
        explicit optional_result(T val) : value_(std::move(val)) {}

        [[nodiscard]] bool has_value() const noexcept { return value_.has_value(); }
        explicit operator bool() const noexcept { return has_value(); }

        [[nodiscard]] T&       operator*()       { return *value_; }
        [[nodiscard]] const T& operator*() const { return *value_; }

        [[nodiscard]] T*       operator->()       { return &*value_; }
        [[nodiscard]] const T* operator->() const { return &*value_; }

    private:
        std::optional<T> value_;
    };

} // namespace orm
