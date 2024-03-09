/**
 *  @file   select.hpp
 *  @brief  Functional-programming-like select queries
 *  @author Alex Tsvetanov
 *  @date   2023-08-31
 ***********************************************/

#pragma once

#include "../details/result_type.hpp"
#include "utils.hpp"
#include <concepts>
#include <tuple>
#include <utility>

namespace webframe::ORM::CRUD {
template <auto... Properties>
class insert_query : public details::insert_query_t {
public:
  using properties =
      typename webframe::ORM::details::get_placeholders_and_properties<
          webframe::ORM::details::orm_tuple<decltype(Properties)...>>::
          orm_placeholders;
  using tables = typename details::unique_tables<Properties...>::orm_tuple_type;
  static constexpr auto signature = webframe::ORM::details::orm_tuple(
      std::make_tuple<decltype(Properties)...>(Properties...));
};
} // namespace webframe::ORM::CRUD
namespace webframe::ORM {
template <auto... Ts>
static constexpr auto insert = webframe::ORM::CRUD::insert_query<Ts...>();
}
#include "../../../src/ORM/CRUD/insert.cpp"