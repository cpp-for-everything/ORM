#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MockDB/mock_db.hpp"
#include "ORM/connector/db.hpp"
#include "ORM/result/joined_row.hpp"
#include <string>
#include <tuple>

namespace
{
    struct User
    {
        orm::property<int, "id">             id;
        orm::property<std::u8string, "name"> name;
    };
    struct Post
    {
        orm::property<int, "id">             id;
        orm::property<std::u8string, "body"> body;
        orm::relationship<orm::store_as::reference<&User::id>, "user_id"> user_id;
    };
    struct Comment
    {
        orm::property<int, "id">             id;
        orm::property<std::u8string, "body"> body;
        orm::relationship<orm::store_as::reference<&Post::id>, "post_id"> post_id;
    };
} // namespace

template <> struct orm::table_name_trait<User>    { static constexpr std::string_view value = "user"; };
template <> struct orm::table_name_trait<Post>    { static constexpr std::string_view value = "posts"; };
template <> struct orm::table_name_trait<Comment> { static constexpr std::string_view value = "comments"; };

namespace d = orm::detail;

template <class TL> struct head_of;
template <class H, class... T> struct head_of<d::tl<H, T...>> { using type = H; };
template <class TL> struct nth1_of;
template <class H0, class H1, class... T> struct nth1_of<d::tl<H0, H1, T...>> { using type = H1; };

// ── compile-time inference ────────────────────────────────────────────────────

TEST(JoinInfer, DistinctTablesBaseIsFirst)
{
    using R = decltype(orm::select(orm::field<&Post::id>, orm::field<&User::name>))::response_type;
    static_assert(std::is_same_v<d::base_table_t<R>, Post>);
    static_assert(d::is_multi_table<R>);
}

TEST(JoinInfer, SingleHopInner)
{
    using R = decltype(orm::select(orm::field<&Post::id>, orm::field<&User::name>))::response_type;
    using P = d::join_plan_t<R>;
    static_assert(d::tl_size<P>::value == 1);
    using S = head_of<P>::type;
    static_assert(S::mode == orm::join::mode::inner);
    static_assert(std::is_same_v<S::from, Post> && std::is_same_v<S::to, User>);
}

TEST(JoinInfer, JoinTypeMatrix)
{
    using RL = decltype(orm::select(orm::field<&Post::id>, orm::optional_field<&User::name>))::response_type;
    static_assert(head_of<d::join_plan_t<RL>>::type::mode == orm::join::mode::left);

    using RR = decltype(orm::select(orm::optional_field<&Post::id>, orm::field<&User::name>))::response_type;
    static_assert(head_of<d::join_plan_t<RR>>::type::mode == orm::join::mode::right);

    using RF = decltype(orm::select(orm::optional_field<&Post::id>, orm::optional_field<&User::name>))::response_type;
    static_assert(head_of<d::join_plan_t<RF>>::type::mode == orm::join::mode::full);
}

TEST(JoinInfer, MultiHopPullsIntermediate)
{
    using R = decltype(orm::select(orm::field<&Comment::body>, orm::field<&User::name>))::response_type;
    using P = d::join_plan_t<R>;
    static_assert(d::tl_size<P>::value == 2);
    using S0 = head_of<P>::type;
    using S1 = nth1_of<P>::type;
    static_assert(std::is_same_v<S0::from, Comment> && std::is_same_v<S0::to, Post>);
    static_assert(std::is_same_v<S1::from, Post>    && std::is_same_v<S1::to, User>);
}

TEST(JoinInfer, SingleTableNoJoins)
{
    using R = decltype(orm::select(orm::field<&Post::id>, orm::field<&Post::body>))::response_type;
    static_assert(!d::is_multi_table<R>);
    static_assert(d::tl_size<d::join_plan_t<R>>::value == 0);
}

// ── rendered SQL via MockDB ────────────────────────────────────────────────────

TEST(JoinInfer, RendersInnerJoin)
{
    orm::MockDB conn; orm::db<orm::MockDB> db{conn};
    db << orm::select(orm::field<&Post::id>, orm::field<&User::name>);
    EXPECT_EQ(conn.last_sql,
        "SELECT posts.id, user.name FROM posts INNER JOIN user ON posts.user_id = user.id");
}

TEST(JoinInfer, RendersLeftJoin)
{
    orm::MockDB conn; orm::db<orm::MockDB> db{conn};
    db << orm::select(orm::field<&Post::id>, orm::optional_field<&User::name>);
    EXPECT_EQ(conn.last_sql,
        "SELECT posts.id, user.name FROM posts LEFT JOIN user ON posts.user_id = user.id");
}

TEST(JoinInfer, RendersMultiHop)
{
    orm::MockDB conn; orm::db<orm::MockDB> db{conn};
    db << orm::select(orm::field<&Comment::body>, orm::field<&User::name>);
    EXPECT_EQ(conn.last_sql,
        "SELECT comments.body, user.name FROM comments INNER JOIN posts ON comments.post_id = posts.id"
        " INNER JOIN user ON posts.user_id = user.id");
}

TEST(JoinInfer, SingleTableUnchanged)
{
    orm::MockDB conn; orm::db<orm::MockDB> db{conn};
    db << orm::select(orm::field<&Post::id>, orm::field<&Post::body>);
    EXPECT_EQ(conn.last_sql, "SELECT id, body FROM ?");
}

// ── partial-entity hydration + joined_row ──────────────────────────────────────

TEST(JoinedRow, HydratePartialLeavesUnselectedUnset)
{
    using Fields = orm::detail::orm_tuple<orm::mem_ptr<&User::id>>;
    User u = orm::hydrate_entity<User, Fields>(std::tuple<int>{42});
    EXPECT_TRUE(u.id.has_value());
    EXPECT_EQ(u.id.value, 42);
    EXPECT_FALSE(u.name.has_value());   // unselected column stays unset
}

TEST(JoinedRow, HydrateMultipleFields)
{
    using Fields = orm::detail::orm_tuple<orm::mem_ptr<&User::id>, orm::mem_ptr<&User::name>>;
    User u = orm::hydrate_entity<User, Fields>(std::tuple<int, std::u8string>{7, u8"alice"});
    EXPECT_TRUE(u.id.has_value());
    EXPECT_TRUE(u.name.has_value());
    EXPECT_EQ(u.id.value, 7);
    EXPECT_EQ(u.name.value, std::u8string(u8"alice"));
}

TEST(JoinedRow, GetByTypeAndIndex)
{
    orm::joined_row<User, Post> row;
    row.get<User>().id.set(1);
    row.get<Post>().id.set(2);
    EXPECT_EQ(row.get<User>().id.value, 1);
    EXPECT_EQ(row.get<0>().id.value, 1);   // by index
    EXPECT_EQ(row.get<Post>().id.value, 2);
    EXPECT_EQ(decltype(row)::size, 2u);
}

TEST(JoinedRow, HydrateJoinedRoutesColumnsToEntities)
{
    // select(field<&Post::id>, field<&Post::body>, field<&User::name>) → joined_row<Post,User>
    using R = decltype(orm::select(orm::field<&Post::id>, orm::field<&Post::body>,
                                   orm::field<&User::name>))::response_type;
    static_assert(std::is_same_v<orm::joined_row_for<R>, orm::joined_row<Post, User>>);

    auto row = orm::hydrate_joined<R>(
        std::tuple<int, std::u8string, std::u8string>{1, u8"hello", u8"alice"});
    EXPECT_EQ(row.get<Post>().id.value, 1);
    EXPECT_TRUE(row.get<Post>().body.has_value());
    EXPECT_EQ(row.get<Post>().body.value, std::u8string(u8"hello"));
    EXPECT_TRUE(row.get<User>().name.has_value());
    EXPECT_EQ(row.get<User>().name.value, std::u8string(u8"alice"));
    EXPECT_FALSE(row.get<User>().id.has_value());   // User.id not selected → unset
}
