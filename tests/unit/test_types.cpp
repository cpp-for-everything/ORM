#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include <chrono>

// ── fixed_string ──────────────────────────────────────────────────────────────

TEST(FixedString, MaxSizeTemplateParam)
{
    static_assert(orm::fixed_string<10>::max_size == 10);
}

TEST(FixedString, ConstructFromU8StringView)
{
    orm::fixed_string<10> s(u8"hello");
    EXPECT_EQ(s.as_u8string_view(), u8"hello");
    EXPECT_EQ(s.size(), 5u);
}

TEST(FixedString, TruncatesAtMaxSize)
{
    orm::fixed_string<3> s(u8"hello");
    EXPECT_EQ(s.size(), 3u);
}

TEST(FixedString, EqualityComparison)
{
    orm::fixed_string<10> a(u8"abc");
    orm::fixed_string<10> b(u8"abc");
    orm::fixed_string<10> c(u8"xyz");
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

// ── binary ────────────────────────────────────────────────────────────────────

TEST(Binary, FixedSizeTemplateParam)
{
    static_assert(orm::binary<16>::fixed_size == 16);
}

TEST(Binary, ConstructFromArray)
{
    std::array<uint8_t, 4> data{0x01, 0x02, 0x03, 0x04};
    orm::binary<4> b(data);
    EXPECT_EQ(b.data()[0], 0x01);
    EXPECT_EQ(b.size(), 4u);
}

// ── varbinary ─────────────────────────────────────────────────────────────────

TEST(Varbinary, MaxSizeTemplateParam)
{
    static_assert(orm::varbinary<255>::max_size == 255);
}

TEST(Varbinary, ConstructFromVector)
{
    std::vector<uint8_t> v{0xAA, 0xBB};
    orm::varbinary<255> vb(v);
    EXPECT_EQ(vb.size(), 2u);
    EXPECT_EQ(vb.data()[1], 0xBB);
}

// ── chrono aliases ────────────────────────────────────────────────────────────

TEST(ChronoAliases, DatetimeIsSystemClockTimePoint)
{
    static_assert(std::is_same_v<orm::datetime, std::chrono::system_clock::time_point>);
}

TEST(ChronoAliases, HasUtcClockMacroDefined)
{
    static_assert(ORM_HAS_UTC_CLOCK == 0 || ORM_HAS_UTC_CLOCK == 1);
}

TEST(ChronoAliases, TimestampUsesAvailableClock)
{
#if ORM_HAS_UTC_CLOCK
    static_assert(std::is_same_v<orm::timestamp, std::chrono::utc_clock::time_point>);
#else
    static_assert(std::is_same_v<orm::timestamp, std::chrono::system_clock::time_point>);
#endif
}

TEST(ChronoAliases, DateIsYearMonthDay)
{
    static_assert(std::is_same_v<orm::date, std::chrono::year_month_day>);
}

TEST(ChronoAliases, TimeOfDayIsHhMmSs)
{
    static_assert(std::is_same_v<orm::time_of_day, std::chrono::hh_mm_ss<std::chrono::seconds>>);
}

TEST(ChronoAliases, YearIsChronoYear)
{
    static_assert(std::is_same_v<orm::year, std::chrono::year>);
}

// ── enum_t ────────────────────────────────────────────────────────────────────

TEST(EnumT, AcceptsValidValue)
{
    using Status = orm::enum_t<u8"active", u8"banned", u8"pending">;
    Status s(u8"active");
    EXPECT_EQ(s.value(), u8"active");
}

TEST(EnumT, RejectsInvalidValue)
{
    using Status = orm::enum_t<u8"active", u8"banned">;
    EXPECT_THROW(Status(u8"unknown"), std::invalid_argument);
}

TEST(EnumT, IsValidCheck)
{
    using Status = orm::enum_t<u8"a", u8"b">;
    EXPECT_TRUE(Status::is_valid(u8"a"));
    EXPECT_FALSE(Status::is_valid(u8"c"));
}

// ── set_t ─────────────────────────────────────────────────────────────────────

TEST(SetT, AcceptsValidSubset)
{
    using Perms = orm::set_t<u8"read", u8"write", u8"admin">;
    Perms p({u8"read", u8"write"});
    EXPECT_TRUE(p.contains(u8"read"));
    EXPECT_FALSE(p.contains(u8"admin"));
}

TEST(SetT, RejectsUnknownValue)
{
    using Perms = orm::set_t<u8"read", u8"write">;
    EXPECT_THROW((Perms({u8"read", u8"execute"})), std::invalid_argument);
}
