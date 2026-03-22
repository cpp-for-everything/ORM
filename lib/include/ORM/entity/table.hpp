#pragma once
#include <string_view>
#include <type_traits>
#include <boost/pfr.hpp>
#include "ORM/entity/property.hpp"

namespace orm {

    // ── table_name_trait<T> ───────────────────────────────────────────────────
    // Specialize to provide a custom table name for an entity type.
    // Default: empty (users must specialize or rely on C++26 reflection).
    template <typename T>
    struct table_name_trait
    {
        static constexpr std::string_view value = "";
    };

    // ── table_name<T>() ───────────────────────────────────────────────────────
    template <typename T>
    [[nodiscard]] constexpr std::string_view table_name()
    {
        return table_name_trait<T>::value;
    }

    // ── is_entity concept ─────────────────────────────────────────────────────
    // An entity is an aggregate struct with at least one member accessible via PFR.
    template <typename T>
    concept is_entity = std::is_aggregate_v<T> &&
        (boost::pfr::tuple_size_v<T> > 0);

} // namespace orm
