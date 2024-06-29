#pragma once

#include <concepts>
#include <ORM/utils/concept.hpp>
#include <ORM/utils/mem_ptr_utils.hpp>

namespace webframe::ORM
{
    namespace details
    {
        class IPlaceholder {};

        template<typename T>
        constexpr bool check_for_placeholder()
        {
            if constexpr (std::is_base_of_v<i_mem_ptr, T>)
            {
                return std::is_base_of_v<IPlaceholder, typename T::class_type>;
            }
            else if constexpr (std::is_member_object_pointer_v<T>)
            {
                return std::is_base_of_v<IPlaceholder, typename mem_ptr_utils<T>::class_type>;
            }
            else
            {
                return false;
            }
        }

        template<typename ptr_t>
        concept is_placeholder_t = check_for_placeholder<ptr_t>();

        template<auto ptr>
        concept is_placeholder = check_for_placeholder<decltype(ptr)>();

        template<typename T>
        concept is_table_pointer_to_member_variable_or_placeholder_t = is_table_pointer_to_member_variable_t<T> || is_placeholder_t<T>;
    } // namespace details
} // namespace webframe::ORM
