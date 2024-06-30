#pragma once

#include <ORM/tools/string_literal.hpp>
#include <ORM/utils/concept.hpp>

namespace webframe::ORM
{
    namespace details
    {
        class IProperty {};
    }

    template<typename T, details::string_literal column_name>
    requires (details::is_database_wrapped_class<T> && !std::is_base_of_v<details::IProperty, T>)
    class property : public T, public details::IProperty
    {
    public:
        using db_type = T;
        using native_type = typename T::native_type;

        static inline constexpr std::string_view name() { return column_name.to_sv(); }
        
        constexpr property();
        
        // mimic any constructor T has
        template<typename... Ts>
        constexpr property(Ts... args);

        constexpr native_type raw() const { return native_type(*this); }
    };
} // namespace webframe::ORM

#include <ORM/tools/property.cpp>
