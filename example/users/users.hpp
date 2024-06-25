#pragma once
#include <ORM/ORM.hpp>
#include <string>

using namespace webframe::ORM;

template <typename T> class Utils;

struct Post
{
	static constexpr std::string_view table_name = "Posts";
	property<INTEGER<>, "id"> id;
	property<TEXT<>, "content"> content;
};

struct User
{
	static constexpr std::string_view table_name = "Users";
	property<INTEGER<>, "id"> id;
	property<TEXT<>, "username"> username;
	relationship<RelationshipTypes::one2many, &Post::id, "post_ids"> posts;
};

struct UserPost
{
	static constexpr std::string_view table_name = "UserPosts";
	property<Nullable<INTEGER<>>, "id"> id;
	relationship<RelationshipTypes::one2one, &User::id, "user_id"> author;
	relationship<RelationshipTypes::one2one, &Post::id, "post_id"> post;
};

template <> class Utils<User> : public webframe::ORM::Table<User>
{
	public:
	static constexpr auto insert_new_user_with_id_placeholder = webframe::ORM::insert<&User::id, &User::username>;
	static constexpr auto insert_new_user = webframe::ORM::insert<&User::username>;
	static constexpr auto get_all_users_with_id_above =
		webframe::ORM::select<modes::ALL, &User::id, &User::username>.where(P<&User::id> > P<Placeholder<int>>).limit(15_per_page & 5_page).limit(5_page & 15_per_page);
	static constexpr auto update_with_optimized_rules = webframe::ORM::update(P<& User::id> = P<Placeholder<int>>, P<& User::username> = "Test")
															.where(!(P<&User::id> > P<Placeholder<int>>))
															.order_by<&User::id, modes::ASC>()
															.limit(5_page & 15_per_page);
	static constexpr auto update_something =
		webframe::ORM::update(P<& User::id> = P<Placeholder<int>>, P<& User::username> = "Test").where(P<&User::id> > P<Placeholder<int>>);
	static constexpr auto update_something_2 =
		webframe::ORM::update(P<& User::id> = P<Placeholder<int>>).update(P<& User::username> = "Test").where(P<&User::id> > P<Placeholder<int>>);
};

template <> class Utils<UserPost> : public webframe::ORM::Table<UserPost>
{
	public:
	static constexpr auto get_all_posts_with_their_assosiated_users = 
        webframe::ORM::select<modes::DISTINCT, &UserPost::author, &UserPost::id, &User::id, &Post::id, &Post::content, &UserPost::author>
            .join<modes::FULL, Post>(!(P<&UserPost::post> == P<&Post::id> && P<&UserPost::post> != P<&Post::id>))
            .limit(15_per_page & 5_page)
            .limit(5_page & 15_per_page)
            .group_by<&UserPost::id>(P<&Post::id> == 5)
            .group_by<&UserPost::id>()
            .order_by<&UserPost::id, modes::ASC, &UserPost::id, modes::DESC, &UserPost::id, modes::DEFAULT, &UserPost::id>()
    ;
	static constexpr auto delete_all_posts = 
        webframe::ORM::deleteq<UserPost>
            .where(P<&UserPost::id> > P<Placeholder<int>>)
            .where(P<&UserPost::id> != P<nullptr>)
            .order_by<&UserPost::id, modes::ASC, &UserPost::id, modes::DESC, &UserPost::id, modes::DEFAULT, &UserPost::id>()
            .limit(5_page & 15_per_page)
    ;
};
