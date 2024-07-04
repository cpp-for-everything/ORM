#include <ORM/tools/statement_expression.hpp>

namespace webframe::ORM
{
	template <typename T>
	template <typename T3, details::expression_operators op2, typename T4>
	constexpr auto Constant<T>::operator+(Expression<T3, op2, T4> e) const
	{
		return Expression<Constant<T>, details::expression_operators::Plus, Expression<T3, op2, T4>>(*this, e);
	}

	template <typename T>
	template <typename T3, details::expression_operators op2, typename T4>
	constexpr auto Constant<T>::operator-(Expression<T3, op2, T4> e) const
	{
		return Expression<Constant<T>, details::expression_operators::Minus, Expression<T3, op2, T4>>(*this, e);
	}

	template <typename T>
	template <typename T3, details::expression_operators op2, typename T4>
	constexpr auto Constant<T>::operator*(Expression<T3, op2, T4> e) const
	{
		return Expression<Constant<T>, details::expression_operators::Mul, Expression<T3, op2, T4>>(*this, e);
	}

	template <typename T>
	template <typename T3, details::expression_operators op2, typename T4>
	constexpr auto Constant<T>::operator/(Expression<T3, op2, T4> e) const
	{
		return Expression<Constant<T>, details::expression_operators::Div, Expression<T3, op2, T4>>(*this, e);
	}

	template <typename T>
	template <typename T3, details::expression_operators op2, typename T4>
	constexpr auto Constant<T>::operator%(Expression<T3, op2, T4> e) const
	{
		return Expression<Constant<T>, details::expression_operators::Mod, Expression<T3, op2, T4>>(*this, e);
	}

	template <typename T> template <typename Y> constexpr auto Constant<T>::operator+(Constant<Y> e) const
	{
		return Expression<Constant<T>, details::expression_operators::Plus, Constant<Y>>(*this, e);
	}

	template <typename T> template <typename Y> constexpr auto Constant<T>::operator-(Constant<Y> e) const
	{
		return Expression<Constant<T>, details::expression_operators::Minus, Constant<Y>>(*this, e);
	}

	template <typename T> template <typename Y> constexpr auto Constant<T>::operator*(Constant<Y> e) const
	{
		return Expression<Constant<T>, details::expression_operators::Mul, Constant<Y>>(*this, e);
	}

	template <typename T> template <typename Y> constexpr auto Constant<T>::operator/(Constant<Y> e) const
	{
		return Expression<Constant<T>, details::expression_operators::Div, Constant<Y>>(*this, e);
	}

	template <typename T> template <typename Y> constexpr auto Constant<T>::operator%(Constant<Y> e) const
	{
		return Expression<Constant<T>, details::expression_operators::Mod, Constant<Y>>(*this, e);
	}

	template <typename T1, details::expression_operators op, typename T2>
	template <typename T3, details::expression_operators op2, typename T4>
	constexpr auto Expression<T1, op, T2>::operator+(Expression<T3, op2, T4> b) const
	{
		return Expression<Expression<T1, op, T2>, details::expression_operators::Plus, Expression<T3, op2, T4>>(*this, b);
	}

	template <typename T1, details::expression_operators op, typename T2>
	template <typename T3, details::expression_operators op2, typename T4>
	constexpr auto Expression<T1, op, T2>::operator-(Expression<T3, op2, T4> b) const
	{
		return Expression<Expression<T1, op, T2>, details::expression_operators::Minus, Expression<T3, op2, T4>>(*this, b);
	}

	template <typename T1, details::expression_operators op, typename T2>
	template <typename T3, details::expression_operators op2, typename T4>
	constexpr auto Expression<T1, op, T2>::operator*(Expression<T3, op2, T4> b) const
	{
		return Expression<Expression<T1, op, T2>, details::expression_operators::Mul, Expression<T3, op2, T4>>(*this, b);
	}

	template <typename T1, details::expression_operators op, typename T2>
	template <typename T3, details::expression_operators op2, typename T4>
	constexpr auto Expression<T1, op, T2>::operator/(Expression<T3, op2, T4> b) const
	{
		return Expression<Expression<T1, op, T2>, details::expression_operators::Div, Expression<T3, op2, T4>>(*this, b);
	}

	template <typename T1, details::expression_operators op, typename T2>
	template <typename T3, details::expression_operators op2, typename T4>
	constexpr auto Expression<T1, op, T2>::operator%(Expression<T3, op2, T4> b) const
	{
		return Expression<Expression<T1, op, T2>, details::expression_operators::Mod, Expression<T3, op2, T4>>(*this, b);
	}

	template <typename T1, details::expression_operators op, typename T2>
	template <typename T>
	constexpr auto Expression<T1, op, T2>::operator+(Constant<T> b) const
	{
		return Expression<Expression<T1, op, T2>, details::expression_operators::Plus, Constant<T>>(*this, b);
	}

	template <typename T1, details::expression_operators op, typename T2>
	template <typename T>
	constexpr auto Expression<T1, op, T2>::operator-(Constant<T> b) const
	{
		return Expression<Expression<T1, op, T2>, details::expression_operators::Minus, Constant<T>>(*this, b);
	}

	template <typename T1, details::expression_operators op, typename T2>
	template <typename T>
	constexpr auto Expression<T1, op, T2>::operator*(Constant<T> b) const
	{
		return Expression<Expression<T1, op, T2>, details::expression_operators::Mul, Constant<T>>(*this, b);
	}

	template <typename T1, details::expression_operators op, typename T2>
	template <typename T>
	constexpr auto Expression<T1, op, T2>::operator/(Constant<T> b) const
	{
		return Expression<Expression<T1, op, T2>, details::expression_operators::Div, Constant<T>>(*this, b);
	}

	template <typename T1, details::expression_operators op, typename T2>
	template <typename T>
	constexpr auto Expression<T1, op, T2>::operator%(Constant<T> b) const
	{
		return Expression<Expression<T1, op, T2>, details::expression_operators::Mod, Constant<T>>(*this, b);
	}

} // namespace webframe::ORM
