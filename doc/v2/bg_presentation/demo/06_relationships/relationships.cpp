// ============================================================================
//  ДЕМО 6 — Релационно-осъзнати заявки (relationship → автоматичен JOIN)
// ----------------------------------------------------------------------------
//  `relationship<store_as::reference<&Target::pk>, "fk_col">` дефинира истинска
//  колона за външен ключ И носи метаданните за свързване. Избор на поле от
//  свързана таблица автоматично извежда нужния JOIN при компилация --- включително
//  многозвенно (Comment → Post → User, с междинна таблица, която не е избрана).
//  Видът на JOIN-а (INNER/LEFT/RIGHT/FULL) идва от field / optional_field.
//
//  Команда (без драйвери):
//    c++ -std=c++23 -I lib/include relationships.cpp -o rel && ./rel
// ============================================================================
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MockDB/mock_db.hpp"
#include "ORM/connector/db.hpp"

#include <iostream>
#include <string>

struct User
{
    orm::property<int,          "id">   id;
    orm::property<std::u8string, "name"> name;
};
struct Post
{
    orm::property<int,          "id">   id;
    orm::property<std::u8string, "body"> body;
    orm::relationship<orm::store_as::reference<&User::id>, "user_id"> user_id;  // FK → User
};
struct Comment
{
    orm::property<int,          "id">   id;
    orm::property<std::u8string, "body"> body;
    orm::relationship<orm::store_as::reference<&Post::id>, "post_id"> post_id;  // FK → Post
};

template <> struct orm::table_name_trait<User>    { static constexpr std::string_view value = "user"; };
template <> struct orm::table_name_trait<Post>    { static constexpr std::string_view value = "posts"; };
template <> struct orm::table_name_trait<Comment> { static constexpr std::string_view value = "comments"; };

int main()
{
    // FK колоната е проверима при компилация:
    using FK = decltype(Post::user_id);
    static_assert(orm::is_reference_relationship_v<FK>);
    static_assert(std::is_same_v<FK::target_table, User> && std::is_same_v<FK::value_type, int>);
    static_assert(FK::column_name() == "user_id" && FK::target_column() == "id");

    orm::MockDB           conn;
    orm::db<orm::MockDB>  db{conn};
    auto show = [&](const char* label, auto q) { db << q; std::cout << label << ":\n  " << conn.last_sql << "\n"; };

    show("INNER  (field<&User::name> налага JOIN)",
         orm::select(orm::field<&Post::id>, orm::field<&User::name>));
    show("LEFT   (optional_field<&User::name>)",
         orm::select(orm::field<&Post::id>, orm::optional_field<&User::name>));
    show("RIGHT  (optional Post, required User)",
         orm::select(orm::optional_field<&Post::id>, orm::field<&User::name>));
    show("FULL   (двете optional)",
         orm::select(orm::optional_field<&Post::id>, orm::optional_field<&User::name>));
    show("MULTI-HOP  Comment -> Post -> User",
         orm::select(orm::field<&Comment::body>, orm::field<&User::name>));

    std::cout << "Един модел; relationship-ите движат заявките --- автоматичен JOIN, "
                 "изведен и проверен при компилация.\n";
    return 0;
}
