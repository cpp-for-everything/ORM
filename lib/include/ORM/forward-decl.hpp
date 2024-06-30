#pragma once

#include <ORM/utils/concept.hpp>
#include <ORM/tools/string_literal.hpp>
#include <ORM/utils/rule_operators.hpp>

namespace webframe::ORM
{
	template <typename T, details::string_literal column_name>
		requires(details::is_database_wrapped_class<T> && !std::is_base_of_v<details::IProperty, T>)
	class property;

	template <typename T1, details::rule_operators op, typename T2> class Rule;
} // namespace webframe::ORM
