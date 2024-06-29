#pragma once
#include <ORM/ORM.hpp>
#include <string>

using namespace webframe::ORM;

template<typename T>
class Utils;

struct Post {
    static constexpr std::string_view entity_name = "Posts";
    property<int, "id"> id;
    property<std::string, "content"> content;
};

struct User {
    static constexpr std::string_view entity_name = "Users";
    property<int, "id"> id;
    property<std::string, "username"> username;
    relationship<RelationshipTypes::one2many, &Post::id> posts;
};

struct UserPost {
    static constexpr std::string_view entity_name = "UserPosts";
    property<int, "id"> id;
    relationship<RelationshipTypes::one2one, &User::id> author;
    relationship<RelationshipTypes::one2one, &Post::id> post;
};

template<>
class Utils<User> : public webframe::ORM::Table<User> {
public:
    static constexpr auto insert_new_user_with_id_placeholder = webframe::ORM::insert<Placeholder<int>, &User::username>;
    static constexpr auto insert_new_user = webframe::ORM::insert<&User::username>;
    static constexpr auto get_all_users_with_id_above = webframe::ORM::select<&User::id, &User::username>
        .where(P<&User::id> > P<Placeholder<int>>)
        .limit(15_per_page & 5_page)
        .limit(5_page & 15_per_page)
    ;
    static constexpr auto update_with_optimized_rules = webframe::ORM::update(P<&User::id> = P<Placeholder<int>>, P<&User::username> = "Test")
        .where(!(P<&User::id> > P<Placeholder<int>>))
    ;
    static constexpr auto update_something = webframe::ORM::update(P<&User::id> = P<Placeholder<int>>, P<&User::username> = "Test")
        .where(P<&User::id> > P<Placeholder<int>>)
    ;
    static constexpr auto update_something_2 = webframe::ORM::update(P<&User::id> = P<Placeholder<int>>)
        .update(P<&User::username> = "Test")
        .where(P<&User::id> > P<Placeholder<int>>)
    ;
};

template<>
class Utils<UserPost> : public webframe::ORM::Table<UserPost> {
public:
    static constexpr auto get_all_posts_with_their_assosiated_users = webframe::ORM::select<&UserPost::id, &User::id, &User::username, &Post::id, &Post::content>
        .join(P<&UserPost::post> == P<&Post::id>)
        .limit(15_per_page & 5_page)
        .limit(5_page & 15_per_page)
    ;
};
