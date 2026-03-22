```toml
[phase]
plan = "implement-code-orm-connectors"
number = 8
total = 12
type = "implement"
title = "Thread Safety — connection_pool + thread_local_db + db::transaction() RAII + tests"
depends_on = [2, 3, 4, 5, 6, 7]
input_files = [
  "architecture/features/FEATURE-thread-safety.md",
  "lib/include/ORM/connector/capabilities.hpp",
  "lib/include/ORM/connector/db.hpp",
  "lib/include/ORM/db/connectors/ThreadSafety/thread_safety.hpp",
  "lib/include/ORM/db/connectors/MockDB/mock_db.hpp",
]
output_files = [
  "lib/include/ORM/db/connectors/ThreadSafety/thread_safety.hpp",
  "lib/include/ORM/db/connectors/MockDB/mock_db.hpp",
  "tests/unit/test_thread_safety.cpp",
]
outputs = ["out/phase-08-thread-safety-summary.md"]
inputs = ["out/phase-01-scaffolding-summary.md"]
```

## Preamble

This is a self-contained phase file. All rules, constraints, and kit content
are included below. Project files listed in the Task section must be read
at runtime. Follow the instructions exactly, run any EXECUTE commands as
written, and report results against the acceptance criteria at the end.

## What

Implement the ORM thread safety infrastructure in `lib/include/ORM/db/connectors/ThreadSafety/thread_safety.hpp`:
- `orm::connection_pool<DB, N>` — template holding N independent connections with RAII `connection_guard<DB>` and blocking acquire.
- `orm::thread_local_db<DB>` — thread-per-request pattern with zero synchronisation.
- `db::transaction()` RAII guard with BEGIN/COMMIT/ROLLBACK semantics.
- `supports_concurrent_execute` capability tag enforcement via `static_assert` at pool instantiation.

Also add `using supports_concurrent_execute = void;` to `connector_trait<MockDB>` so tests compile.
Write unit tests in `tests/unit/test_thread_safety.cpp`.

## Prior Context

- Phases 2–7 implemented 6 connectors. This phase adds cross-cutting infrastructure.
- `db<DB>` class in `lib/include/ORM/connector/db.hpp` needs a `transaction()` method added.
- `connection_pool<DB, N>` must `static_assert` at instantiation that `connector_trait<DB>` declares `supports_concurrent_execute`.
- Connection state machine: `Idle` → `InUse` → `Closed` (transitions via pool acquire/release/destructor).
- Pool uses `std::mutex` + `std::condition_variable`; blocking acquire when all N connections in use.
- `thread_local_db<DB>` uses `thread_local` storage; zero synchronisation overhead.
- `db::transaction()` guard: BEGIN issued on construction; COMMIT when `commit()` called; ROLLBACK on guard destruction if not committed. `static_assert` when `connector_trait<DB>` lacks `supports_transactions`.
- Traceability: DOCS-ONLY.

## User Decisions

### Already Decided (pre-resolved during planning)
- **Traceability mode**: DOCS-ONLY
- **Mock for pool tests**: use `MockDB` with `supports_concurrent_execute` added; mock connection holds a counter for execute calls
- **Thread test strategy**: use two `std::thread`s on a pool of size 1; second thread must block until first releases guard

## Rules

### Structural
- MUST: `connection_pool<DB, N>` MUST `static_assert` at instantiation that `connector_trait<DB>` declares `supports_concurrent_execute`; message MUST contain "supports_concurrent_execute"
- MUST: `connection_guard<DB>` MUST be RAII — destructor transitions connection back to `Idle` and calls `notify_one` on the condition variable
- MUST: `db::transaction()` MUST produce `static_assert` compile error when `connector_trait<DB>` does not declare `supports_transactions`; message MUST contain "supports_transactions"
- MUST: `db::transaction()` RAII guard MUST issue ROLLBACK on destruction when `commit()` not called
- MUST: `thread_local_db<DB>` MUST create exactly one connection per thread with no mutex contention
- MUST: Connection state machine MUST implement: Idle → InUse (on acquire), InUse → Idle (on guard release), InUse → Closed (on fatal error), Idle → Closed (on pool destructor)
- MUST: Code implements all DoD items from FEATURE-thread-safety.md §5
- MUST NOT: leave TODO/TBD/FIXME

### Engineering
- MUST: TDD — tests for blocking acquire, transaction rollback, static_assert for missing tag
- MUST: SOLID / Single Responsibility — pool, guard, transaction guard are separate types
- MUST: SOLID / Dependency Inversion — pool templated on DB; no hard-coded connector types
- MUST NOT: block the executing test thread indefinitely — pool N=1 tests must use a timeout or short-lived guard
- MUST NOT: use raw new/delete

### Quality
- MUST: Tests cover: (a) pool N=1 blocks second acquire until first guard released; (b) static_assert for missing `supports_concurrent_execute`; (c) transaction ROLLBACK on guard destruction; (d) COMMIT issued when `commit()` called; (e) `thread_local_db` independent per thread

## Input

### DoD items from FEATURE-thread-safety.md §5:

**DoD: connection_pool<DB, N> Template** — holds N connections; `acquire()` returns RAII guard; blocks when all in use.
Implements: `cpt-orm-flow-thread-safety-pool-execute`, `cpt-orm-algo-thread-safety-pool-acquire`

**DoD: Thread-Local Connection Pattern** — `thread_local_db<DB>` helper; one connection per thread; zero synchronisation.
Implements: `cpt-orm-feature-thread-safety`

**DoD: db::transaction() RAII Guard** — BEGIN on construction; COMMIT when `commit()` called; ROLLBACK on destruction if uncommitted. `static_assert` when `supports_transactions` absent.
Implements: `cpt-orm-flow-thread-safety-transaction`

**DoD: supports_concurrent_execute Capability Tag** — `static_assert` at `connection_pool<DB,N>` instantiation when tag absent; message contains "supports_concurrent_execute".
Implements: `cpt-orm-algo-thread-safety-capability-check`

### TDD acceptance criteria from FEATURE §6:
- `connection_pool<DB,N>` compiles when `connector_trait<DB>` declares `supports_concurrent_execute`.
- Two threads on pool size 1: second blocks until first releases guard; no data races (ThreadSanitizer).
- `db::transaction()` guard issues ROLLBACK when destroyed without `commit()`.
- `db::transaction()` guard issues COMMIT when `commit()` called.
- Missing `supports_concurrent_execute` → `static_assert` compile error containing "supports_concurrent_execute".
- `thread_local_db<DB>` provides independent connection per thread with no mutex contention.

## Task

1. **Read input files** — Read `architecture/features/FEATURE-thread-safety.md` (full), `lib/include/ORM/connector/capabilities.hpp`, `lib/include/ORM/connector/db.hpp`, existing stub `lib/include/ORM/db/connectors/ThreadSafety/thread_safety.hpp`, `lib/include/ORM/db/connectors/MockDB/mock_db.hpp` (lines 244–260 for capability aliases).

2. **Update `mock_db.hpp`** — Add `using supports_concurrent_execute = void;` to `connector_trait<MockDB>` (after existing capability aliases).

3. **Implement `thread_safety.hpp`**:
   - `enum class connection_state { Idle, InUse, Closed };`
   - `template<typename DB, std::size_t N> class connection_pool`:
     - Private: array of `DB` connections, array of `connection_state`, `std::mutex mutex_`, `std::condition_variable cv_`
     - `static_assert(requires { typename connector_trait<DB>::supports_concurrent_execute; }, "connection_pool requires connector_trait<DB>::supports_concurrent_execute")`
     - `acquire()` → `connection_guard<DB>` (blocking; locks mutex, scans for Idle, waits on cv if none)
   - `template<typename DB> class connection_guard`:
     - RAII: holds pool reference + connection index; destructor transitions back to Idle, `cv_.notify_one()`
     - `db<DB>& get()` accessor
   - `template<typename DB> class thread_local_db`:
     - `static DB& get()` returning `thread_local DB instance_; return instance_;`
   - Add `transaction()` method to `db<DB>` in `lib/include/ORM/connector/db.hpp`:
     - Returns `transaction_guard<DB>` with BEGIN/COMMIT/ROLLBACK semantics
     - `static_assert(requires { typename connector_trait<DB>::supports_transactions; }, "db::transaction() requires connector_trait<DB>::supports_transactions")`
   - `template<typename DB> class transaction_guard` (in thread_safety.hpp or db.hpp):
     - Constructor: calls `connector_trait<DB>::begin(conn_)`
     - `commit()`: calls `connector_trait<DB>::commit(conn_)`, sets `committed_ = true`
     - Destructor: if `!committed_`, calls `connector_trait<DB>::rollback(conn_)`
   - Add `begin/commit/rollback` stubs to `connector_trait<MockDB>` (store last command in `last_sql`)

4. **Write unit tests** — Create `tests/unit/test_thread_safety.cpp`:
   - `TEST(ThreadSafety, PoolAcquireBlocking)` — pool N=1; acquire guard in thread A; thread B tries acquire; B unblocks only after A releases
   - `TEST(ThreadSafety, TransactionAutoRollback)` — `{ auto txn = db.transaction(); }` → `last_sql` contains "ROLLBACK"
   - `TEST(ThreadSafety, TransactionCommit)` — `auto txn = db.transaction(); txn.commit();` → `last_sql` contains "COMMIT"
   - `TEST(ThreadSafety, ThreadLocalIndependence)` — two threads each call `thread_local_db<MockDB>::get()`; verify distinct addresses

5. **Update `tests/unit/CMakeLists.txt`** — Add `test_thread_safety.cpp`; add `<thread>` linking if needed (standard library).

6. **Write `out/phase-08-thread-safety-summary.md`**.

7. **Self-verify**.

## Acceptance Criteria

- [ ] `thread_safety.hpp` contains `connection_pool<DB,N>`, `connection_guard<DB>`, `thread_local_db<DB>`, `transaction_guard<DB>`
- [ ] `connection_pool` `static_assert` message contains "supports_concurrent_execute"
- [ ] `db::transaction()` added to `db.hpp`; `static_assert` message contains "supports_transactions"
- [ ] `connector_trait<MockDB>` now declares `using supports_concurrent_execute = void;`
- [ ] Blocking acquire test present (two-thread)
- [ ] Transaction auto-rollback test present
- [ ] Transaction commit test present
- [ ] `test_thread_safety.cpp` with ≥ 3 TEST() cases; CMakeLists updated
- [ ] No TODO/TBD/FIXME; no unresolved `{variable}` outside code fences
- [ ] `out/phase-08-thread-safety-summary.md` exists

## Output Format

```text
PHASE 8/12 COMPLETE
Status: PASS | FAIL
Files created/modified: {list}
Acceptance criteria: {checklist}
Notes: {any issues}
```

```text
I have a Cypilot execution plan at:
  .cypilot/.plans/implement-code-orm-connectors/plan.toml

Phase 8 is complete (PASS).
Please read the plan manifest, then execute Phase 9: "Wire Protocol — async awaitable + batch_insert + zero-copy + constexpr SQL + tests".
The phase file is: .cypilot/.plans/implement-code-orm-connectors/phase-09-wire-protocol.md
It is self-contained — follow its instructions exactly.
After completion, report results and generate the prompt for the next phase.
```
