#pragma once

#include <ORM/tools/string_literal.hpp>
#include <ORM/utils/concept.hpp>

namespace webframe::ORM
{
	namespace details
	{
		class IAlias
		{
		};
	} // namespace details

	template <typename T, details::string_literal column_name>
	class alias : public T, public details::IAlias
	{
		public:
		T val;
		constexpr alias(T _val) : val(_val) { }

		static inline constexpr std::string_view name()
		{
			return column_name.to_sv();
		}
	};
} // namespace webframe::ORM
