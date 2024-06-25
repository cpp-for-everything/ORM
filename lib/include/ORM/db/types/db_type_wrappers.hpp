#pragma once
#include <cstring>
#include <bitset>
#include <string>
#include "primitive_type_wrapper.hpp"

namespace webframe::ORM {

// Bitset
    template<size_t n>
    class BIT : public std::bitset<n> {
    public:
        using reference = typename std::bitset<n>::reference;
        using native_type = std::bitset<n>;
        static constexpr size_t db_size = n;
        static constexpr std::string_view db_type = "BIT";

        BIT(auto... Ts) : std::bitset<n>(Ts...) {}

        native_type& get() { return *this; } const native_type& get_const() const { return *this; }
    };

// Strings
    template <typename T>
    concept Character = 
        std::same_as<T, char> ||
        std::same_as<T, signed char> ||
        std::same_as<T, unsigned char> ||
        std::same_as<T, wchar_t> ||
        std::same_as<T, char8_t> ||
        std::same_as<T, char16_t> ||
        std::same_as<T, char32_t>;

    template<size_t n, typename T = char> requires (Character<T>)
    class CHAR : public std::basic_string<T> {
    public:
        using native_type = std::basic_string<T>;
        static constexpr size_t db_size = n;
        static constexpr std::string_view db_type = "CHAR";

        CHAR(auto... Ts) : std::basic_string<T>(Ts...) {}

        native_type& get() { return *this; } const native_type& get_const() const { return *this; }
    };

    template<size_t n, typename T = char> requires (Character<T>)
    class VARCHAR : public std::basic_string<T> {
    public:
        using native_type = std::basic_string<T>;
        static constexpr size_t db_size = n;
        static constexpr std::string_view db_type = "VARCHAR";

        VARCHAR(auto... Ts) : std::basic_string<T>(Ts...) {}

        native_type& get() { return *this; } const native_type& get_const() const { return *this; }
    };

    template<typename T = char, size_t n = 255> requires (n <= 255)
    using TINYTEXT = VARCHAR<n, T>;
    template<typename T = char, size_t n = 65'535> requires (n <= 65'535)
    using TEXT = VARCHAR<n, T>;
    template<typename T = char, size_t n = 16'777'215> requires (n <= 16'777'215)
    using MEDIUMTEXT = VARCHAR<n, T>;
    template<typename T = char, size_t n = 4'294'967'295> requires (n <= 4'294'967'295)
    using LONGTEXT = VARCHAR<n, T>;

// Binary
    template<size_t n>
    class BINARY : CHAR<n, unsigned char> {
    public:
        using native_type = typename CHAR<n, unsigned char>::native_type;
        static constexpr size_t db_size = n;
        static constexpr std::string_view db_type = "BINARY";

        BINARY(auto... Ts) : CHAR<n, unsigned char>(Ts...) {}

        native_type& get() { return *this; } const native_type& get_const() const { return *this; }
    };

    template<size_t n>
    class VARBINARY : VARCHAR<n, unsigned char> {
    public:
        using native_type = typename VARCHAR<n, unsigned char>::native_type;
        static constexpr size_t db_size = n;
        static constexpr std::string_view db_type = "VARBINARY";
        
        VARBINARY(auto... Ts) : VARCHAR<n, unsigned char>(Ts...) {}

        native_type& get() { return *this; } const native_type& get_const() const { return *this; }
    };

    template<size_t n = 255> requires (n <= 255)
    using TINYBLOB = VARBINARY<n>;
    template<size_t n = 65'535> requires (n <= 65'535)
    using BLOB = VARBINARY<n>;
    template<size_t n = 16'777'215> requires (n <= 16'777'215)
    using MEDIUMBLOB = VARBINARY<n>;
    template<size_t n = 4'294'967'295> requires (n <= 4'294'967'295)
    using LONGBLOB = VARBINARY<n>;

// Boolean
    class BOOL {
        bool value;
    public:
        using native_type = bool;
        static constexpr std::string_view db_type = "BOOL";

        BOOL(const bool value) : value(value) {}
        BOOL(bool& value) : value(value) {}

        BOOL() : value() {}

        inline operator bool() const { return value; }
        inline operator bool& () { return value; }

        bool& operator = (auto _value) { 
            static_cast<bool&>(*this) = _value;
            return this->value; 
        }
        
        native_type& get() { return this->value; } const native_type& get_const() const { return this->value; }
    };

    using BOOLEAN = BOOL;

// Integers
    namespace impl {
        template<size_t n = 32, typename T = signed> requires (n <= 64)
        struct INTEGER_wrapper;

#define specify(CLASS, N1, N2, UN_OR_SIGNED, TYPE) \
        template<size_t n> requires (n >= N1 && n <= N2) \
        struct CLASS<n, UN_OR_SIGNED> { \
            using type = webframe::ORM::Primitive<TYPE, #TYPE, n>; \
        }

        specify(INTEGER_wrapper, 0, 16, signed, int16_t);
        specify(INTEGER_wrapper, 17, 32, signed, int32_t);
        specify(INTEGER_wrapper, 33, 64, signed, int64_t);
        specify(INTEGER_wrapper, 0, 16, unsigned, uint16_t);
        specify(INTEGER_wrapper, 17, 32, unsigned, uint32_t);
        specify(INTEGER_wrapper, 33, 64, unsigned, uint64_t);
#undef specify
    }

    template<size_t n = 32, typename T = signed> requires (n <= 64)
    using INTEGER = typename impl::INTEGER_wrapper<n, T>::type;

    constexpr size_t percision_not_set = 18446744073709551615ull;

    template<size_t n, size_t p = percision_not_set> requires (p <= 24 || p == percision_not_set)
    using FLOAT = webframe::ORM::Primitive<float, "FLOAT", n, p>;

    template<size_t n, size_t p = percision_not_set> requires ((p >= 25 && p <= 53) || p == percision_not_set)
    using DOUBLE = webframe::ORM::Primitive<double, "DOUBLE", n, p>;
    
    namespace impl {
        template<size_t, size_t>
        struct DECIMAL_wrapper;
        
        template<size_t n, size_t p> requires (p <= 24)
        struct DECIMAL_wrapper<n, p> {
            using type = FLOAT<n, p>;
        };
        
        template<size_t n, size_t p> requires (p >= 25 && p <= 53)
        struct DECIMAL_wrapper<n, p> {
            using type = DOUBLE<n, p>;
        };
    }
    
    template<size_t n, size_t p>
    using DECIMAL = typename impl::DECIMAL_wrapper<n, p>::type;

// NULLable
    class INullable {};
    
    template<typename T>
    concept is_nullable = std::is_base_of_v<INullable, T>;

    template<typename T>
    class Nullable : public INullable, public T {
        bool _is_null;
    public:
        using original_type = T;
        
        Nullable() : Nullable(nullptr) {}
        
        template<typename U> requires (!std::is_same_v<U, nullptr_t>)
        Nullable(U x) : T(x), _is_null(false) {}
        
        template<typename U> requires (!std::is_same_v<U, nullptr_t>)
        Nullable(U x, auto... args) : T(x, args...), _is_null(false) {}
        
        Nullable(nullptr_t) : _is_null(true) {}

        bool is_null() const { return _is_null; }
    };
}