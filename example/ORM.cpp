// ORM.cpp : Defines the entry point for the testing application.
//

#include "users/users.hpp"
#include <ORM/ORM.hpp>
#include <iostream>

using namespace std;
using namespace webframe::ORM::literals;

template <typename T, size_t i> constexpr void create_table_helper() {
  if constexpr (i >= pfr::tuple_size<T>::value)
    return;
  if constexpr (std::derived_from<decltype(pfr::get<i>(std::declval<T>())),
                                  webframe::ORM::details::property>) {
    constexpr std::string_view column_name =
        decltype(pfr::get<i>(std::declval<T>()))::_name();
    using ColumnType = typename decltype(pfr::get<i>(std::declval<T>()))::var_t;

    std::cout << MockDB::MockDB::delim
              << /*T::table_name << "." <<*/ column_name << " "
              << typeid(ColumnType).name();
  }
  if constexpr (i + 1 < pfr::tuple_size<T>::value) {
    std::cout << ",";
    create_table_helper<T, i + 1>();
  }
}

template <typename T> void create_table() {
  std::cout << "CREATE TABLE " << T::table_name << "(";
  create_table_helper<T, 0>();
  std::cout << "\n);";
}

#define my_assert(x)                                                           \
  if (!(x)) {                                                                  \
    std::cout << "\"" << #x << "\" failed." << std::endl;                      \
  }

int main() {
  create_table<User>();
  std::cout << std::endl << std::endl;

  User alex = webframe::ORM::Table<User>::tuple_to_struct(
      webframe::ORM::Table<User>::to_tuple((INTEGER<>)(0), (TEXT<>)("Alex")));
  my_assert(alex.id == 0);
  my_assert(alex.username == "Alex");

  MockDB::MockDB db;
  std::cout << (db << Utils<User>::insert_new_user)("Name") << std::endl;
  std::cout << std::endl;
  std::cout << (db << Utils<User>::insert_new_user_with_id_placeholder)(5,
                                                                        "Name")
            << std::endl;
  std::cout << std::endl;
  std::cout << (db << Utils<User>::insert_new_user_with_id_placeholder)(
                   5, "Name", 6, "alex")
            << std::endl;
  std::cout << std::endl;
  std::cout
      << (db << Utils<UserPost>::get_all_posts_with_their_assosiated_users)()
      << std::endl;
  std::cout << std::endl;
  std::cout << (db << Utils<User>::get_all_users_with_id_above)(5) << std::endl;
  std::cout << std::endl;
  std::cout << (db << Utils<User>::update_something)(5, 10) << std::endl;
  std::cout << std::endl;
  std::cout << (db << Utils<User>::update_with_optimized_rules)(5, 10)
            << std::endl;
  std::cout << std::endl;
  std::cout << (db << Utils<UserPost>::delete_all_posts)(5, 5) << std::endl;
  std::cout << std::endl;
  return 0;
}
