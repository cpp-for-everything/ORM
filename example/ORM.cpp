// ORM.cpp : Defines the entry point for the testing application.
//

#include <iostream>
#include <ORM/ORM.hpp>
#include "users/users.hpp"

using namespace std;
using namespace webframe::ORM::literals;

int main()
{
	User test;
	std::cout << test.id.name() << std::endl;
	test.id = 5;
	std::cout << test.id << std::endl;
	test.id += 10;
	std::cout << test.id << std::endl;

	std::apply([](auto&&... args) {((std::cout << typeid(args).name() << std::endl), ...);}, decltype(User::insert_new_user_with_name)::parameters_type());
	std::apply([](auto&&... args) {((std::cout << typeid(args).name() << std::endl), ...);}, User::insert_new_user_with_name.get_columns());
	std::apply([](auto&&... args) {((std::cout << typeid(args).name() << std::endl), ...);}, User::insert_new_user_with_name.get_values());
	std::apply([](auto&&... args) {((std::cout << typeid(args).name() << std::endl), ...);}, User::insert_new_user_with_name.get_update_statements());
	std::cout << std::endl;
	std::apply([](auto&&... args) {((std::cout << typeid(args).name() << std::endl), ...);}, decltype(User::insert_into_select_test)::parameters_type());
	std::apply([](auto&&... args) {((std::cout << typeid(args).name() << std::endl), ...);}, User::insert_into_select_test.get_columns());
	std::apply([](auto&&... args) {((std::cout << typeid(args).name() << std::endl), ...);}, User::insert_into_select_test.get_update_statements());
	std::cout << std::endl;
	std::apply([](auto&&... args) {((std::cout << typeid(args).name() << std::endl), ...);}, User::update_test.get_orders());
	
	return 0;
}
