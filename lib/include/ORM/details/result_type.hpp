/**
 *  @file   result_type.hpp
 *  @brief  Result dataset class definition
 *  @author Alex Tsvetanov
 *  @date   2023-08-31
 ***********************************************/

#pragma once

#include "member_pointer.hpp"
#include "orm_tuple.hpp"
#include <utility>

namespace webframe::ORM::details
{
	constexpr auto result_all = []<typename>() consteval { return true; };
	constexpr auto result_properties = []<typename T>() consteval { return details::is_property<T>; };

	template <auto filter, typename... args>
		requires requires {
			{
				filter.template operator()<int>()
			};
		}
	class result_t
	{
		private:
		template <typename arg1, typename... args1> static constexpr auto result_t_utils(arg1 a, args1... A)
		{
			if constexpr (sizeof...(args1) == 0)
			{
				if constexpr (filter.template operator()<arg1>())
				{
					return orm_tuple(std::make_tuple(a));
				}
				if constexpr (!filter.template operator()<arg1>())
				{
					return orm_tuple(std::tuple<>());
				}
			}
			if constexpr (sizeof...(args1) > 0)
			{
				return orm_tuple(std::tuple_cat(result_t_utils<arg1>(a).to_tuple(), result_t_utils<args1...>(A...).to_tuple()));
			}
		}

		public:
		using type = decltype(result_t_utils<args...>((std::declval<args>())...));

		static constexpr type create(args... argv)
		{
			return result_t_utils<args...>(argv...);
		}

		static constexpr type fuck_this_shit(args...)
		{
		}
	};
} // namespace webframe::ORM::details

#include "../../../src/ORM/details/result_type.cpp"
