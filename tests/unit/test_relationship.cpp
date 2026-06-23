#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include <list>
#include <vector>

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
        // FK column: Post.user_id references User.id.
        orm::relationship<orm::store_as::reference<&User::id>, "user_id"> user_id;
        // embedded collection.
        orm::relationship<orm::store_as::embed, "tags", std::vector<int>>  tags;
    };
} // namespace

// ── inferred_kind ─────────────────────────────────────────────────────────────

TEST(InferredRelationship, SingleStructIsOneToOne)
{
    static_assert(orm::infer_relationship_v<Post> == orm::inferred_kind::one_to_one);
}

TEST(InferredRelationship, VectorIsOneToMany)
{
    static_assert(orm::infer_relationship_v<std::vector<Post>> == orm::inferred_kind::one_to_many);
}

TEST(InferredRelationship, ListIsOneToMany)
{
    static_assert(orm::infer_relationship_v<std::list<Post>> == orm::inferred_kind::one_to_many);
}

// ── reference relationship (acts as the FK column) ─────────────────────────────

TEST(Relationship, ReferenceIsReferenceNotEmbed)
{
    using R = decltype(Post::user_id);
    static_assert(orm::is_relationship_v<R>);
    static_assert(orm::is_reference_relationship_v<R>);
    static_assert(!orm::is_embed_relationship_v<R>);
    static_assert(R::is_reference && !R::is_embed);
}

TEST(Relationship, ReferenceFkColumnMetadata)
{
    using R = decltype(Post::user_id);
    EXPECT_EQ(R::column_name(), "user_id");      // the FK column on Post
    EXPECT_EQ(R::target_column(), "id");         // the PK column on User
    static_assert(std::is_same_v<R::target_table, User>);
    static_assert(std::is_same_v<R::value_type, int>);   // FK C++ type
}

TEST(Relationship, ReferenceUsableAsSelectField)
{
    // field<&Post::user_id> resolves to the FK column name + type.
    static_assert(orm::is_field<orm::mem_ptr<&Post::user_id>>);
    EXPECT_EQ(orm::mem_ptr<&Post::user_id>::column_name(), "user_id");
    static_assert(std::is_same_v<
        orm::detail::field_cpp_type<orm::mem_ptr<&Post::user_id>>::type, int>);
}

// ── embed relationship ─────────────────────────────────────────────────────────

TEST(Relationship, EmbedMetadata)
{
    using R = decltype(Post::tags);
    static_assert(orm::is_embed_relationship_v<R>);
    static_assert(!orm::is_reference_relationship_v<R>);
    static_assert(R::is_embed && !R::is_reference);
    static_assert(R::is_collection);
    static_assert(std::is_same_v<R::element_type, int>);
    EXPECT_EQ(R::field_name(), "tags");
}

TEST(Relationship, EmbedSingleIsNotCollection)
{
    using R = orm::relationship<orm::store_as::embed, "addr", User>;
    static_assert(!R::is_collection);
    static_assert(std::is_same_v<R::element_type, User>);
}

// ── is_relationship trait ─────────────────────────────────────────────────────

TEST(Relationship, IsRelationshipFalseForInt)
{
    static_assert(!orm::is_relationship_v<int>);
}

TEST(Relationship, IsRelationshipFalseForPost)
{
    static_assert(!orm::is_relationship_v<Post>);
}
