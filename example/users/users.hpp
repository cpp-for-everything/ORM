#pragma once
#include <ORM/ORM.hpp>
#include <string>

using namespace webframe::ORM;
using namespace webframe::ORM::literals;

struct User
{
	static constexpr std::string_view entity_name = "Users";
	property<INT, "id"> id;
	property<TEXT<>, "name"> name;
	
	static constexpr auto insert_new_user_with_name = webframe::ORM::insert<&User::name>
		.values(std::placeholders::_1)
		.on_duplicate_key_update(DB<&User::name> += "_copy")
	;
	static constexpr auto insert_into_select_tesst = webframe::ORM::insert<&User::name>
		.into(5)
		.on_duplicate_key_update(DB<&User::name> += "_copy")
	;
};
