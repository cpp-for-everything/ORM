#pragma once

#include <ORM/forward-decl.hpp>
#include <ORM/utils/placeholder.hpp>
#include <ORM/utils/query_input_data.hpp>

namespace webframe::ORM
{
	template <typename T1, details::rule_operators op, typename T2> class Rule
	{
		public:
		T1 a;
		T2 b;

		using operand1_t = T1;
		static constexpr details::rule_operators operation = op;
		using operand2_t = T2;

		constexpr Rule(T1 x, T2 y) : a(x), b(y)
		{
		}

		template <details::rule_operators op2> constexpr Rule(Rule<T1, op2, T2> x) : a(x.a), b(x.b)
		{
		}

		constexpr auto operator!() const;

		template <typename T3, details::rule_operators op2, typename T4> constexpr auto operator&&(Rule<T3, op2, T4>) const;

		template <typename T3, details::rule_operators op2, typename T4> constexpr auto operator||(Rule<T3, op2, T4>) const;

		template <typename T3, details::rule_operators op2, typename T4> constexpr auto operator^(Rule<T3, op2, T4>) const;
	};

} // namespace webframe::ORM

#include <ORM/tools/rule.cpp>
