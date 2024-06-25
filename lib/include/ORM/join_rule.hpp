/**
 *  @file   join_rule.hpp
 *  @brief  Wrapper for rules extended with join funcitonality
 *  @author Alex Tsvetanov
 *  @date   2023-09-21
 ***********************************************/

#pragma once

#include "rules.hpp"
#include "CRUD/utils.hpp"

using namespace std::literals;

namespace webframe::ORM
{
	template <modes::join _mode, typename _Table, is_rule rule_t> class JoinRule : public rule_t
	{
		public:
		static constexpr auto mode = _mode;
		using Table = _Table;
		constexpr JoinRule() : rule_t()
		{
		}
		constexpr JoinRule(rule_t a) : rule_t(a)
		{
		}
		constexpr JoinRule(auto a, auto b) : rule_t(a, b)
		{
		}

		constexpr operator rule_t()
		{
			return this->to_rule();
		}

		constexpr rule_t to_rule() const
		{
			return static_cast<rule_t>(*this);
		}
	};

	template <typename _Properties, typename rule_t> class GroupByRule;

	template <typename _Properties, is_rule rule_t> class GroupByRule<_Properties, rule_t> : public rule_t
	{
		public:
		using Properties = _Properties;
		Properties properties;
		static constexpr size_t size = Properties::size;

		constexpr GroupByRule() : rule_t()
		{
		}
		constexpr GroupByRule(Properties a, rule_t b) : rule_t(b), properties(a)
		{
		}
		constexpr GroupByRule(Properties p, auto a, auto b) : rule_t(a, b), properties(p)
		{
		}

		static constexpr bool has_rule()
		{
			return true;
		}

		constexpr operator rule_t()
		{
			return this->to_rule();
		}

		constexpr rule_t to_rule() const
		{
			return static_cast<rule_t>(*this);
		}
	};

	template <typename _Properties> class GroupByRule<_Properties, nullptr_t>
	{
		public:
		using Properties = _Properties;
		Properties properties;
		static constexpr size_t size = Properties::size;

		constexpr GroupByRule()
		{
		}
		constexpr GroupByRule(Properties a, nullptr_t) : properties(a)
		{
		}

		static constexpr bool has_rule()
		{
			return false;
		}
	};

	template <auto P, modes::order way> class OrderBy
	{
		public:
		static constexpr auto property = P;
		static constexpr auto sort = way;
	};
} // namespace webframe::ORM
