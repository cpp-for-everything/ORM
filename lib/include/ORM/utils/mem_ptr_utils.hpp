#pragma once

#include <ORM/utils/concept.hpp>

namespace webframe::ORM::details
{
	template <typename T, typename C> class mem_ptr_utils<T C::*>
	{
		public:
		using class_type = C;
		using type = typename T::native_type;
	};
} // namespace webframe::ORM::details
