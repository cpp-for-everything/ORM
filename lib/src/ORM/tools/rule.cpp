#include <ORM/tools/rule.hpp>

namespace webframe::ORM
{
	template <typename T1, details::rule_operators op, typename T2> constexpr auto Rule<T1, op, T2>::operator!() const
	{
		if constexpr (op == details::Less)
		{
			return Rule<T1, details::Greater_or_equal, T2>(*this);
		}

		if constexpr (op == details::Greater)
		{
			return Rule<T1, details::Less_or_equal, T2>(*this);
		}

		if constexpr (op == details::Equals)
		{
			return Rule<T1, details::Not_equal, T2>(*this);
		}

		if constexpr (op == details::Not_equal)
		{
			return Rule<T1, details::Equals, T2>(*this);
		}

		if constexpr (op == details::Greater_or_equal)
		{
			return Rule<T1, details::Less, T2>(*this);
		}

		if constexpr (op == details::Less_or_equal)
		{
			return Rule<T1, details::Greater, T2>(*this);
		}

		if constexpr (op == details::And)
		{
			using notT1 = decltype(!this->a);
			using notT2 = decltype(!this->b);
			return Rule<notT1, details::Or, notT2>(!this->a, !this->b);
		}

		if constexpr (op == details::Or)
		{
			using notT1 = decltype(!this->a);
			using notT2 = decltype(!this->b);
			return Rule<notT1, details::And, notT2>(!this->a, !this->b);
		}

		if constexpr (op == details::Xor)
		{
			// !(a ^ b) = ((a && b) || (!a && !b))
			using notT1 = decltype(!this->a);
			using notT2 = decltype(!this->b);
			return Rule<Rule<T1, details::And, T2>, details::Or, Rule<notT1, details::And, notT2>>((this->a && this->b), (!this->a && !this->b));
		}
	}

	template <typename T1, details::rule_operators op, typename T2>
	template <typename T3, details::rule_operators op2, typename T4>
	constexpr auto Rule<T1, op, T2>::operator&&(Rule<T3, op2, T4> rule2) const
	{
		return Rule<Rule<T1, op, T2>, details::And, Rule<T3, op2, T4>>(*this, rule2);
	}

	template <typename T1, details::rule_operators op, typename T2>
	template <typename T3, details::rule_operators op2, typename T4>
	constexpr auto Rule<T1, op, T2>::operator||(Rule<T3, op2, T4> rule2) const
	{
		return Rule<Rule<T1, op, T2>, details::Or, Rule<T3, op2, T4>>(*this, rule2);
	}

	template <typename T1, details::rule_operators op, typename T2>
	template <typename T3, details::rule_operators op2, typename T4>
	constexpr auto Rule<T1, op, T2>::operator^(Rule<T3, op2, T4> rule2) const
	{
		return Rule<Rule<T1, op, T2>, details::Xor, Rule<T3, op2, T4>>(*this, rule2);
	}
} // namespace webframe::ORM
