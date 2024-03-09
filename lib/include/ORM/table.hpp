/**
 *  @file   table.hpp
 *  @brief  Static functionalities for entities to be treated as tables
 *  @author Alex Tsvetanov
 *  @date   2022-09-09
 ***********************************************/

#pragma once

#include <ORM/ORM-version.hpp>
#include <ORM/details/warnings.hpp>

#include <ORM/std/string_view>
#include <algorithm>
#include <concepts>
#include <iostream>
#include <pfr.hpp>
#include <string>

#include <ORM/fields.hpp>
#include <utility>

namespace webframe::ORM {
template <details::string_literal name> struct DBTable {
  static constexpr std::string_view table_name =
      name.operator std::string_view();
};

template <typename T> class Table {
public:
  using tuple_t = decltype(pfr::structure_to_tuple(std::declval<T>()));

private:
  template <size_t i>
  static size_t _get_index_by_column(const std::string_view column);

  template <size_t i>
  static T &_tuple_to_struct(T &output, const tuple_t &tuple);

  template <size_t i, typename T1> static T &_to_struct(T &output, T1 arg);

  template <size_t i, typename T1, typename... Ts>
  static T &_to_struct(T &output, T1 arg, Ts... args);

  template <size_t i, typename T1>
  static tuple_t &_to_tuple(tuple_t &output, T1 arg);

  template <size_t i, typename T1, typename... Ts>
  static tuple_t &_to_tuple(tuple_t &output, T1 arg, Ts... args);

public:
  template <typename... Ts> static T to_struct(Ts... args);

  template <typename... Ts> static tuple_t to_tuple(Ts... args);

  static constexpr size_t get_index_by_column(std::string_view column);

  static T tuple_to_struct(const tuple_t &tuple);
};
} // namespace webframe::ORM

#include "../../src/ORM/table.cpp"