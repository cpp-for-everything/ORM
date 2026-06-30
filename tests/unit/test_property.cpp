#include <gtest/gtest.h>
#include "ORM/ORM.hpp"

struct SampleEntity
{
    orm::property<int, "id">             id;
    orm::property<std::u8string, "name"> name;
    orm::property<double, "score">       score;
};

// ── property column_name ──────────────────────────────────────────────────────

TEST(Property, ColumnNameInt)
{
    using P = orm::property<int, "user_id">;
    EXPECT_EQ(P::column_name(), "user_id");
}

TEST(Property, ColumnNameFixedString)
{
    using P = orm::property<orm::fixed_string<50>, "code">;
    EXPECT_EQ(P::column_name(), "code");
}

TEST(Property, ColumnNameDatetime)
{
    using P = orm::property<orm::datetime, "created_at">;
    EXPECT_EQ(P::column_name(), "created_at");
}

// ── property value_type ───────────────────────────────────────────────────────

TEST(Property, NativeTypeInt)
{
    static_assert(std::is_same_v<orm::property<int, "id">::value_type, int>);
}

TEST(Property, NativeTypeU8String)
{
    static_assert(std::is_same_v<orm::property<std::u8string, "name">::value_type, std::u8string>);
}

// ── property value roundtrip ──────────────────────────────────────────────────

TEST(Property, ValueRoundtripInt)
{
    orm::property<int, "id"> p;
    p.value = 42;
    EXPECT_EQ(p.value, 42);
}

TEST(Property, ExplicitConstructor)
{
    orm::property<int, "id"> p(99);
    EXPECT_EQ(p.value, 99);
}

// ── optional / engaged semantics (partial hydration) ──────────────────────────

TEST(Property, DefaultIsUnset)
{
    orm::property<int, "id"> p;
    EXPECT_FALSE(p.has_value());
    EXPECT_FALSE(static_cast<bool>(p));
}

TEST(Property, ValueConstructorEngages)
{
    orm::property<int, "id"> p(7);
    EXPECT_TRUE(p.has_value());
    EXPECT_EQ(p.get(), 7);
}

TEST(Property, SetEngages)
{
    orm::property<int, "id"> p;
    p.set(5);
    EXPECT_TRUE(p.has_value());
    EXPECT_EQ(p.value, 5);
}

TEST(Property, ResetDisengages)
{
    orm::property<int, "id"> p(5);
    p.reset();
    EXPECT_FALSE(p.has_value());
    EXPECT_EQ(p.value, 0);
}

// ── is_property concept ───────────────────────────────────────────────────────

TEST(Property, IsPropertyTrue)
{
    static_assert(orm::is_property<orm::property<int, "id">>);
}

TEST(Property, IsPropertyFalseForInt)
{
    static_assert(!orm::is_property<int>);
}

TEST(Property, IsPropertyFalseForStruct)
{
    static_assert(!orm::is_property<SampleEntity>);
}

// ── is_entity concept ─────────────────────────────────────────────────────────

TEST(Entity, IsEntityTrueForAggregate)
{
    static_assert(orm::is_entity<SampleEntity>);
}

TEST(Entity, IsEntityFalseForInt)
{
    static_assert(!orm::is_entity<int>);
}

// ── table_name ────────────────────────────────────────────────────────────────

struct UserEntity
{
    orm::property<int, "id"> id;
};

namespace orm
{
    template <>
    struct table_name_trait<UserEntity>
    {
        static constexpr std::string_view value = "users";
    };
} // namespace orm

TEST(Table, CustomTableName)
{
    EXPECT_EQ(orm::table_name<UserEntity>(), "users");
}
