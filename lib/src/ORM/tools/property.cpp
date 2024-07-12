#include <ORM/tools/property.hpp>

namespace webframe::ORM
{
	template <typename T, details::string_literal column_name>
		requires(details::is_database_wrapped_class<T>)
	constexpr property<T, column_name>::property() : T()
	{
	}

	template <typename T, details::string_literal column_name>
		requires(details::is_database_wrapped_class<T>)
	template <typename... Ts>
	constexpr property<T, column_name>::property(Ts... args) : T(args...)
	{
	}
} // namespace webframe::ORM
