#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include <list>

struct Post
{
    orm::property<int, "id">             id;
    orm::property<std::u8string, "body"> body;
};

// ── store_as enum ─────────────────────────────────────────────────────────────

TEST(StoreAs, ValuesAreDifferent)
{
    static_assert(orm::store_as::reference != orm::store_as::embed);
}

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

TEST(InferredRelationship, IntIsOneToOne)
{
    static_assert(orm::infer_relationship_v<int> == orm::inferred_kind::one_to_one);
}

// ── relationship<> ────────────────────────────────────────────────────────────

TEST(Relationship, StoreAsReferenceStrategy)
{
    using R = orm::relationship<orm::store_as::reference, Post, "posts">;
    static_assert(R::strategy == orm::store_as::reference);
}

TEST(Relationship, StoreAsEmbedStrategy)
{
    using R = orm::relationship<orm::store_as::embed, Post, "archived">;
    static_assert(R::strategy == orm::store_as::embed);
}

TEST(Relationship, FieldName)
{
    using R = orm::relationship<orm::store_as::reference, Post, "posts">;
    EXPECT_EQ(R::field_name(), "posts");
}

TEST(Relationship, RelatedType)
{
    using R = orm::relationship<orm::store_as::reference, Post, "posts">;
    static_assert(std::is_same_v<R::related_type, Post>);
}

TEST(Relationship, ElementTypeForSingle)
{
    using R = orm::relationship<orm::store_as::reference, Post, "posts">;
    static_assert(std::is_same_v<R::element_type, Post>);
}

TEST(Relationship, IsCollectionFalseForSingle)
{
    using R = orm::relationship<orm::store_as::reference, Post, "posts">;
    static_assert(!R::is_collection);
}

TEST(Relationship, OneToManyVectorCollection)
{
    using R = orm::relationship<orm::store_as::reference, std::vector<Post>, "likes">;
    static_assert(R::is_collection);
    static_assert(std::is_same_v<R::element_type, Post>);
}

// ── is_relationship trait ─────────────────────────────────────────────────────

TEST(Relationship, IsRelationshipTrue)
{
    using R = orm::relationship<orm::store_as::reference, Post, "posts">;
    static_assert(orm::is_relationship_v<R>);
}

TEST(Relationship, IsRelationshipFalseForInt)
{
    static_assert(!orm::is_relationship_v<int>);
}

TEST(Relationship, IsRelationshipFalseForPost)
{
    static_assert(!orm::is_relationship_v<Post>);
}
