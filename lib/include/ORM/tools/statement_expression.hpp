#pragma once

#include <ORM/forward-decl.hpp>
#include <ORM/utils/expression_operators.hpp>
#include <ORM/tools/alias.hpp>

namespace webframe::ORM
{
	namespace details
	{
		class IExpression
		{
		};
		class IConstant : public IExpression
		{
		};

		template <typename T>
		concept is_expression = std::is_base_of_v<IExpression, T>;

		template <typename T>
		concept is_constant = std::is_base_of_v<IConstant, T>;

		template <typename T>
		concept is_just_expression = is_expression<T> && !is_constant<T>;
	} // namespace details

	template <typename T1, details::expression_operators op, typename T2> class Expression;

	template <typename T> class Constant : public details::IConstant
	{
		public:
		T a;
		using operand1_t = T;

		constexpr Constant(T x) : a(x)
		{
		}

		template <typename Y> constexpr Constant(Constant<Y> x) : a(x.a)
		{
		}

		constexpr operator T&()
		{
			return a;
		}

		template <typename T3, details::expression_operators op2, typename T4> constexpr auto operator+(Expression<T3, op2, T4>) const;

		template <typename T3, details::expression_operators op2, typename T4> constexpr auto operator-(Expression<T3, op2, T4>) const;

		template <typename T3, details::expression_operators op2, typename T4> constexpr auto operator*(Expression<T3, op2, T4>) const;

		template <typename T3, details::expression_operators op2, typename T4> constexpr auto operator/(Expression<T3, op2, T4>) const;

		template <typename T3, details::expression_operators op2, typename T4> constexpr auto operator%(Expression<T3, op2, T4>) const;

		template <typename Y>
			requires requires(T a, Y b) { a + b; }
		constexpr auto operator+(Y) const;

		template <typename Y>
			requires requires(T a, Y b) { a - b; }
		constexpr auto operator-(Y) const;

		template <typename Y>
			requires requires(T a, Y b) { a* b; }
		constexpr auto operator*(Y) const;

		template <typename Y>
			requires requires(T a, Y b) { a / b; }
		constexpr auto operator/(Y) const;

		template <typename Y>
			requires requires(T a, Y b) { a % b; }
		constexpr auto operator%(Y) const;
	};

	constexpr Constant<unsigned long long> operator"" _c(unsigned long long x)
	{
		return x;
	}
	constexpr Constant<long double> operator"" _c(long double x)
	{
		return x;
	}
	constexpr Constant<char> operator"" _c(char x)
	{
		return x;
	}
	constexpr Constant<wchar_t> operator"" _c(wchar_t x)
	{
		return x;
	}
	constexpr Constant<char16_t> operator"" _c(char16_t x)
	{
		return x;
	}
	constexpr Constant<char32_t> operator"" _c(char32_t x)
	{
		return x;
	}
	constexpr Constant<std::string_view> operator"" _c(const char* x)
	{
		return std::string_view(x);
	}

	template <typename T1, details::expression_operators op, typename T2> class Expression : public details::IExpression
	{
		public:
		T1 a;
		T2 b;

		using operand1_t = T1;
		static constexpr details::expression_operators operation = op;
		using operand2_t = T2;

		constexpr Expression(T1 x, T2 y) : a(x), b(y)
		{
		}

		template <details::expression_operators op2> constexpr Expression(Expression<T1, op2, T2> x) : a(x.a), b(x.b)
		{
		}

		template <details::string_literal name> static constexpr auto as = alias<Expression<T1, op, T2>, name>();

		template <typename T3, details::expression_operators op2, typename T4> constexpr auto operator+(Expression<T3, op2, T4>) const;

		template <typename T3, details::expression_operators op2, typename T4> constexpr auto operator-(Expression<T3, op2, T4>) const;

		template <typename T3, details::expression_operators op2, typename T4> constexpr auto operator*(Expression<T3, op2, T4>) const;

		template <typename T3, details::expression_operators op2, typename T4> constexpr auto operator/(Expression<T3, op2, T4>) const;

		template <typename T3, details::expression_operators op2, typename T4> constexpr auto operator%(Expression<T3, op2, T4>) const;

		template <typename T> constexpr auto operator+(Constant<T>) const;

		template <typename T> constexpr auto operator-(Constant<T>) const;

		template <typename T> constexpr auto operator*(Constant<T>) const;

		template <typename T> constexpr auto operator/(Constant<T>) const;

		template <typename T> constexpr auto operator%(Constant<T>) const;
	};

} // namespace webframe::ORM

#include <ORM/tools/statement_expression.cpp>
