#pragma once

#include <ORM/forward-decl.hpp>
#include <ORM/utils/assignment_operators.hpp>
#include <ORM/utils/placeholder.hpp>
#include <ORM/utils/query_input_data.hpp>
#include <ORM/tools/statement_expression.hpp>

namespace webframe::ORM
{
	template <details::is_table_pointer_to_member_variable_t T1, details::assignment_operators op, typename T2> class Statement
	{
		public:
		T1 a;
		T2 b;

		using operand1_t = T1;
		static constexpr details::assignment_operators operation = op;
		using operand2_t = T2;

		constexpr Statement(T1 x, T2 y) : a(x), b(y)
		{
		}
	};
} // namespace webframe::ORM

#include <ORM/tools/statement.cpp>
