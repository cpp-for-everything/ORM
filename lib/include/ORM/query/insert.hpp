#pragma once
#include "ORM/query/field.hpp"
#include "ORM/details/orm_tuple.hpp"
#include <cstddef>
#include <string>
#include <type_traits>

namespace orm {

    struct insert_query_tag {};

    template <typename T>
    concept is_insert_query = std::derived_from<T, insert_query_tag>;

    // ── insert_query<Properties> ──────────────────────────────────────────────
    template <typename Properties>
    struct insert_query : insert_query_tag
    {
        using properties = Properties;

        Properties signature_;
        // Number of rows one INSERT carries. Only a connector that declares
        // supports_multi_row_insert reads it. Everything else sends one row.
        std::size_t rows_ = 1;

        explicit constexpr insert_query(Properties p) : signature_(std::move(p)) {}

        [[nodiscard]] constexpr Properties signature() const { return signature_; }

        [[nodiscard]] constexpr insert_query with_rows(std::size_t n) const
        {
            insert_query copy = *this;
            copy.rows_ = n;
            return copy;
        }
    };

    namespace detail {

        // "(?, ?), (?, ?)" for `columns` markers per row and `rows` rows.
        [[nodiscard]] inline std::string multi_row_placeholders(std::size_t columns, std::size_t rows)
        {
            std::string one = "(";
            for (std::size_t c = 0; c < columns; ++c)
                one += (c > 0 ? ", ?" : "?");
            one += ")";
            std::string out;
            for (std::size_t r = 0; r < rows; ++r)
                out += (r > 0 ? ", " : "") + one;
            return out;
        }

    } // namespace detail

    // ── insert(...) factory ───────────────────────────────────────────────────
    template <typename... Fields>
        requires (is_field<Fields> && ...)
    [[nodiscard]] constexpr auto insert(Fields... fields)
    {
        using Properties = detail::orm_tuple<Fields...>;
        return insert_query<Properties>{Properties{fields...}};
    }

} // namespace orm
