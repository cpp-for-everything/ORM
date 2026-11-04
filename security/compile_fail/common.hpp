// Shared fixture for the negative compilation tests: one entity, one MockDB.
// MockDB renders SQL without a database, so these tests need no driver.
#pragma once
#include "ORM/ORM.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/connectors/MockDB/mock_db.hpp"
#include <string>
#include <string_view>

struct Item
{
    orm::property<int, "id">           id;
    orm::property<std::string, "name"> name;
    orm::property<int, "owner">        owner;
};

namespace orm
{
    template <>
    struct table_name_trait<Item>
    {
        static constexpr std::string_view value = "items";
    };
} // namespace orm
