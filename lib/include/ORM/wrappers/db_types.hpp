#pragma once

#include <type_traits>
#include <bitset>
#include <string>
#include <array>
#include <stdint.h>

namespace webframe::ORM
{
	namespace details
	{
		template <typename T> class PrimitiveWrapper;

		template <typename T>
			requires(std::is_fundamental_v<T>)
		class PrimitiveWrapper<T>
		{
			T data;

			public:
			constexpr PrimitiveWrapper() : data()
			{
			}
			constexpr PrimitiveWrapper(const PrimitiveWrapper<T>& x) : data(x.data)
			{
			}

			template <typename Y>
				requires(!std::is_same_v<Y, PrimitiveWrapper<T>>)
			constexpr PrimitiveWrapper(const Y& x) : data(x)
			{
			}

			operator T&()
			{
				return data;
			}
			operator const T&() const
			{
				return data;
			}
		};

		template <typename T>
			requires(!std::is_fundamental_v<T>)
		class PrimitiveWrapper<T> : public T
		{
			public:
			constexpr PrimitiveWrapper() : T()
			{
			}

			constexpr PrimitiveWrapper(const PrimitiveWrapper<T>& x) : T(x)
			{
			}

			template <typename... Ts> constexpr PrimitiveWrapper(Ts... args) : T(args...)
			{
			}
		};

		enum db_type
		{
			variadic_string,
			fixed_string,
			binary_variadic_string,
			binary_fixed_string,

			tiny_string,
			medium_string,
			long_string,

			tiny_binary,
			medium_binary,
			long_binary,

			boolean,
			bitset,

			tiny_int,
			small_int,
			medium_int,
			integer,
			big_int,

			floating_point
		};
		template <typename cpp_equivalent> class db_type_placeholder : public details::PrimitiveWrapper<cpp_equivalent>
		{
			public:
			using native_type = cpp_equivalent;
			constexpr db_type_placeholder() : details::PrimitiveWrapper<cpp_equivalent>()
			{
			}
			template <typename... Ts> constexpr db_type_placeholder(Ts... args) : details::PrimitiveWrapper<cpp_equivalent>(args...)
			{
			}
		};
	} // namespace details

#define COMMA ,
#define create_db_type(db_type_name, cpp_equivalent, db_params, db_enum_type, ...)                                                                             \
	db_params __VA_ARGS__ class db_type_name : public details::PrimitiveWrapper<cpp_equivalent>                                                                \
	{                                                                                                                                                          \
		public:                                                                                                                                                \
		using native_type = cpp_equivalent;                                                                                                                    \
		static constexpr auto Type = details::db_type::db_enum_type;                                                                                           \
		constexpr db_type_name() : details::PrimitiveWrapper<cpp_equivalent>()                                                                                 \
		{                                                                                                                                                      \
		}                                                                                                                                                      \
		template <typename... Ts> constexpr db_type_name(Ts... args) : details::PrimitiveWrapper<cpp_equivalent>(args...)                                      \
		{                                                                                                                                                      \
		}                                                                                                                                                      \
	};

	// String Data Types
	create_db_type(CHAR, std::array<char COMMA size>, template <size_t size>, fixed_string, requires(0 <= size && size <= 255));
	create_db_type(VARCHAR, std::string, template <size_t size>, variadic_string, requires(0 <= size && size <= 65'535));

	namespace details
	{
		create_db_type(TEXT, std::string, template <size_t size>, variadic_string, );
		create_db_type(BLOB, std::string, template <size_t size>, variadic_string, );
	} // namespace details

	template <size_t size = 255>
		requires(0 <= size && size <= 255)
	using TINYTEXT = details::TEXT<size>;
	template <size_t size = 65'535>
		requires(0 <= size && size <= 65'535)
	using TEXT = details::TEXT<size>;
	template <size_t size = 16'777'215>
		requires(0 <= size && size <= 16'777'215)
	using MEDIUMTEXT = details::TEXT<size>;
	template <size_t size = 4'294'967'295>
		requires(0 <= size && size <= 4'294'967'295)
	using LONGTEXT = details::TEXT<size>;

	template <size_t size = 255>
		requires(0 <= size && size <= 255)
	using TINYBLOB = details::BLOB<size>;
	template <size_t size = 65'535>
		requires(0 <= size && size <= 65'535)
	using BLOB = details::BLOB<size>;
	template <size_t size = 16'777'215>
		requires(0 <= size && size <= 16'777'215)
	using MEDIUMBLOB = details::BLOB<size>;
	template <size_t size = 4'294'967'295>
		requires(0 <= size && size <= 4'294'967'295)
	using LONGBLOB = details::BLOB<size>;

	create_db_type(BINARY, std::bitset<size>, template <size_t size>, binary_fixed_string, requires(0 <= size && size <= 255));
	create_db_type(VARBINARY, std::bitset<size>, template <size_t size>, binary_variadic_string, requires(0 <= size && size <= 65'535));

	// Numeric Data Types
	create_db_type(BOOL, bool, , boolean, );
	create_db_type(BIT, std::bitset<size>, template <size_t size>, bitset, requires(1 <= size && size <= 64));

	create_db_type(TINYINT, int8_t, , tiny_int, );
	create_db_type(UTINYINT, uint8_t, , tiny_int, );

	create_db_type(SMALLINT, int16_t, , small_int, );
	create_db_type(USMALLINT, uint16_t, , small_int, );

	create_db_type(INT, int32_t, , integer, );
	create_db_type(UINT, uint32_t, , integer, );

	create_db_type(BIGINT, int64_t, , big_int, );
	create_db_type(UBIGINT, uint64_t, , big_int, );

	create_db_type(FLOAT, float, template <size_t precision = 24>, floating_point, requires(0 <= precision && precision <= 24));
	create_db_type(DOUBLE, double, template <size_t precision = 53>, floating_point, requires(25 <= precision && precision <= 53));

	// Date and Time Data Types
	create_db_type(DATE, std::string, , variadic_string, );
	create_db_type(DATETIME, std::string, , variadic_string, );
	create_db_type(TIMESTAMP, std::string, , variadic_string, );
	create_db_type(TIME, std::string, , variadic_string, );
	create_db_type(YEAR, std::string, , variadic_string, );

#undef create_db_type
#undef COMMA

} // namespace webframe::ORM
