/**
 *  @file   ORM.hpp
 *  @brief  Single header containing all the necessary tools regarding the
 *abstract ORM
 *  @author Alex Tsvetanov
 *  @date   2022-09-09
 ***********************************************/

#pragma once

#include <ORM/CRUD/delete.hpp>
#include <ORM/CRUD/insert.hpp>
#include <ORM/CRUD/select.hpp>
#include <ORM/CRUD/update.hpp>
#include <ORM/CRUD/utils.hpp>
#include <ORM/ORM-version.hpp>
#include <ORM/details/fundamental_type.hpp>
#include <ORM/details/member_pointer.hpp>
#include <ORM/details/orm_tuple.hpp>
#include <ORM/details/result_type.hpp>
#include <ORM/details/string_literal.hpp>
#include <ORM/details/warnings.hpp>
#include <ORM/fields.hpp>
#include <ORM/limits.hpp>
#include <ORM/rules.hpp>
#include <pfr.hpp>
//#include <ORM/db/connectors/interface.hpp>
#include <ORM/db/connectors/MockDB/init.hpp>
#include <ORM/db/types/db_type_wrappers.hpp>
#include <ORM/table.hpp>

namespace webframe::ORM {

template <typename Result> class Query {
public:
  using res_t = Result;
  constexpr Query() {}
};

} // namespace webframe::ORM

#include "../../src/ORM/ORM.cpp"