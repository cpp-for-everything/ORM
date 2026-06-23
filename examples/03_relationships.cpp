// ============================================================================
//  ПРИМЕР 03 — Релационно-осъзнати заявки (relationship → auto-JOIN)
// ----------------------------------------------------------------------------
//  `orm::relationship<store_as::reference<&Target::pk>, "fk_col">` дефинира
//  истинска колона за външен ключ И носи метаданни за свързване. Когато една
//  заявка избере поле от свързана таблица, компилаторът автоматично извежда
//  нужния JOIN (включително многозвенно: Comment → Post → User), а видът на
//  JOIN-а (INNER/LEFT/RIGHT/FULL) се определя от field / optional_field.
//
//  Команда:  c++ -std=c++23 -I lib/include 03_relationships.cpp -o rel && ./rel
// ============================================================================
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MockDB/mock_db.hpp"
#include "ORM/connector/db.hpp"

#include <iostream>
#include <string>

// ── обектен модел с външни ключове ──────────────────────────────────────────
struct User
{
    orm::property<int,          "id">   id;
    orm::property<std::u8string, "name"> name;
};

struct Post
{
    orm::property<int,          "id">   id;
    orm::property<std::u8string, "body"> body;
    // Post.user_id е външен ключ към User.id.
    orm::relationship<orm::store_as::reference<&User::id>, "user_id"> user_id;
};

struct Comment
{
    orm::property<int,          "id">   id;
    orm::property<std::u8string, "body"> body;
    // Comment.post_id е външен ключ към Post.id.
    orm::relationship<orm::store_as::reference<&Post::id>, "post_id"> post_id;
};

template <> struct orm::table_name_trait<User>    { static constexpr std::string_view value = "user"; };
template <> struct orm::table_name_trait<Post>    { static constexpr std::string_view value = "posts"; };
template <> struct orm::table_name_trait<Comment> { static constexpr std::string_view value = "comments"; };

int main()
{
    // ── relationship-ът е истинска FK колона, проверена при компилация ──────
    using FK = decltype(Post::user_id);
    static_assert(orm::is_reference_relationship_v<FK>);
    static_assert(std::is_same_v<FK::target_table, User>);
    static_assert(std::is_same_v<FK::value_type, int>);
    static_assert(FK::column_name() == "user_id" && FK::target_column() == "id");

    orm::MockDB           conn;
    orm::db<orm::MockDB>  db{conn};
    auto show = [&](const char* label, auto q)
    {
        db << q;
        std::cout << label << ":\n  " << conn.last_sql << "\n";
    };

    show("INNER (избрано поле от User → задължителен JOIN)",
         orm::select(orm::field<&Post::id>, orm::field<&Post::body>, orm::field<&User::name>));

    show("LEFT (optional_field → User може да липсва)",
         orm::select(orm::field<&Post::id>, orm::optional_field<&User::name>));

    show("Многозвенно (Comment → Post → User, posts се присъединява автоматично)",
         orm::select(orm::field<&Comment::body>, orm::field<&User::name>));

    show("Една таблица (без JOIN)",
         orm::select(orm::field<&Post::id>, orm::field<&Post::body>));

    return 0;
}
