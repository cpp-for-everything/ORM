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

	return 0;
}
