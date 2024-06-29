#pragma once

#include <ORM/tools/property.hpp>
#include <ORM/wrappers/db_types.hpp>

namespace webframe::ORM
{
    namespace details
    {
        template<typename T>
        struct QueryInput : public IPlaceholder
        {
            static constexpr std::string_view entity_name = "Placeholder";
            property<db_type_placeholder<T>, "query_input"> placeholder;
        };
    } // namespace details
    
    template<typename T>
    static constexpr auto Placeholder = &details::QueryInput<T>::placeholder;
}