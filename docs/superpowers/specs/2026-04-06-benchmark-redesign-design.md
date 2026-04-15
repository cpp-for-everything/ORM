# Benchmark Redesign: Sync vs Async Throughput

**Date:** 2026-04-06
**Status:** Approved (verbal)

## Problem

The existing benchmarks call `sync_wait()` in a tight loop per Google Benchmark
iteration.  This measures single-coroutine round-trip latency, not real-world
throughput.  Worse, `sync_wait()` sleeps the main thread in `cv.wait()`, which
consumes near-zero CPU time — Google Benchmark's calibration loop never
terminates.

Real users never call `sync_wait()`.  They launch many coroutines that
interleave on a thread pool: when one coroutine awaits I/O, the pool thread
picks up another.

## Design

### Infrastructure

| Component | Purpose |
|---|---|
| `cpu_work(seed, iters) → int` | Deterministic CPU-bound work (LCG). ~1 µs / 200 iters at -O2. |
| `CompletionLatch(n)` | Atomic counter + CV.  `count_down()` per task; `wait()` on main thread. |
| `TimerQueue` | Single thread, priority queue of `(deadline, callback)`.  For realistic I/O. |
| `SimulatedAsyncIO{timer, pool, delay}` | Awaitable: suspends coroutine, timer resumes it on pool after delay. |
| `PoolYield{pool}` | Awaitable: suspends coroutine, immediately re-posts `h.resume()` to pool. |

### Throughput Benchmarks

Each benchmark iteration = one batch of **M tasks**, measured wall-time.

#### `BM_SyncThroughput(threads, tasks, io_latency_us)`

1. `ThreadPool pool(threads)`
2. Post M lambdas: `cpu_work → sleep_for(io_latency) → cpu_work`
3. `sleep_for` **blocks** the pool thread.
4. `CompletionLatch::wait()` on main thread.

#### `BM_AsyncThroughput(threads, tasks, io_latency_us)`

1. `ThreadPool pool(threads)` + `TimerQueue timer`
2. Create M `Task<void>` coroutines: `cpu_work → co_await SimulatedAsyncIO → cpu_work`
3. Release handle, set `detached_=true`, `on_complete_=latch.count_down()`
4. Post `h.resume()` to pool.
5. `CompletionLatch::wait()` on main thread.
6. `co_await` **frees** the pool thread; timer resumes later.

#### `BM_AsyncYieldThroughput(threads, tasks)`

Same as `BM_AsyncThroughput` but uses `PoolYield` (no delay).  Sync
counterpart is `BM_SyncThroughput` with `io_latency=0`.

#### Parameter Grid

```
Threads  Tasks  IO_latency_µs
   4      100       10
   4      100      100
   4      100     1000
   4     1000      100
   8      100      100
```

### Micro-benchmarks (retained)

- `BM_TaskCreateAndWait` — `Task<int>` creation + `sync_wait()`.
- `BM_ThreadPoolPost` — `pool.post()` + `promise/future`.
- `BM_NestedCoAwait` — nested `co_await` chain depth 1–100.

All use `->Iterations(N)->UseRealTime()` to bypass calibration.

### Removed

`BM_RunOnPoolVoid/Int`, `BM_AsyncSelectRunOnPool`, `BM_AsyncSequentialQueries`,
`BM_AsyncPoolAcquireRelease`, `BM_AsyncTransactionCommit/Rollback` — all
measured single-coroutine `sync_wait()` latency.

## Files Changed

- `benchmarks/bench_async.cpp` — full rewrite
- `benchmarks/test_hang.cpp` — deleted (debug artifact)
- `benchmarks/CMakeLists.txt` — remove `test_hang` target
