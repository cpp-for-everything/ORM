#pragma once
#include <ORM/utils/query_input_data.hpp>

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
		};
	} // namespace details

	template <auto ptr> constexpr auto P = details::mem_ptr_wrapper<ptr>();

} // namespace webframe::ORM
