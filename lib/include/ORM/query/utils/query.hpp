#pragma once

#include <concepts>
#include <tuple>

namespace webframe::ORM
{
    namespace details
    {
        class IQuery { };
        class ITypeContainer { };
        
        template<typename T>
        concept is_query = requires {
            typename T::parameters_type;
            typename T::result_type;
            { T::allow_repeats } -> std::same_as<const bool&>;
            std::derived_from<T, IQuery>;
        };
        
        template<typename T>
        concept is_type_container = requires {
            std::derived_from<T, ITypeContainer>;
        };

        template<typename... Ts>
        class type_constainer : public ITypeContainer
        {
        public:
            using tuple_equivalent = std::tuple<Ts...>;
            static constexpr size_t size = sizeof...(Ts);
            template<size_t ind>
            using type_at = decltype(std::get<ind>(std::declval<tuple_equivalent>()));
        };
    } // namespace details
} // namespace webframe::ORM
