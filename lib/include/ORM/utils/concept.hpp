#pragma once

#include <concepts>
#include <string_view>

namespace webframe::ORM::details
{
    template<typename T>
    class mem_ptr_utils;

    class i_mem_ptr {};

    template<auto ptr>
    class mem_ptr_wrapper;

    template<typename T>
    concept is_database_wrapped_class = requires {
        typename T::native_type;
    };

    template<typename T>
    concept is_table = requires {
        typename T;
        { T::entity_name } -> std::same_as<std::string_view>;
    };

    template<typename T>
    concept is_table_pointer_to_member_variable_t = 
        (std::is_member_object_pointer_v<T> && is_table<typename mem_ptr_utils<T>::class_type>) ||
        (std::is_base_of_v<i_mem_ptr, T> && is_table<typename T::class_type>);
}
