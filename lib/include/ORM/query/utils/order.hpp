#pragma once
#include <ORM/utils/concept.hpp>

namespace webframe::ORM
{
	enum OrderEnum
	{
		ASC,
		DESC,
		Default
	};

	namespace details
	{
		template <details::is_table_pointer_to_member_variable_t mem_ptr, OrderEnum _order> struct OrderWrapper
		{
			using member_ptr = mem_ptr;
			static constexpr OrderEnum order = _order;
		};
        
		template <auto _rows, auto _offset>
        requires ((std::is_convertible_v<decltype(_rows), size_t> || is_orm_placeholder<decltype(_rows)>) && (std::is_convertible_v<decltype(_offset), size_t>  || is_orm_placeholder<decltype(_offset)>))
        struct LimitWrapper
		{
			static constexpr auto rows = _rows;
			static constexpr auto offset = _offset;
		};
    }
}