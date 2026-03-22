#include <gtest/gtest.h>
#include "ORM/ORM.hpp"
#include "ORM/db/connectors/MockDB/mock_db.hpp"
#include "ORM/db/connectors/WireProtocol/wire_protocol.hpp"

// ── Test entity ───────────────────────────────────────────────────────────────

struct WireUser
{
    orm::property<int, "id">   id;
    orm::property<int, "age">  age;
};

// ── io_uring_awaitable: await_ready() returns false ───────────────────────────

#ifdef __linux__
TEST(WireProtocol, AwaitReadyReturnsFalse)
{
    orm::MockDB db;
    const auto q = orm::select(orm::field<&WireUser::id>);
    orm::io_uring_awaitable<orm::MockDB, decltype(q)> aw{db, q};

    EXPECT_FALSE(aw.await_ready())
        << "io_uring_awaitable::await_ready() must always return false";
}
#endif

#ifdef _WIN32
TEST(WireProtocol, IocpAwaitReadyReturnsFalse)
{
    orm::MockDB db;
    const auto q = orm::select(orm::field<&WireUser::id>);
    orm::iocp_awaitable<orm::MockDB, decltype(q)> aw{db, q};
    EXPECT_FALSE(aw.await_ready());
}
#endif

// ── batch_insert: empty batch → no execute call ───────────────────────────────

TEST(WireProtocol, BatchInsertEmptyNoOp)
{
    orm::MockDB db;
    const auto q = orm::insert(orm::field<&WireUser::id>, orm::field<&WireUser::age>);

    orm::batch_insert<orm::MockDB, decltype(q.signature())> bi;

    EXPECT_TRUE(bi.empty());
    bi.execute(db, q);

    EXPECT_EQ(bi.execute_count(), 0)
        << "Empty batch must not call execute on the connector";
    EXPECT_EQ(db.last_sql, "")
        << "Empty batch must not modify last_sql";
}

// ── batch_insert: N rows → exactly 1 execute call ────────────────────────────

TEST(WireProtocol, BatchInsertNRowsOneExecute)
{
    orm::MockDB db;
    const auto q = orm::insert(orm::field<&WireUser::id>, orm::field<&WireUser::age>);

    orm::batch_insert<orm::MockDB, decltype(q.signature())> bi;
    for (int i = 0; i < 5; ++i)
        bi.add_row({"id_val", "age_val"});

    EXPECT_EQ(bi.size(), 5u);
    bi.execute(db, q);

    EXPECT_EQ(bi.execute_count(), 1)
        << "Non-empty batch must call execute exactly once regardless of row count";
}

// ── zero_copy_result: span data() equals buffer + offset ─────────────────────

TEST(WireProtocol, ZeroCopySpanDataEqualsBufferPlusOffset)
{
    // Simulate a driver buffer with two columns at byte offsets 0 and 4
    std::array<std::byte, 8> driver_buf{};
    driver_buf[0] = std::byte{42};
    driver_buf[4] = std::byte{7};

    std::size_t offsets[] = {0, 4};
    std::size_t lengths[] = {4, 4};

    orm::zero_copy_result<4> zcr{driver_buf.data(), 2, offsets, lengths};

    auto sp0 = zcr.span<0>();
    auto sp1 = zcr.span<1>();

    EXPECT_EQ(sp0.data(), driver_buf.data())
        << "span<0> must point to buffer + offset[0]";
    EXPECT_EQ(sp1.data(), driver_buf.data() + 4)
        << "span<1> must point to buffer + offset[1]";
    EXPECT_EQ(sp0.size(), 4u);
    EXPECT_EQ(sp1.size(), 4u);

    // Verify value access (no copy at span<> call site)
    EXPECT_EQ(sp0[0], std::byte{42});
    EXPECT_EQ(sp1[0], std::byte{7});
}

// ── make_constexpr_sql: non-empty result for supports_constexpr_sql connector ─

TEST(WireProtocol, ConstexprSqlNotEmpty)
{
    constexpr auto q   = orm::select(orm::field<&WireUser::id>);
    constexpr auto sql = orm::make_constexpr_sql<orm::MockDB>(q);

    static_assert(sql[0] != '\0',
        "make_constexpr_sql must return a non-empty constexpr string for MockDB");
    EXPECT_NE(sql[0], '\0');
}

// ── supports_constexpr_sql capability present on MockDB ───────────────────────

TEST(WireProtocol, MockDbSupportsConstexprSql)
{
    static_assert(
        requires { typename orm::connector_trait<orm::MockDB>::supports_constexpr_sql; },
        "MockDB must declare supports_constexpr_sql for wire protocol tests");
}
