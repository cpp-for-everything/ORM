#pragma once
#include <ORM/utils/query_input_data.hpp>
#include <ORM/tools/statement.hpp>

namespace webframe::ORM
{
	namespace details
	{
		template <typename T, typename C, T C::*ptr> class mem_ptr_wrapper<ptr> : public i_mem_ptr
		{
			public:
			using variable_type = T;
			using class_type = C;
			constexpr mem_ptr_wrapper() = default;
			template <auto ptr2> static constexpr bool equals()
			{
				if constexpr (!std::is_same_v<decltype(ptr2), T C::*>)
				{
					return false;
				}
				if constexpr (std::is_same_v<decltype(ptr2), T C::*>)
				{
					return ptr2 == ptr;
				}
			}
			constexpr operator T C::*()
			{
				return ptr;
			}
			static constexpr T C::*to_mem_ptr()
			{
				return ptr;
			}

#define DEFINE_OPERATOR(op)                                                                                                                                    \
	template <typename Y>                                                                                                                                      \
		requires(std::is_convertible_v<Y, typename T::native_type>)                                                                                            \
	constexpr auto operator op(Y a) const;                                                                                                                     \
	constexpr auto operator op(decltype(Placeholder<typename T::native_type>) a) const;

			DEFINE_OPERATOR(<)
			DEFINE_OPERATOR(>)
			DEFINE_OPERATOR(==)
			DEFINE_OPERATOR(!=)
			DEFINE_OPERATOR(<=)
			DEFINE_OPERATOR(>=)
#undef DEFINE_OPERATOR

#define DEFINE_OPERATOR(op) template <details::is_expression Y> constexpr auto operator op(Y a) const;

			DEFINE_OPERATOR(=)
			DEFINE_OPERATOR(+=)
			DEFINE_OPERATOR(-=)
			DEFINE_OPERATOR(/=)
			DEFINE_OPERATOR(*=)
			DEFINE_OPERATOR(%=)
#undef DEFINE_OPERATOR

#define DEFINE_OPERATOR(op, op_enum)                                                                                                                           \
	template <typename X> constexpr auto operator op(X a) const                                                                                                \
	{                                                                                                                                                          \
		return Expression<mem_ptr_wrapper<ptr>, op_enum, X>(*this, a);                                                                                         \
	}                                                                                                                                                          \
	template <typename X> friend constexpr auto operator op(Constant<X> a, mem_ptr_wrapper<ptr> b)                                                             \
	{                                                                                                                                                          \
		return Expression<Constant<X>, op_enum, mem_ptr_wrapper<ptr>>(a, b);                                                                                   \
	}                                                                                                                                                          \
	template <typename X, details::expression_operators op2, typename Y> friend constexpr auto operator op(Expression<X, op2, Y> a, mem_ptr_wrapper<ptr> b)    \
	{                                                                                                                                                          \
		return Expression<Expression<X, op2, Y>, op_enum, mem_ptr_wrapper<ptr>>(a, b);                                                                         \
	}

			DEFINE_OPERATOR(+, details::expression_operators::Plus)
			DEFINE_OPERATOR(-, details::expression_operators::Minus)
			DEFINE_OPERATOR(/, details::expression_operators::Div)
			DEFINE_OPERATOR(*, details::expression_operators::Mul)
			DEFINE_OPERATOR(%, details::expression_operators::Mod)
#undef DEFINE_OPERATOR
		};
	} // namespace details

	template <auto ptr> constexpr auto P = details::mem_ptr_wrapper<ptr>();

} // namespace webframe::ORM

#include <ORM/utils/mem_ptr_wrapper.cpp>
