#pragma once
#include "ORM/query/field.hpp"
#include "ORM/details/orm_tuple.hpp"
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

        explicit constexpr insert_query(Properties p) : signature_(std::move(p)) {}

        [[nodiscard]] constexpr Properties signature() const { return signature_; }
    };

    // ── insert(...) factory ───────────────────────────────────────────────────
    template <typename... Fields>
        requires (is_field<Fields> && ...)
    [[nodiscard]] constexpr auto insert(Fields... fields)
    {
        using Properties = detail::orm_tuple<Fields...>;
        return insert_query<Properties>{Properties{fields...}};
    }

} // namespace orm
