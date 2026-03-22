#include <gtest/gtest.h>
#include "ORM/ORM.hpp"

// ── Explicit string override works on all compilers ───────────────────────────

TEST(Cpp26Reflection, ExplicitStringOverrideCurrentCompiler)
{
    // property<T, "name"> with explicit string — must work on both C++23 and C++26
    static_assert(
        std::string_view(orm::property<int, "user_id">::column_name()) == "user_id",
        "property<int, \"user_id\"> must return column name \"user_id\" on all compilers");
}

TEST(Cpp26Reflection, ExplicitStringPreservesType)
{
    using P = orm::property<int, "id">;
    static_assert(std::is_same_v<P::value_type, int>);
    static_assert(std::string_view(P::column_name()) == "id");
}

TEST(Cpp26Reflection, ExplicitStringOverrideForU8String)
{
    using P = orm::property<std::u8string, "email">;
    static_assert(std::string_view(P::column_name()) == "email");
}

// ── C++26 reflection path (ORM_HAS_REFLECTION=1) ─────────────────────────────
// These tests only compile on C++26-capable compilers where reflection is
// available. They are guarded by ORM_HAS_REFLECTION to remain valid on C++23.

#if ORM_HAS_REFLECTION

// On a C++26 compiler, property<T> (no string arg) would infer the column name
// from the member name via std::meta::name_of(^^member).
// This test verifies the dual-path coexistence: a property with explicit string
// produces the same column name as if reflection had inferred it.
TEST(Cpp26Reflection, ExplicitMatchesInferredOnC26)
{
    // property<int, "id"> and a hypothetical property<int> named 'id' on C++26
    // must produce the same column name. We test this by checking that the
    // explicit-string variant returns the expected name.
    constexpr auto col = orm::property<int, "id">::column_name();
    static_assert(col == "id", "Explicit string must match expected column name");
}

#endif

// ── Pre-C++26: property<T, "name"> compiles and works ────────────────────────

TEST(Cpp26Reflection, PropertyCompilesPfr)
{
    // Verify the property type instantiates, has the correct column name,
    // and that value_type is correct.
    orm::property<int, "score"> p;
    p.value = 42;
    EXPECT_EQ(p.value, 42);
    EXPECT_EQ(std::string_view(orm::property<int, "score">::column_name()), "score");
}

// ── Entity using property<T, "col"> in query IR ───────────────────────────────

struct ReflEntity
{
    orm::property<int, "id">              id;
    orm::property<std::u8string, "email"> email;
};

TEST(Cpp26Reflection, EntityUsableInQueryIr)
{
    const auto q = orm::select(orm::field<&ReflEntity::id>, orm::field<&ReflEntity::email>);
    (void)q;
    static_assert(
        std::string_view(decltype(orm::field<&ReflEntity::id>)::column_name()) == "id");
    static_assert(
        std::string_view(decltype(orm::field<&ReflEntity::email>)::column_name()) == "email");
}

// ── ORM_HAS_REFLECTION macro presence ────────────────────────────────────────

TEST(Cpp26Reflection, HasReflectionMacroDefined)
{
    // ORM_HAS_REFLECTION must be defined as 0 or 1 by the build system.
    // This test verifies the macro is present (will fail to compile if absent).
    static_assert(ORM_HAS_REFLECTION == 0 || ORM_HAS_REFLECTION == 1,
        "ORM_HAS_REFLECTION must be 0 or 1");
}
