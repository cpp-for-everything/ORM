#pragma once

#include <tuple>
#include <ORM/tools/rule.hpp>
#include <ORM/tools/statement_expression.hpp>
#include <ORM/tools/statement.hpp>

namespace webframe::ORM::details
{
	template <typename T> struct tuple_of_the_placeholders
	{
		using type = std::tuple<>;
	};

	template <typename... Ts> struct tuple_of_the_placeholders<std::tuple<Ts...>>
	{
		using type = decltype(std::tuple_cat(std::declval<typename tuple_of_the_placeholders<Ts>::type>()...));
	};

	template <typename T1, details::rule_operators op, typename T2> struct tuple_of_the_placeholders<Rule<T1, op, T2>>
	{
		using type = decltype(std::tuple_cat(std::declval<typename tuple_of_the_placeholders<T1>::type>(), std::declval<typename tuple_of_the_placeholders<T2>::type>()));
	};

	template <typename T1, details::expression_operators op, typename T2> struct tuple_of_the_placeholders<Expression<T1, op, T2>>
	{
		using type = decltype(std::tuple_cat(std::declval<typename tuple_of_the_placeholders<T1>::type>(), std::declval<typename tuple_of_the_placeholders<T2>::type>()));
	};

	template <typename T1, details::assignment_operators op, typename T2> struct tuple_of_the_placeholders<Statement<T1, op, T2>>
	{
		using type = decltype(std::tuple_cat(std::declval<typename tuple_of_the_placeholders<T1>::type>(), std::declval<typename tuple_of_the_placeholders<T2>::type>()));
	};

	template <details::is_table_pointer_to_member_variable_t T1, details::assignment_operators op, typename T2>
	struct tuple_of_the_placeholders<Statement<T1, op, T2>>
	{
		using type = decltype(std::tuple_cat(std::declval<typename tuple_of_the_placeholders<T1>::type>(), std::declval<typename tuple_of_the_placeholders<T2>::type>()));
	};

	template <is_orm_placeholder T> struct tuple_of_the_placeholders<T>
	{
		using type = std::tuple<T>;
	};
} // namespace webframe::ORM::details
