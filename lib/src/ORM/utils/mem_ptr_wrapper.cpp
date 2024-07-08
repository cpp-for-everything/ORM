#include <ORM/utils/mem_ptr_wrapper.hpp>

#include <ORM/tools/rule.hpp>

namespace webframe::ORM
{
#define IMPLEMENT_OPERATOR(op, op_enum)                                                                                                                        \
	template <typename T, typename C, T C::*ptr>                                                                                                               \
	template <typename Y>                                                                                                                                      \
		requires(std::is_convertible_v<Y, typename T::native_type>)                                                                                            \
	constexpr auto details::mem_ptr_wrapper<ptr>::operator op(Y a) const                                                                                       \
	{                                                                                                                                                          \
		return Rule<details::mem_ptr_wrapper<ptr>, details::op_enum, Y>(*this, a);                                                                             \
	}                                                                                                                                                          \
	template <typename T, typename C, T C::*ptr>                                                                                                               \
	constexpr auto details::mem_ptr_wrapper<ptr>::operator op(decltype(Placeholder<typename T::native_type>)) const                                            \
	{                                                                                                                                                          \
		return Rule<details::mem_ptr_wrapper<ptr>, details::op_enum, decltype(Placeholder<typename T::native_type>)>(                                          \
			*this, Placeholder<typename T::native_type>);                                                                                                      \
	}

	IMPLEMENT_OPERATOR(<, Less)
	IMPLEMENT_OPERATOR(>, Greater)
	IMPLEMENT_OPERATOR(==, Equals)
	IMPLEMENT_OPERATOR(!=, Not_equal)
	IMPLEMENT_OPERATOR(<=, Less_or_equal)
	IMPLEMENT_OPERATOR(>=, Greater_or_equal)
#undef IMPLEMENT_OPERATOR
} // namespace webframe::ORM

#include <ORM/tools/statement.hpp>

namespace webframe::ORM::details
{
#define IMPLEMENT_OPERATOR(op, op_enum)                                                                                                                        \
	template <typename T, typename C, T C::*ptr> template <is_expression Y> constexpr auto mem_ptr_wrapper<ptr>::operator op(Y a) const                   \
	{                                                                                                                                                          \
		return Statement<mem_ptr_wrapper<ptr>, op_enum, Y>(*this, a);                                                                                          \
	}                                                                                                                        \
	template <typename T, typename C, T C::*ptr> template <typename Y> constexpr auto mem_ptr_wrapper<ptr>::operator op(Y a) const                   \
	{                                                                                                                                                          \
		return Statement<mem_ptr_wrapper<ptr>, op_enum, Constant<Y>>(*this, a);                                                                                          \
	}

	IMPLEMENT_OPERATOR(=, assignment_operators::Eq)
	IMPLEMENT_OPERATOR(+=, assignment_operators::PlusEq)
	IMPLEMENT_OPERATOR(-=, assignment_operators::MinusEq)
	IMPLEMENT_OPERATOR(/=, assignment_operators::DivEq)
	IMPLEMENT_OPERATOR(*=, assignment_operators::MulEq)
	IMPLEMENT_OPERATOR(%=, assignment_operators::ModEq)
#undef IMPLEMENT_OPERATOR
} // namespace webframe::ORM::details
