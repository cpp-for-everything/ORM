#pragma once
#include "ORM/details/string_literal.hpp"
#include "ORM/query/field.hpp"
#include "ORM/query/placeholders.hpp"
#include <type_traits>
#include <string_view>

namespace orm {

    struct IRule
    {
    };

    template <typename T1, detail::string_literal Op, typename T2>
    struct Rule : IRule
    {
        static constexpr std::string_view operation{Op};
        T1 lhs_;
        T2 rhs_;

        constexpr Rule() = default;
        constexpr Rule(T1 a, T2 b) : lhs_(std::move(a)), rhs_(std::move(b)) {}
    };

    template <typename T>
    concept is_rule = std::derived_from<T, IRule>;

    // ── Negation ──────────────────────────────────────────────────────────────
    namespace detail {
        template <string_literal Op>
        consteval auto negated_op()
        {
            constexpr std::string_view s = Op;
            if constexpr (s == "==")  return string_literal("!=");
            else if constexpr (s == "!=")  return string_literal("==");
            else if constexpr (s == ">")   return string_literal("<=");
            else if constexpr (s == "<")   return string_literal(">=");
            else if constexpr (s == ">=")  return string_literal("<");
            else if constexpr (s == "<=")  return string_literal(">");
            else if constexpr (s == "&&")  return string_literal("||");
            else                           return string_literal("&&");
        }
    } // namespace detail

    template <typename T1, detail::string_literal Op, typename T2>
    [[nodiscard]] constexpr auto operator!(const Rule<T1, Op, T2>& r)
    {
        return Rule<T1, detail::negated_op<Op>(), T2>(r.lhs_, r.rhs_);
    }

    // ── Operator overloads for mem_ptr ────────────────────────────────────────
#define ORM_FIELD_OP(sym, str)                                                      \
    template <auto P, typename T>                                                   \
        requires(!std::is_same_v<T, std::nullptr_t>)                               \
    [[nodiscard]] constexpr auto operator sym(mem_ptr<P> /*a*/, T x)               \
    {                                                                               \
        if constexpr (is_field<T>)                                                  \
            return Rule<decltype(P), str, decltype(T::get())>(P, T::get());         \
        else                                                                        \
            return Rule<decltype(P), str, T>(P, x);                                \
    }

    ORM_FIELD_OP(==, "==")
    ORM_FIELD_OP(!=, "!=")
    ORM_FIELD_OP(< , "<" )
    ORM_FIELD_OP(> , ">" )
    ORM_FIELD_OP(<=, "<=")
    ORM_FIELD_OP(>=, ">=")
#undef ORM_FIELD_OP

    template <auto P>
    [[nodiscard]] constexpr auto operator==(mem_ptr<P> /*a*/, std::nullptr_t)
    {
        return Rule<decltype(P), "==", std::nullptr_t>(P, nullptr);
    }

    template <auto P>
    [[nodiscard]] constexpr auto operator!=(mem_ptr<P> /*a*/, std::nullptr_t)
    {
        return Rule<decltype(P), "!=", std::nullptr_t>(P, nullptr);
    }

    // ── Logical combination of rules ──────────────────────────────────────────
    template <typename T1, detail::string_literal Op1, typename T2,
              typename U1, detail::string_literal Op2, typename U2>
    [[nodiscard]] constexpr auto operator&&(Rule<T1, Op1, T2> a, Rule<U1, Op2, U2> b)
    {
        return Rule<Rule<T1, Op1, T2>, "&&", Rule<U1, Op2, U2>>(a, b);
    }

    template <typename T1, detail::string_literal Op1, typename T2,
              typename U1, detail::string_literal Op2, typename U2>
    [[nodiscard]] constexpr auto operator||(Rule<T1, Op1, T2> a, Rule<U1, Op2, U2> b)
    {
        return Rule<Rule<T1, Op1, T2>, "||", Rule<U1, Op2, U2>>(a, b);
    }

} // namespace orm
