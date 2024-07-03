#pragma once

#include <ORM/forward-decl.hpp>
#include <ORM/utils/expression_operators.hpp>

namespace webframe::ORM
{
    namespace details
    {
        class IExpression {};
        class IConstant : public IExpression {};
        
        template<typename T>
        concept is_expression = std::is_base_of_v<IExpression, T>;

        template<typename T>
        concept is_constant = std::is_base_of_v<IConstant, T>;
    }

    template<typename T1, details::expression_operators op, typename T2>
    class Expression;

    template<typename T>
    class Constant : public details::IConstant
    {
    public:
        T a;
        using operand1_t = T;

        constexpr Constant(T x) : a(x) { }
        
        template<typename Y>
        constexpr Constant(Constant<Y> x) : a(x.a) { }

        template<typename T3, details::expression_operators op2, typename T4>
        constexpr auto operator+ (Expression<T3, op2, T4>) const;

        template<typename T3, details::expression_operators op2, typename T4>
        constexpr auto operator- (Expression<T3, op2, T4>) const;

        template<typename T3, details::expression_operators op2, typename T4>
        constexpr auto operator* (Expression<T3, op2, T4>) const;

        template<typename T3, details::expression_operators op2, typename T4>
        constexpr auto operator/ (Expression<T3, op2, T4>) const;

        template<typename T3, details::expression_operators op2, typename T4>
        constexpr auto operator% (Expression<T3, op2, T4>) const;        

        template<typename Y>
        constexpr auto operator+ (Constant<Y>) const;

        template<typename Y>
        constexpr auto operator- (Constant<Y>) const;

        template<typename Y>
        constexpr auto operator* (Constant<Y>) const;

        template<typename Y>
        constexpr auto operator/ (Constant<Y>) const;

        template<typename Y>
        constexpr auto operator% (Constant<Y>) const;
    };

    template<typename T1, details::expression_operators op, typename T2>
    class Expression : public details::IExpression
    {
    public:
        T1 a;
        T2 b;
        
        using operand1_t = T1;
        static constexpr details::expression_operators operation = op;
        using operand2_t = T2;

        constexpr Expression(T1 x, T2 y) : a(x), b(y) { }
        
        template<details::expression_operators op2>
        constexpr Expression(Expression<T1, op2, T2> x) : a(x.a), b(x.b) { }

        template<typename T3, details::expression_operators op2, typename T4>
        constexpr auto operator+ (Expression<T3, op2, T4>) const;

        template<typename T3, details::expression_operators op2, typename T4>
        constexpr auto operator- (Expression<T3, op2, T4>) const;

        template<typename T3, details::expression_operators op2, typename T4>
        constexpr auto operator* (Expression<T3, op2, T4>) const;

        template<typename T3, details::expression_operators op2, typename T4>
        constexpr auto operator/ (Expression<T3, op2, T4>) const;

        template<typename T3, details::expression_operators op2, typename T4>
        constexpr auto operator% (Expression<T3, op2, T4>) const;        

        template<typename T>
        constexpr auto operator+ (Constant<T>) const;

        template<typename T>
        constexpr auto operator- (Constant<T>) const;

        template<typename T>
        constexpr auto operator* (Constant<T>) const;

        template<typename T>
        constexpr auto operator/ (Constant<T>) const;

        template<typename T>
        constexpr auto operator% (Constant<T>) const;
    };

} // namespace webframe::ORM

#include <ORM/tools/statement_expression.cpp>
