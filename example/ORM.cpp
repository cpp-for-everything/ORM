// ORM.cpp : Defines the entry point for the testing application.
//

#include <iostream>
#include <ORM/ORM.hpp>
#include "users/users.hpp"

using namespace std;
using namespace webframe::ORM::literals;

template <typename T, size_t i> constexpr void create_table_helper()
{
	if constexpr (i >= pfr::tuple_size<T>::value)
	{
		return;
	}
	if constexpr (std::derived_from<decltype(pfr::get<i>(std::declval<T>())), webframe::ORM::details::property>)
	{
		constexpr std::string_view column_name = decltype(pfr::get<i>(std::declval<T>()))::_name();
		using ColumnType = typename decltype(pfr::get<i>(std::declval<T>()))::var_t;

		std::cout << column_name << " " << typeid(ColumnType).name() << std::endl;
	}
	if constexpr (i + 1 < pfr::tuple_size<T>::value)
	{
		create_table_helper<T, i + 1>();
	}
}

template <typename T> void create_table()
{
	create_table_helper<T, 0>();
}

#define my_assert(x)                                                                                                                                           \
	if (!(x))                                                                                                                                                  \
	{                                                                                                                                                          \
		std::cout << "\"" << #x << "\" failed." << std::endl;                                                                                                  \
	}

int main()
{
	std::cout << typeid(P<&UserPost::post> == P<&Post::id>).name();
	User test;
	std::cout << test.id._name() << std::endl;
	test.id = 5;
	std::cout << test.id << std::endl;
	test.id += 10;
	std::cout << test.id << std::endl;
	create_table<User>();
	my_assert(webframe::ORM::Table<User>::get_index_by_column("id"sv) == 0);
	my_assert(webframe::ORM::Table<User>::get_index_by_column("username"sv) == 1);
	my_assert(webframe::ORM::Table<User>::get_index_by_column(decltype(std::declval<User>().username)::_name()) == 1);
	std::cout << webframe::ORM::Table<User>::get_index_by_column("id"sv) << std::endl;
	std::cout << webframe::ORM::Table<User>::get_index_by_column("username"sv) << std::endl;
	User alex = webframe::ORM::Table<User>::tuple_to_struct(webframe::ORM::Table<User>::to_tuple(0, "Alex"));
	my_assert(alex.id == 0);
	my_assert(alex.username == "Alex");
	std::cout << alex.id << " " << std::string(alex.username) << std::endl;

	webframe::ORM::relationship<webframe::ORM::RelationshipTypes::one2one, &User::id> y;

	std::cout << typeid(!(P<&UserPost::author> == P<&User::id> && P<&UserPost::post> == P<&Post::id>)).name() << std::endl;
	std::cout << typeid(!((P<&UserPost::author> == P<&User::id>)^(P<&UserPost::post> == P<&Post::id>))).name() << std::endl;
	MockDB::MockDB db;
	std::cout << (db << Utils<User>::insert_new_user)("Name") << std::endl;
	std::cout << (db << Utils<User>::insert_new_user_with_id_placeholder)(5, "Name") << std::endl;
	std::cout << (db << Utils<User>::insert_new_user_with_id_placeholder)(5, "Name", 6, "alex") << std::endl;
	std::cout << (db << Utils<UserPost>::get_all_posts_with_their_assosiated_users)() << std::endl;
	std::cout << (db << Utils<User>::get_all_users_with_id_above)(5) << std::endl;
	std::cout << (db << Utils<User>::update_something)(5, 10) << std::endl;
	std::cout << (db << Utils<User>::update_with_optimized_rules)(5, 10) << std::endl;
	return 0;
}
