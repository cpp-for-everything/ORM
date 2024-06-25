#pragma once

#include "../../lib/moka/moka.h"

#include <ORM/ORM.hpp>

using namespace webframe::ORM;

namespace TableTests
{
	struct User
	{
		property<int, "id"> id;
		property<std::string, "name"> name;
	};
	struct UserWithNonProperties
	{
		property<int, "id"> id;
		property<std::string, "name"> name;
		int not_property_but_part_of_the_struct;
	};
	struct UserWithNonPropertiesFirst
	{
		int not_property_but_part_of_the_struct;
		property<int, "id"> id;
		property<std::string, "name"> name;
	};
	class UserAsClass
	{
		public:
		property<int, "id"> id;
		property<std::string, "name"> name;
	};
	class UserAsClassWithNonProperties
	{
		public:
		property<int, "id"> id;
		property<std::string, "name"> name;
		int not_property_but_part_of_the_struct;
	};
	class UserAsClassWithNonPropertiesFirst
	{
		public:
		int not_property_but_part_of_the_struct;
		property<int, "id"> id;
		property<std::string, "name"> name;
	};
	struct UserWithStaticConstexprMembers
	{
		property<int, "id"> id;
		property<std::string, "name"> name;
		static constexpr auto sample_query = 1;
	};

	void init(Moka::Context& it)
	{
		it.describe("Static table functionalities", [](Moka::Context& it) {
			it.describe_many<User, UserWithNonProperties, UserAsClass, UserAsClassWithNonProperties, UserWithStaticConstexprMembers>(
				"for %s",
				{"standard struct",
				 "struct with custom non-DB data",
				 "class with public properties",
				 "class with custom non-DB data",
				 "struct with static constexpr members"},
				[]<typename T>(Moka::Context& it) {
					it.should("correctly find the 1st property", []() {
						must_be_equal(
							Table<T>::get_index_by_column("id"), 0ull, "get_index_by_column could not identify correctly the index of the first column.");
					});
					it.should("correctly find the 2nd property", []() {
						must_be_equal(
							Table<T>::get_index_by_column("name"), 1ull, "get_index_by_column could not identify correctly the index of the second column.");
					});
				});
			it.describe_many<UserWithNonPropertiesFirst, UserAsClassWithNonPropertiesFirst>(
				"for %s", {"struct with custom non-DB data first", "struct with custom non-DB data first"}, []<typename T>(Moka::Context& it) {
					it.should("correctly find the 1st property", []() {
						must_be_equal(
							Table<T>::get_index_by_column("id"), 1ull, "get_index_by_column could not identify correctly the index of the first column.");
					});
					it.should("correctly find the 2nd property", []() {
						must_be_equal(
							Table<T>::get_index_by_column("name"), 2ull, "get_index_by_column could not identify correctly the index of the second column.");
					});
				});
		});
	}
} // namespace TableTests
