#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Result rows for relationship-aware SELECT.
//
//  A multi-table SELECT yields one entity per referenced table, each hydrated
//  PARTIALLY — only the selected columns are engaged (`property::has_value()`),
//  the rest stay unset. `joined_row<Es...>` bundles those entities; `get<E>()`
//  returns the E entity by reference.
//
//  `hydrate_entity<Entity, FieldsTuple>(values)` builds one partial entity from
//  the values of its selected columns (FieldsTuple = orm_tuple<mem_ptr<&E::m>...>).
// ─────────────────────────────────────────────────────────────────────────────
#include "ORM/query/field.hpp"
#include "ORM/query/join_infer.hpp"   // distinct_tables_t
#include "ORM/details/orm_tuple.hpp"
#include <cstddef>
#include <tuple>
#include <utility>

namespace orm {

    // ── joined_row<Entities...> ────────────────────────────────────────────────
    template <typename... Entities>
    struct joined_row
    {
        std::tuple<Entities...> entities_{};

        constexpr joined_row() = default;
        explicit constexpr joined_row(Entities... es) : entities_(std::move(es)...) {}

        template <typename E>
        [[nodiscard]] constexpr E& get() noexcept { return std::get<E>(entities_); }
        template <typename E>
        [[nodiscard]] constexpr const E& get() const noexcept { return std::get<E>(entities_); }

        template <std::size_t I>
        [[nodiscard]] constexpr auto&       get()       noexcept { return std::get<I>(entities_); }
        template <std::size_t I>
        [[nodiscard]] constexpr const auto& get() const noexcept { return std::get<I>(entities_); }

        static constexpr std::size_t size = sizeof...(Entities);
    };

    // ── partial-entity hydration ───────────────────────────────────────────────
    namespace detail {

        template <typename Entity, typename FieldsTuple, typename ValsTuple>
        struct entity_hydrator;

        template <typename Entity, typename... Fields, typename ValsTuple>
        struct entity_hydrator<Entity, orm_tuple<Fields...>, ValsTuple>
        {
            [[nodiscard]] static constexpr Entity run(const ValsTuple& vals)
            {
                Entity e{};
                [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    // Fields::get() is the member pointer &Entity::m to a property<T,N>
                    // member; .set() writes the value and marks it engaged.
                    (((e.*(Fields::get())).set(std::get<Is>(vals))), ...);
                }(std::make_index_sequence<sizeof...(Fields)>{});
                return e;
            }
        };

    } // namespace detail

    // FieldsTuple = orm_tuple<mem_ptr<&Entity::m>...> — this entity's selected fields,
    // in the same order as the values in `vals`.
    template <typename Entity, typename FieldsTuple, typename ValsTuple>
    [[nodiscard]] constexpr Entity hydrate_entity(const ValsTuple& vals)
    {
        return detail::entity_hydrator<Entity, FieldsTuple, ValsTuple>::run(vals);
    }

    // ── joined_row type for a multi-table Response ─────────────────────────────
    namespace detail {
        template <typename TL>
        struct tl_to_joined_row;
        template <typename... Ts>
        struct tl_to_joined_row<tl<Ts...>>
        {
            using type = joined_row<Ts...>;
        };
    } // namespace detail

    template <typename Response>
    using joined_row_for =
        typename detail::tl_to_joined_row<detail::distinct_tables_t<Response>>::type;

    // ── hydrate a whole joined row from the flat SELECT column values ──────────
    // The SELECT lists columns in Response order, so column i's value goes to the
    // member that Response field i points at, in the entity for its table_type.
    namespace detail {
        template <typename Row, typename Fields, typename ValsTuple>
        struct joined_hydrator;
        template <typename Row, typename... Fields, typename ValsTuple>
        struct joined_hydrator<Row, orm_tuple<Fields...>, ValsTuple>
        {
            [[nodiscard]] static Row run(const ValsTuple& vals)
            {
                Row row{};
                [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    ((( row.template get<typename Fields::table_type>()
                            .*(Fields::get()) ).set(std::get<Is>(vals))), ...);
                }(std::index_sequence_for<Fields...>{});
                return row;
            }
        };
    } // namespace detail

    template <typename Response, typename ValsTuple>
    [[nodiscard]] joined_row_for<Response> hydrate_joined(const ValsTuple& vals)
    {
        return detail::joined_hydrator<joined_row_for<Response>, Response, ValsTuple>::run(vals);
    }

} // namespace orm
