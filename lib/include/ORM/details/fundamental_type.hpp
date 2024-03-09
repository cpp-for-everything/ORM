/**
 *  @file   fundamental_type.hpp
 *  @brief  Concept definition for fundamental type
 *  @author Alex Tsvetanov
 *  @date   2023-08-31
 ***********************************************/

#pragma once

#include <concepts>

namespace webframe::ORM::details {
template <typename T>
concept is_inheritable = !std::is_fundamental_v<T> && !std::is_enum_v<T>;

template <typename T>
concept is_not_inheritable = std::is_fundamental_v<T> || std::is_enum_v<T>;
} // namespace webframe::ORM::details