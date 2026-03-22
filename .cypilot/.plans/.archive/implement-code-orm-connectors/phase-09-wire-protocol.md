```toml
[phase]
plan = "implement-code-orm-connectors"
number = 9
total = 12
type = "implement"
title = "Wire Protocol — async awaitable + batch_insert + zero-copy + constexpr SQL + tests"
depends_on = [2, 3]
input_files = [
  "architecture/features/FEATURE-wire-protocol.md",
  "lib/include/ORM/connector/capabilities.hpp",
  "lib/include/ORM/connector/db.hpp",
  "lib/include/ORM/db/connectors/WireProtocol/wire_protocol.hpp",
  "lib/include/ORM/db/connectors/MockDB/mock_db.hpp",
]
output_files = [
  "lib/include/ORM/db/connectors/WireProtocol/wire_protocol.hpp",
  "tests/unit/test_wire_protocol.cpp",
]
outputs = ["out/phase-09-wire-protocol-summary.md"]
inputs = ["out/phase-01-scaffolding-summary.md"]
```

## Preamble

This is a self-contained phase file. All rules, constraints, and kit content
are included below. Project files listed in the Task section must be read
at runtime. Follow the instructions exactly, run any EXECUTE commands as
written, and report results against the acceptance criteria at the end.

## What

Implement four wire-protocol optimisations as header-only C++23 templates in
`lib/include/ORM/db/connectors/WireProtocol/wire_protocol.hpp`:
1. `orm::io_uring_awaitable<DB>` (Linux) / `orm::iocp_awaitable<DB>` (Windows) — C++20 coroutine awaitables for async `co_await db.execute()`.
2. `orm::zero_copy_result<DB>` — per-column `std::span<const std::byte>` views into driver buffer without copy.
3. `orm::batch_insert<DB, Entity>` — accumulates N rows, emits single multi-row INSERT.
4. Compile-time SQL generation — `constexpr char[]` SQL string for fully-static query IRs on connectors declaring `supports_constexpr_sql`.

All four are platform-guarded (`#ifdef __linux__` / `#ifdef _WIN32`) where applicable. Unit tests use MockDB as the connector and a mock io_uring/IOCP surface.

## Prior Context

- Phases 2–3 implemented MySQL/PostgreSQL connectors. This phase adds performance infrastructure that wraps them.
- C++20 coroutines: `co_await`, `await_ready()`, `await_suspend()`, `await_resume()`.
- `io_uring` on Linux: `io_uring_prep_send` / `io_uring_submit` / CQE completion.
- IOCP on Windows: overlapped I/O, `PostQueuedCompletionStatus`.
- Zero-copy: `std::span<const std::byte>` pointing into driver buffer; no copy until `.value<T>()` called.
- `batch_insert`: single `INSERT INTO table (cols) VALUES (row1), (row2), ...`; empty batch → no-op.
- `constexpr` SQL: `if constexpr` path for IRs with no runtime-variant nodes, when connector declares `using supports_constexpr_sql = void`.
- `orm::result<>` must carry the awaitable resolution — `await_resume()` returns `orm::result<...>`.
- Traceability: DOCS-ONLY.

## User Decisions

### Already Decided (pre-resolved during planning)
- **Traceability mode**: DOCS-ONLY
- **Mock async strategy**: `MockAsyncHandle` that immediately completes (no actual I/O); `await_ready()` returns false; `await_suspend` stores handle; `await_resume` returns mock result
- **Platform guards**: `#ifdef __linux__` for io_uring path; `#ifdef _WIN32` for IOCP path; both paths present in the header; only one compiles per platform
- **Zero-copy mock**: `MockDriverBuffer` holding `std::vector<std::byte>` with span accessor

## Rules

### Structural
- MUST: `io_uring_awaitable<DB>::await_ready()` MUST return `false` (always suspend — I/O is always async)
- MUST: `io_uring_awaitable<DB>::await_suspend(handle)` MUST store the coroutine handle in SQE user data / OVERLAPPED structure
- MUST: `io_uring_awaitable<DB>::await_resume()` MUST return `orm::result<Row...>`
- MUST: `zero_copy_result::span<col_idx>()` MUST return `std::span<const std::byte>` pointing directly into driver buffer; no copy performed at the call site
- MUST: `batch_insert<DB, Entity>::execute()` MUST emit exactly one SQL statement regardless of row count; MUST return zero-rows-affected for empty batch without sending any statement
- MUST: Compile-time SQL generation MUST only activate for connectors declaring `using supports_constexpr_sql = void`; MUST fall back to runtime rendering when runtime-variant nodes are present
- MUST: Code implements all DoD items from FEATURE-wire-protocol.md §5
- MUST NOT: leave TODO/TBD/FIXME

### Engineering
- MUST: TDD — tests for `await_ready()`, batch count, zero-copy pointer equality, constexpr SQL `static_assert`
- MUST: SOLID / Single Responsibility — each optimisation is a separate class/function
- MUST: Platform guards (#ifdef __linux__ / #ifdef _WIN32) around platform-specific code; neutral fallback for other platforms
- MUST NOT: block any thread in await_suspend — store handle and return; resumption triggered by completion event
- MUST NOT: use raw new/delete

### Quality
- MUST: Tests cover: (a) `await_ready()` returns false; (b) batch_insert N rows → 1 execute call; (c) empty batch → 0 execute calls; (d) zero-copy span data() equals buffer base + offset; (e) constexpr SQL static_assert for fully-static IR

## Input

### DoD items from FEATURE-wire-protocol.md §5:

**DoD: Async I/O Integration** — `co_await db.execute()` overload; suspends coroutine without blocking OS thread; resumes on I/O completion.
Implements: `cpt-orm-flow-wire-protocol-async-execute`, `cpt-orm-algo-wire-protocol-io-uring-dispatch`

**DoD: Zero-Copy Result Parsing** — per-column `std::span<const std::byte>` views into driver buffer; no ORM copy during iteration.
Implements: `cpt-orm-algo-wire-protocol-zero-copy-result`

**DoD: Batch INSERT Query Type** — `batch_insert(rows...)` accumulates N rows; single multi-row INSERT; reduces N round-trips to 1.
Implements: `cpt-orm-flow-wire-protocol-batch-insert`, `cpt-orm-algo-wire-protocol-batch-insert-render`

**DoD: Compile-Time SQL Generation** — `constexpr char[]` SQL for static IRs; embedded as string literal; eliminates `std::format` on hot path.
Implements: `cpt-orm-algo-wire-protocol-constexpr-sql`

### TDD acceptance criteria from FEATURE §6:
- `co_await db.execute()` compiles and correctly suspends/resumes in C++20 coroutine test on Linux (io_uring) and Windows (IOCP).
- `result.span<0>()` returns valid `std::span<const std::byte>` pointing into driver buffer.
- `batch_insert` with 1000 rows emits exactly one SQL statement.
- Fully-static query IR targeting `supports_constexpr_sql` connector produces `constexpr char[]` verified by `static_assert(sql[0] != '\0')`.
- `co_await db.execute()` under ThreadSanitizer shows no data races.

## Task

1. **Read input files** — Read `architecture/features/FEATURE-wire-protocol.md` (full), `lib/include/ORM/connector/capabilities.hpp`, `lib/include/ORM/connector/db.hpp`, existing stub `lib/include/ORM/db/connectors/WireProtocol/wire_protocol.hpp`, `lib/include/ORM/db/connectors/MockDB/mock_db.hpp` (lines 1–30 for pattern).

2. **Implement `wire_protocol.hpp`**:
   - **Async awaitable** (platform-guarded):
     ```cpp
     #ifdef __linux__
     template<typename DB, typename Query>
     struct io_uring_awaitable {
         bool await_ready() const noexcept { return false; }
         void await_suspend(std::coroutine_handle<> h) { handle_ = h; /* store for resume */ }
         orm::result<> await_resume() { return result_; }
         std::coroutine_handle<> handle_;
         orm::result<> result_;
     };
     #endif
     #ifdef _WIN32
     template<typename DB, typename Query>
     struct iocp_awaitable { /* same interface */ };
     #endif
     ```
   - Add `co_await_execute()` free function / `db<DB>` extension (or a non-member overload) returning the platform awaitable.
   - **Zero-copy result**:
     ```cpp
     template<typename DB>
     struct zero_copy_result {
         template<std::size_t ColIdx>
         std::span<const std::byte> span() const noexcept {
             return std::span<const std::byte>(buffer_ + offsets_[ColIdx], lengths_[ColIdx]);
         }
         const std::byte* buffer_;
         std::size_t offsets_[16];
         std::size_t lengths_[16];
     };
     ```
   - **Batch INSERT**:
     ```cpp
     template<typename DB, typename Entity>
     struct batch_insert {
         void add(Entity e) { rows_.push_back(std::move(e)); }
         orm::result<std::tuple<>> execute(DB& conn);
         std::vector<Entity> rows_;
     };
     ```
     `execute()` — if empty, return zero-rows result; else render single multi-row INSERT SQL and call `connector_trait<DB>::execute(conn, ...)`.
   - **Constexpr SQL**:
     ```cpp
     template<typename DB, typename Query>
     constexpr auto make_constexpr_sql() {
         static_assert(requires { typename connector_trait<DB>::supports_constexpr_sql; },
             "make_constexpr_sql requires connector_trait<DB>::supports_constexpr_sql");
         // Compile-time SQL rendering
         return connector_trait<DB>::render_constexpr(Query{});
     }
     ```
     Add `using supports_constexpr_sql = void;` and `static constexpr auto render_constexpr(auto q)` to `connector_trait<MockDB>` that returns a small `constexpr char[]`.

3. **Write unit tests** — Create `tests/unit/test_wire_protocol.cpp`:
   - `TEST(WireProtocol, AwaitReadyFalse)` — `io_uring_awaitable<MockDB, SelectQuery> aw; EXPECT_FALSE(aw.await_ready());` (Linux only, guarded by `#ifdef __linux__`)
   - `TEST(WireProtocol, BatchInsertEmptyNoOp)` — `batch_insert<MockDB, User> bi; bi.execute(mock_conn); EXPECT_EQ(mock_conn.last_sql, "");`
   - `TEST(WireProtocol, BatchInsertNRows)` — add 3 rows, execute; verify `last_sql` contains "VALUES" exactly once
   - `TEST(WireProtocol, ZeroCopySpanPointer)` — create `zero_copy_result` with known buffer; `span<0>().data() == buffer + offsets[0]`
   - `TEST(WireProtocol, ConstexprSqlNotEmpty)` — `static_assert(make_constexpr_sql<MockDB, SelectQuery>()[0] != '\0')`

4. **Update `tests/unit/CMakeLists.txt`** — Add `test_wire_protocol.cpp`.

5. **Write `out/phase-09-wire-protocol-summary.md`**.

6. **Self-verify**.

## Acceptance Criteria

- [ ] `wire_protocol.hpp` contains `io_uring_awaitable` (Linux) and `iocp_awaitable` (Windows) with correct awaitable interface
- [ ] `await_ready()` returns false (test present)
- [ ] `zero_copy_result::span<N>()` returns `std::span<const std::byte>` with correct pointer (test present)
- [ ] `batch_insert` empty batch → no SQL emitted (test present)
- [ ] `batch_insert` N rows → single VALUES clause (test present)
- [ ] Constexpr SQL `static_assert` test present
- [ ] `connector_trait<MockDB>` updated with `supports_constexpr_sql` and `render_constexpr`
- [ ] `test_wire_protocol.cpp` with ≥ 4 TEST() cases; CMakeLists updated
- [ ] No TODO/TBD/FIXME; no unresolved `{variable}` outside code fences
- [ ] `out/phase-09-wire-protocol-summary.md` exists

## Output Format

```text
PHASE 9/12 COMPLETE
Status: PASS | FAIL
Files created/modified: {list}
Acceptance criteria: {checklist}
Notes: {any issues}
```

```text
I have a Cypilot execution plan at:
  .cypilot/.plans/implement-code-orm-connectors/plan.toml

Phase 9 is complete (PASS).
Please read the plan manifest, then execute Phase 10: "C++26 Reflection — property<T> inference + PFR fallback + tests".
The phase file is: .cypilot/.plans/implement-code-orm-connectors/phase-10-cpp26-reflection.md
It is self-contained — follow its instructions exactly.
After completion, report results and generate the prompt for the next phase.
```
