/**
 *  @file   rules.hpp
 *  @brief  Rule class definition
 *  @author Alex Tsvetanov
 *  @date   2023-08-31
 ***********************************************/

#pragma once

#include "details/member_pointer.hpp"
#include "details/string_literal.hpp"
#include <iostream>

using namespace std::literals;

namespace webframe::ORM {
class IRule {};
template <typename T>
concept is_rule = std::derived_from<T, IRule>;
template <typename T1, details::string_literal op, typename T2> class Rule;
} // namespace webframe::ORM

#include "../../src/ORM/rules.cpp"