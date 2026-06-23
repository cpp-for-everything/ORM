#pragma once
#include "ORM/details/string_literal.hpp"
#include "ORM/query/field.hpp"
#include "ORM/query/placeholders.hpp"
#include <concepts>
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

    // ── Type compatibility for field comparisons ──────────────────────────────
    // Both operand C++ types are known at compile time, so an incompatible
    // comparison (e.g. an `int` column against a `std::u8string`) is rejected by
    // the compiler instead of producing a malformed query at runtime.
    namespace detail {

        // Unwrap property<T, Name> → T; leave any other type unchanged.
        template <typename V>
        struct unwrap_property
        {
            using type = V;
        };
        template <typename T, string_literal N>
        struct unwrap_property<property<T, N>>
        {
            using type = T;
        };
        template <typename V>
        using unwrap_property_t = typename unwrap_property<V>::type;

        // Logical C++ value type of a comparison operand:
        //   field       → the column's C++ type (property unwrapped)
        //   placeholder → its bound value_type
        //   literal     → the literal's own type (cvref-stripped)
        template <typename Operand>
        consteval auto operand_value_type()
        {
            if constexpr (is_field<Operand>)
                return std::type_identity<
                    unwrap_property_t<typename Operand::value_type>>{};
            else if constexpr (is_placeholder_v<Operand>)
                return std::type_identity<typename Operand::value_type>{};
            else
                return std::type_identity<std::remove_cvref_t<Operand>>{};
        }
        template <typename Operand>
        using operand_value_type_t =
            typename decltype(operand_value_type<Operand>())::type;

        // A field may be compared with a value when the two C++ types are the
        // same or convertible in either direction (e.g. a `double` column vs an
        // `int` literal). Genuine mismatches (numeric vs string) are rejected.
        template <typename L, typename R>
        inline constexpr bool comparable_value_types =
            std::same_as<L, R> || std::convertible_to<R, L> ||
            std::convertible_to<L, R>;

    } // namespace detail

    // ── Operator overloads for mem_ptr ────────────────────────────────────────
#define ORM_FIELD_OP(sym, str)                                                      \
    template <auto P, typename T>                                                   \
        requires(!std::is_same_v<T, std::nullptr_t>)                               \
    [[nodiscard]] constexpr auto operator sym(mem_ptr<P> /*a*/, T x)               \
    {                                                                               \
        static_assert(                                                              \
            detail::comparable_value_types<                                         \
                detail::operand_value_type_t<mem_ptr<P>>,                           \
                detail::operand_value_type_t<T>>,                                   \
            "orm: incompatible operand types in a field comparison. The "          \
            "right-hand value's C++ type is not convertible to (or from) the "      \
            "column's C++ type. Check the literal/placeholder type against the "    \
            "property<> declaration of the field.");                               \
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
