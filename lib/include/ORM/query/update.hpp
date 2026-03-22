#pragma once
#include "ORM/query/field.hpp"
#include "ORM/query/rules.hpp"
#include "ORM/query/placeholders.hpp"
#include "ORM/details/orm_tuple.hpp"
#include <type_traits>

namespace orm {

    struct update_query_tag {};

    template <typename T>
    concept is_update_query = std::derived_from<T, update_query_tag>;

    // ── UpdateStatement stores the mem_ptr<Ptr> tag type so column_name() is accessible
    template <typename MemPtrTag, typename ValueT>
    struct UpdateStatement
    {
        using field_tag = MemPtrTag;
        ValueT value_;
    };

    // ── update_query<Table, Statements, Wheres> ───────────────────────────────
    template <
        typename Table,
        typename Statements = detail::orm_tuple<>,
        typename Wheres     = detail::orm_tuple<>
    >
    struct update_query : update_query_tag
    {
        using table_type = Table;
        using tables = detail::orm_tuple<Table>;

        Statements updates_;
        Wheres     wheres_;

        constexpr update_query() = default;
        constexpr update_query(Statements s, Wheres w)
            : updates_(std::move(s)), wheres_(std::move(w)) {}

        // .set(field<&T::m>, value)
        template <auto FieldPtr, typename ValueT>
        [[nodiscard]] constexpr auto set(mem_ptr<FieldPtr> /*f*/, ValueT v) const
        {
            using Tag  = mem_ptr<FieldPtr>;
            using Stmt = UpdateStatement<Tag, ValueT>;
            using NewStmts = detail::append_type_t<Statements, Stmt>;
            return update_query<Table, NewStmts, Wheres>(
                detail::tuple_cat(updates_, detail::orm_tuple<Stmt>(Stmt{std::move(v)})),
                wheres_);
        }

        // .where(rule)
        template <typename RuleType>
            requires is_rule<RuleType>
        [[nodiscard]] constexpr auto where(RuleType r) const
        {
            using NewWheres = detail::append_type_t<Wheres, RuleType>;
            return update_query<Table, Statements, NewWheres>(
                updates_,
                detail::tuple_cat(wheres_, detail::orm_tuple<RuleType>(r)));
        }

        [[nodiscard]] constexpr Statements updates() const { return updates_; }
        [[nodiscard]] constexpr Wheres     wheres()  const { return wheres_; }
    };

    // ── update<Table>() factory ───────────────────────────────────────────────
    template <typename Table>
    [[nodiscard]] constexpr auto update()
    {
        return update_query<Table>{};
    }

} // namespace orm
