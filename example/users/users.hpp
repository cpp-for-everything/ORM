#pragma once
#include <ORM/ORM.hpp>
#include <string>

using namespace webframe::ORM;
using namespace webframe::ORM::placeholders;

struct User
{
	static constexpr std::string_view entity_name = "Users";
	property<INT, "id"> id;
	property<TEXT<>, "name"> name;

	static constexpr auto insert_new_user_with_name =
		webframe::ORM::insert<&User::name>.values(_1<std::string>).on_duplicate_key_update(DB<& User::name> += "_copy");

	static constexpr auto insert_into_select_test =
		webframe::ORM::insert<&User::name>.into(insert_new_user_with_name).on_duplicate_key_update(DB<& User::name> += "_copy");

	static constexpr auto update_test = webframe::ORM::update<User>.where(DB<&User::name> == "_copy").order_by<&User::name, ASC>().order_by<&User::id>();
};
