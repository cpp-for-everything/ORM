// bench_async.cpp — Sync vs Async throughput benchmarks
//
// Measures the real-world advantage of async coroutine scheduling over sync
// blocking under concurrent load with simulated I/O latency.
//
// Structure:
//   1. Infrastructure: cpu_work, CompletionLatch, TimerQueue, awaitables
//   2. Throughput:      BM_SyncThroughput, BM_AsyncThroughput,
//                       BM_AsyncYieldThroughput
//   3. Micro:           BM_TaskCreateAndWait, BM_ThreadPoolPost,
//                       BM_NestedCoAwait
//
// NOTE: Google Benchmark calibrates iterations via CPU time.  sync_wait() and
// cv.wait() consume near-zero CPU time, so calibration never terminates on
// macOS.  All async benchmarks use ->Iterations(N)->UseRealTime().

#include <benchmark/benchmark.h>
#include "ORM/async/task.hpp"
#include "ORM/async/thread_pool.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// ═══════════════════════════════════════════════════════════════════════════
// 1. Infrastructure
// ═══════════════════════════════════════════════════════════════════════════

// Deterministic CPU-bound work (LCG).  Returns a hash-like value so the
// compiler cannot dead-code-eliminate it.  ~1 µs per ~200 iterations at -O2.
static int cpu_work(int seed, int iterations) noexcept
{
    int val = seed;
    for (int i = 0; i < iterations; ++i)
    {
        val = (val * 1103515245 + 12345) & 0x7fffffff;
    }
    return val;
}

// Lightweight latch: M tasks call count_down(); main thread blocks in wait().
class CompletionLatch
{
    std::atomic<int> remaining_;
    std::mutex mtx_;
    std::condition_variable cv_;

public:
    explicit CompletionLatch(int count) noexcept : remaining_(count) {}

    void count_down() noexcept
    {
        if (remaining_.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            std::lock_guard<std::mutex> lk(mtx_);
            cv_.notify_one();
        }
    }

    void wait()
    {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait(lk, [this] { return remaining_.load(std::memory_order_acquire) <= 0; });
    }

    void reset(int count) noexcept
    {
        remaining_.store(count, std::memory_order_release);
    }
};

// Single-thread timer queue for simulated async I/O.  schedule() enqueues a
// (deadline, callback) pair; the timer thread fires callbacks at their
// deadlines.  Used by SimulatedAsyncIO to resume coroutines after a delay.
class TimerQueue
{
    struct Entry
    {
        std::chrono::steady_clock::time_point deadline;
        std::function<void()> callback;

        bool operator>(const Entry& other) const noexcept
        {
            return deadline > other.deadline;
        }
    };

    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> stopped_{false};
    std::thread thread_;

    void run()
    {
        std::unique_lock<std::mutex> lk(mtx_);
        while (!stopped_.load(std::memory_order_relaxed))
        {
            if (pq_.empty())
            {
                cv_.wait(lk, [this] {
                    return !pq_.empty() || stopped_.load(std::memory_order_relaxed);
                });
                continue;
            }

            auto deadline = pq_.top().deadline;
            if (cv_.wait_until(lk, deadline) == std::cv_status::timeout)
            {
                auto now = std::chrono::steady_clock::now();
                while (!pq_.empty() && pq_.top().deadline <= now)
                {
                    auto cb = std::move(const_cast<Entry&>(pq_.top()).callback);
                    pq_.pop();
                    lk.unlock();
                    cb();
                    lk.lock();
                }
            }
        }
    }

public:
    TimerQueue() : thread_([this] { run(); }) {}

    ~TimerQueue()
    {
        stopped_.store(true, std::memory_order_release);
        cv_.notify_one();
        if (thread_.joinable()) thread_.join();
    }

    TimerQueue(const TimerQueue&) = delete;
    TimerQueue& operator=(const TimerQueue&) = delete;

    void schedule(std::chrono::steady_clock::time_point deadline,
                  std::function<void()> callback)
    {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            pq_.push({deadline, std::move(callback)});
        }
        cv_.notify_one();
    }
};

// Awaitable: suspends the coroutine, schedules a delayed resumption via the
// TimerQueue, then the timer posts h.resume() back to the thread pool.
// The pool thread is freed immediately — other coroutines can run.
struct SimulatedAsyncIO
{
    TimerQueue& timer;
    orm::ThreadPool& pool;
    std::chrono::microseconds delay;

    static bool await_ready() noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) noexcept
    {
        timer.schedule(
            std::chrono::steady_clock::now() + delay,
            [&p = pool, h]() { p.post([h]() { h.resume(); }); });
    }

    static void await_resume() noexcept {}
};

// Awaitable: suspends the coroutine and immediately re-posts h.resume() to
// the back of the pool queue.  No delay — pure scheduling interleave.
struct PoolYield
{
    orm::ThreadPool& pool;

    static bool await_ready() noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) noexcept
    {
        pool.post([h]() { h.resume(); });
    }

    static void await_resume() noexcept {}
};

// Helper: launch a Task<void> as detached on the pool, calling
// latch.count_down() when it completes.
static void launch_on_pool(orm::Task<void> task,
                           orm::ThreadPool& pool,
                           CompletionLatch& latch)
{
    auto h = task.release();
    h.promise().on_complete_ = [&latch]() { latch.count_down(); };
    h.promise().detach();
    pool.post([h]() { h.resume(); });
}

// ═══════════════════════════════════════════════════════════════════════════
// 2. Throughput benchmarks
// ═══════════════════════════════════════════════════════════════════════════

// Args: {threads, tasks, io_latency_µs, cpu_iterations}
// Each benchmark iteration = one batch of M tasks, measured wall-time.

// ── Sync: pool threads BLOCK during simulated I/O ───────────────────────

static void BM_SyncThroughput(benchmark::State& state)
{
    const auto num_threads = static_cast<size_t>(state.range(0));
    const auto num_tasks = static_cast<int>(state.range(1));
    const auto io_lat = std::chrono::microseconds(state.range(2));
    const auto cpu_iters = static_cast<int>(state.range(3));

    orm::ThreadPool pool(num_threads);
    CompletionLatch latch(0);

    for (auto _ : state)
    {
        latch.reset(num_tasks);
        for (int i = 0; i < num_tasks; ++i)
        {
            pool.post([i, io_lat, cpu_iters, &latch]() {
                int r = cpu_work(i, cpu_iters);
                std::this_thread::sleep_for(io_lat);
                r += cpu_work(r, cpu_iters);
                benchmark::DoNotOptimize(r);
                latch.count_down();
            });
        }
        latch.wait();
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) * num_tasks);
}

// ── Async: coroutines YIELD during simulated I/O (TimerQueue) ───────────

static void BM_AsyncThroughput(benchmark::State& state)
{
    const auto num_threads = static_cast<size_t>(state.range(0));
    const auto num_tasks = static_cast<int>(state.range(1));
    const auto io_lat = std::chrono::microseconds(state.range(2));
    const auto cpu_iters = static_cast<int>(state.range(3));

    orm::ThreadPool pool(num_threads);
    TimerQueue timer;
    CompletionLatch latch(0);

    for (auto _ : state)
    {
        latch.reset(num_tasks);
        for (int i = 0; i < num_tasks; ++i)
        {
            auto coro = [](int idx, int iters,
                           std::chrono::microseconds delay,
                           TimerQueue& tmr,
                           orm::ThreadPool& pl) -> orm::Task<void> {
                int r = cpu_work(idx, iters);
                co_await SimulatedAsyncIO{tmr, pl, delay};
                r += cpu_work(r, iters);
                benchmark::DoNotOptimize(r);
                co_return;
            }(i, cpu_iters, io_lat, timer, pool);
            launch_on_pool(std::move(coro), pool, latch);
        }
        latch.wait();
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) * num_tasks);
}

// ── Async yield-only: coroutines YIELD to pool (no I/O delay) ───────────

static void BM_AsyncYieldThroughput(benchmark::State& state)
{
    const auto num_threads = static_cast<size_t>(state.range(0));
    const auto num_tasks = static_cast<int>(state.range(1));
    const auto cpu_iters = static_cast<int>(state.range(2));

    orm::ThreadPool pool(num_threads);
    CompletionLatch latch(0);

    for (auto _ : state)
    {
        latch.reset(num_tasks);
        for (int i = 0; i < num_tasks; ++i)
        {
            auto coro = [](int idx, int iters,
                           orm::ThreadPool& pl) -> orm::Task<void> {
                int r = cpu_work(idx, iters);
                co_await PoolYield{pl};
                r += cpu_work(r, iters);
                benchmark::DoNotOptimize(r);
                co_return;
            }(i, cpu_iters, pool);
            launch_on_pool(std::move(coro), pool, latch);
        }
        latch.wait();
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) * num_tasks);
}

// Parameter grid:  {threads, tasks, io_latency_µs, cpu_iterations}
//
// We compare sync vs async across varying I/O latencies and task counts.
// The async advantage grows with I/O latency — blocked sync threads waste
// CPU time that async coroutines reclaim.

static constexpr int kCpuIters = 1000;

BENCHMARK(BM_SyncThroughput)
    ->Args({4, 100, 0, kCpuIters})
    ->Args({4, 100, 10, kCpuIters})
    ->Args({4, 100, 100, kCpuIters})
    ->Args({4, 100, 1000, kCpuIters})
    ->Args({4, 1000, 0, kCpuIters})
    ->Args({4, 1000, 100, kCpuIters})
    ->Args({8, 100, 0, kCpuIters})
    ->Args({8, 100, 100, kCpuIters})
    ->Iterations(10)->UseRealTime();

BENCHMARK(BM_AsyncThroughput)
    ->Args({4, 100, 10, kCpuIters})
    ->Args({4, 100, 100, kCpuIters})
    ->Args({4, 100, 1000, kCpuIters})
    ->Args({4, 1000, 100, kCpuIters})
    ->Args({8, 100, 100, kCpuIters})
    ->Iterations(10)->UseRealTime();

// Yield-only variant: {threads, tasks, cpu_iterations}
BENCHMARK(BM_AsyncYieldThroughput)
    ->Args({4, 100, kCpuIters})
    ->Args({4, 1000, kCpuIters})
    ->Args({8, 100, kCpuIters})
    ->Iterations(10)->UseRealTime();

// ═══════════════════════════════════════════════════════════════════════════
// 3. Micro-benchmarks — isolated overhead measurements
// ═══════════════════════════════════════════════════════════════════════════

// Task<int> creation + sync_wait overhead (no pool, no I/O)
static void BM_TaskCreateAndWait(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto task = []() -> orm::Task<int> {
            co_return 42;
        }();
        auto val = task.sync_wait();
        benchmark::DoNotOptimize(val);
    }
}
BENCHMARK(BM_TaskCreateAndWait);

// Raw ThreadPool::post() + promise/future round-trip
static void BM_ThreadPoolPost(benchmark::State& state)
{
    orm::ThreadPool pool(static_cast<size_t>(state.range(0)));
    std::atomic<int> counter{0};

    for (auto _ : state)
    {
        auto done = std::make_shared<std::promise<void>>();
        auto fut = done->get_future();
        pool.post([&counter, done]() {
            counter.fetch_add(1, std::memory_order_relaxed);
            done->set_value();
        });
        fut.wait();
    }
    benchmark::DoNotOptimize(counter.load());
}
BENCHMARK(BM_ThreadPoolPost)->Arg(1)->Arg(2)->Arg(4)->Arg(8)
    ->Iterations(100000)->UseRealTime();

// Nested co_await chain: measures symmetric transfer depth overhead
static void BM_NestedCoAwait(benchmark::State& state)
{
    const auto depth = state.range(0);

    for (auto _ : state)
    {
        std::function<orm::Task<int>(int64_t)> make_chain;
        make_chain = [&](int64_t d) -> orm::Task<int> {
            if (d <= 0) co_return 1;
            co_return co_await make_chain(d - 1);
        };

        auto task = make_chain(depth);
        auto val = task.sync_wait();
        benchmark::DoNotOptimize(val);
    }
}
BENCHMARK(BM_NestedCoAwait)->Arg(1)->Arg(5)->Arg(10)->Arg(50)->Arg(100);
