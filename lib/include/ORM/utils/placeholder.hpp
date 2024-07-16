#pragma once

#include <concepts>
#include <ORM/utils/concept.hpp>
#include <ORM/utils/mem_ptr_utils.hpp>

namespace webframe::ORM
{
	namespace details
	{
		class IPlaceholder
		{
		};

		template <typename T>
		concept is_orm_placeholder = std::is_base_of_v<IPlaceholder, T>;
	} // namespace details

	template <int I, typename T> struct placeholder : public details::IPlaceholder
	{
		using type = T;
	};

	template <int I, typename T> constexpr placeholder<I, T> Placeholder = placeholder<I, T>{};

	namespace placeholders
	{
#define create_placeholder(N) template <typename T> constexpr auto _##N = Placeholder<N, T>;
		create_placeholder(1);
		create_placeholder(2);
		create_placeholder(3);
		create_placeholder(4);
		create_placeholder(5);
		create_placeholder(6);
		create_placeholder(7);
		create_placeholder(8);
		create_placeholder(9);
		create_placeholder(10);
		create_placeholder(11);
		create_placeholder(12);
		create_placeholder(13);
		create_placeholder(14);
		create_placeholder(15);
		create_placeholder(16);
		create_placeholder(17);
		create_placeholder(18);
		create_placeholder(19);
		create_placeholder(20);
	} // namespace placeholders
} // namespace webframe::ORM

#include <functional>

namespace std
{
	template <int I, typename T> struct is_placeholder<webframe::ORM::placeholder<I, T>> : std::integral_constant<int, I>
	{
	};
} // namespace std
