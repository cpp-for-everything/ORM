// Empirical probe: does MariaDB Connector/C's non-blocking _start/_cont API
// work correctly when driven by stackless C++20 coroutines on a plain
// (non-TLS) MariaDB connection?
//
// The test is gated by the environment variable ORM_TEST_MYSQL_LIVE=1. It
// connects to a local MariaDB / MySQL instance on loopback and exercises:
//
//   1. 50 sequential simple queries  (SELECT 1)
//   2. 50 sequential parameterized   queries (users table)
//   3. one large result set (>= 1000 rows) — stresses
//      mysql_store_result_start/_cont
//   4. concurrent: 10 coroutines, each with its own AsyncMySQLDB connection,
//      all sharing one IoContext, each running 10 queries
//
// Correctness is asserted with ASSERT_* (loud failure required by the
// investigation plan — no silent EXPECT_*). Timing is recorded so the
// reviewer can verify the workload actually overlapped.

#include <gtest/gtest.h>
#include "ORM/db/connectors/MySQLDB/mysql_async.hpp"
#include "ORM/async/io_context.hpp"
#include "ORM/async/task.hpp"
#include "ORM/async/thread_pool.hpp"

#include <chrono>
#include <cstdlib>
#include <future>
#include <string>
#include <thread>
#include <vector>

namespace {

bool gating_env_enabled()
{
    const char* g = std::getenv("ORM_TEST_MYSQL_LIVE");
    return g && std::string(g) == "1";
}

struct Endpoint
{
    std::string host;
    unsigned int port;
    std::string user;
    std::string password;
    std::string database;
};

Endpoint env_endpoint()
{
    const char* host = std::getenv("ORM_MYSQL_HOST");
    const char* port = std::getenv("ORM_MYSQL_PORT");
    const char* user = std::getenv("ORM_MYSQL_USER");
    const char* pw   = std::getenv("ORM_MYSQL_PASSWORD");
    const char* db   = std::getenv("ORM_MYSQL_DATABASE");
    return Endpoint{
        host ? host : "127.0.0.1",
        static_cast<unsigned int>(port ? std::stoul(port) : 3306u),
        user ? user : "orm_test",
        pw   ? pw   : "orm_test_password",
        db   ? db   : "orm_test",
    };
}

// Helper: drive a Task to completion on an IoContext running in a
// background thread, blocking the caller until the coroutine returns.
template <typename T>
T run_task(orm::IoContext& ctx, orm::Task<T> task)
{
    std::promise<T> p;
    auto fut = p.get_future();

    if constexpr (std::is_void_v<T>)
    {
        auto wrapper = [&]() -> orm::Task<void> {
            try
            {
                co_await std::move(task);
                p.set_value();
            }
            catch (...)
            {
                p.set_exception(std::current_exception());
            }
            ctx.stop();
        }();
        wrapper.start_detached();
    }
    else
    {
        auto wrapper = [&]() -> orm::Task<void> {
            try
            {
                T v = co_await std::move(task);
                p.set_value(std::move(v));
            }
            catch (...)
            {
                p.set_exception(std::current_exception());
            }
            ctx.stop();
        }();
        wrapper.start_detached();
    }

    ctx.run();
    return fut.get();
}

orm::Task<int> count_select_one(orm::AsyncMySQLDB& db, int rounds)
{
    int total = 0;
    for (int i = 0; i < rounds; ++i)
    {
        MYSQL_RES* res = co_await orm::mysql_async_detail::query_async(db, "SELECT 1");
        if (!res) throw std::runtime_error("SELECT 1 returned null result");
        MYSQL_ROW row = mysql_fetch_row(res);
        if (!row || !row[0]) { mysql_free_result(res); throw std::runtime_error("no row"); }
        total += std::atoi(row[0]);
        mysql_free_result(res);
    }
    co_return total;
}

orm::Task<int> select_by_id_param(orm::AsyncMySQLDB& db, int rounds)
{
    int total = 0;
    for (int i = 1; i <= rounds; ++i)
    {
        std::string sql = "SELECT id, name FROM probe_users WHERE id = " + std::to_string(i);
        MYSQL_RES* res = co_await orm::mysql_async_detail::query_async(db, sql);
        if (!res) throw std::runtime_error("SELECT by id returned null result");
        MYSQL_ROW row = mysql_fetch_row(res);
        if (!row) { mysql_free_result(res); throw std::runtime_error("no row for id " + std::to_string(i)); }
        total += std::atoi(row[0]);
        mysql_free_result(res);
    }
    co_return total;
}

orm::Task<int> large_result_set(orm::AsyncMySQLDB& db)
{
    MYSQL_RES* res = co_await orm::mysql_async_detail::query_async(
        db, "SELECT id, name FROM probe_users ORDER BY id");
    if (!res) throw std::runtime_error("large select returned null");
    int rows = 0;
    while (MYSQL_ROW r = mysql_fetch_row(res))
    {
        if (!r[0]) throw std::runtime_error("null id in large result");
        ++rows;
    }
    mysql_free_result(res);
    co_return rows;
}

orm::Task<int> per_connection_workload(
    orm::IoContext& ctx, Endpoint ep, int rounds)
{
    auto db = co_await orm::AsyncMySQLDB::connect(
        ep.host.c_str(), ep.port,
        ep.user.c_str(), ep.password.c_str(), ep.database.c_str(), ctx);
    int total = 0;
    for (int i = 0; i < rounds; ++i)
    {
        MYSQL_RES* res = co_await orm::mysql_async_detail::query_async(db, "SELECT 1");
        if (!res) throw std::runtime_error("concurrent SELECT 1 returned null");
        MYSQL_ROW row = mysql_fetch_row(res);
        if (!row) { mysql_free_result(res); throw std::runtime_error("concurrent no row"); }
        total += std::atoi(row[0]);
        mysql_free_result(res);
    }
    co_return total;
}

// Schema setup uses the sync C API so we don't accidentally exercise the
// thing under test here.
void prepare_schema(const Endpoint& ep)
{
    MYSQL* m = mysql_init(nullptr);
    ASSERT_NE(m, nullptr);
    if (!mysql_real_connect(m, ep.host.c_str(), ep.user.c_str(),
                            ep.password.c_str(), ep.database.c_str(),
                            ep.port, nullptr, 0))
    {
        std::string err = mysql_error(m);
        mysql_close(m);
        FAIL() << "MySQL sync connect failed: " << err;
    }

    auto do_sql = [&](const char* sql) {
        if (mysql_query(m, sql) != 0)
        {
            std::string err = mysql_error(m);
            mysql_close(m);
            FAIL() << "schema setup failed: " << err << " (sql: " << sql << ")";
        }
    };

    do_sql("DROP TABLE IF EXISTS probe_users");
    do_sql("CREATE TABLE probe_users ("
           "  id   INT PRIMARY KEY,"
           "  name VARCHAR(64)"
           ") ENGINE=InnoDB");

    // Insert 1500 rows: 50 used by the param test, the rest pad the large
    // result-set test above 1000 rows.
    for (int batch = 0; batch < 15; ++batch)
    {
        std::string sql = "INSERT INTO probe_users (id, name) VALUES ";
        for (int j = 0; j < 100; ++j)
        {
            if (j > 0) sql += ", ";
            int id = batch * 100 + j + 1;
            sql += "(" + std::to_string(id) + ", 'name_" + std::to_string(id) + "')";
        }
        do_sql(sql.c_str());
    }

    mysql_close(m);
}

} // namespace

class MySQLAsyncProbe : public ::testing::Test
{
protected:
    Endpoint ep;
    std::unique_ptr<orm::IoContext> ctx;

    void SetUp() override
    {
        if (!gating_env_enabled())
            GTEST_SKIP() << "ORM_TEST_MYSQL_LIVE!=1 — skipping live MySQL async probe.";
        ep = env_endpoint();
        prepare_schema(ep);
        ctx = orm::IoContext::create(1);
        ASSERT_NE(ctx, nullptr);
    }
};

// 1) 50 sequential simple queries on one async connection.
TEST_F(MySQLAsyncProbe, FiftySequentialSelectOne)
{
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();

    int total = run_task(*ctx, [&]() -> orm::Task<int> {
        auto db = co_await orm::AsyncMySQLDB::connect(
            ep.host.c_str(), ep.port,
            ep.user.c_str(), ep.password.c_str(),
            ep.database.c_str(), *ctx);
        int s = co_await count_select_one(db, 50);
        co_return s;
    }());

    auto t1 = clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    ASSERT_EQ(total, 50)
        << "50 sequential SELECT 1 should sum to 50";
    std::cerr << "[probe] 50 SELECT 1: " << ms << " ms\n";
}

// 2) 50 sequential parameterized lookups against a populated table.
TEST_F(MySQLAsyncProbe, FiftySequentialParamSelect)
{
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();

    int total = run_task(*ctx, [&]() -> orm::Task<int> {
        auto db = co_await orm::AsyncMySQLDB::connect(
            ep.host.c_str(), ep.port,
            ep.user.c_str(), ep.password.c_str(),
            ep.database.c_str(), *ctx);
        int s = co_await select_by_id_param(db, 50);
        co_return s;
    }());

    auto t1 = clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    ASSERT_EQ(total, 50 * 51 / 2)
        << "sum of id 1..50 should be 1275";
    std::cerr << "[probe] 50 param lookups: " << ms << " ms\n";
}

// 3) Large result set — exercises mysql_store_result_start/_cont with the
//    full network-fetch path.
TEST_F(MySQLAsyncProbe, LargeResultSetOverThousandRows)
{
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();

    int rows = run_task(*ctx, [&]() -> orm::Task<int> {
        auto db = co_await orm::AsyncMySQLDB::connect(
            ep.host.c_str(), ep.port,
            ep.user.c_str(), ep.password.c_str(),
            ep.database.c_str(), *ctx);
        int n = co_await large_result_set(db);
        co_return n;
    }());

    auto t1 = clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    ASSERT_EQ(rows, 1500)
        << "probe_users has 1500 rows after schema setup";
    std::cerr << "[probe] large result (1500 rows): " << ms << " ms\n";
}

// 4) Concurrent workload — 10 async connections each running 10 queries,
//    all sharing one IoContext. Each connection lives in its own coroutine.
//    This catches scheduler bugs that only show up when multiple
//    _start/_cont state machines are alive on the same reactor.
TEST_F(MySQLAsyncProbe, ConcurrentTenConnectionsTenQueries)
{
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();

    constexpr int kConns = 10;
    constexpr int kPerConn = 10;

    auto totals = run_task(*ctx, [&]() -> orm::Task<std::vector<int>> {
        std::vector<orm::Task<int>> tasks;
        tasks.reserve(kConns);
        for (int i = 0; i < kConns; ++i)
            tasks.emplace_back(per_connection_workload(*ctx, ep, kPerConn));

        std::vector<int> out;
        out.reserve(kConns);
        for (auto& t : tasks)
            out.push_back(co_await std::move(t));
        co_return out;
    }());

    auto t1 = clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    ASSERT_EQ(totals.size(), static_cast<size_t>(kConns));
    for (int i = 0; i < kConns; ++i)
        ASSERT_EQ(totals[i], kPerConn)
            << "coroutine " << i << " did not complete all " << kPerConn << " SELECT 1s";

    std::cerr << "[probe] 10 conns x 10 queries: " << ms << " ms\n";
}
