---
phase: 4
title: "Thread Safety & Wire-Protocol Optimisations FEATURE specs"
slug: infrastructure
status: pending
kind: delivery
depends_on: [3]
output_files:
  - architecture/features/FEATURE-thread-safety.md
  - architecture/features/FEATURE-wire-protocol.md
outputs:
  - .cypilot/.plans/implement-orm-features/out/phase-04-ids.md
inputs:
  - .cypilot/.plans/implement-orm-features/out/phase-01-ids.md
  - .cypilot/.plans/implement-orm-features/out/phase-02-ids.md
  - .cypilot/.plans/implement-orm-features/out/phase-03-ids.md
---

--- CONTEXT BOUNDARY ---
Disregard all previous context. This phase file is self-contained.
Read ONLY the files listed in Prior Context. Follow these instructions exactly.
---

## What

Generate two FEATURE spec artifacts:
1. `architecture/features/FEATURE-thread-safety.md` — thread safety, connection pool, and transaction support
2. `architecture/features/FEATURE-wire-protocol.md` — wire-protocol-level optimisations (async I/O, zero-copy, batch insert, compile-time SQL)

## Prior Context

Read these files before proceeding:

1. `doc/v2/bg/chapters/05_future_work.tex` lines 35–59 — Thread Safety and Wire Protocol sections
2. `.cypilot/config/kits/sdlc/artifacts/FEATURE/template.md` — full template
3. `.cypilot/config/kits/sdlc/artifacts/FEATURE/rules.md` — sections: Structural, Versioning, Semantic, Scope, Checkbox Management, Deliberate Omissions
4. `.cypilot/.plans/implement-orm-features/out/phase-01-ids.md`
5. `.cypilot/.plans/implement-orm-features/out/phase-02-ids.md`
6. `.cypilot/.plans/implement-orm-features/out/phase-03-ids.md`

## User Decisions

- **System**: `orm`
- **ID prefix**: `cpt-orm-`
- **Feature slugs**: `thread-safety`, `wire-protocol`
- **No DECOMPOSITION / DESIGN** → "DESIGN pending" in frontmatter and References
- **Output destination**: file
- **Lifecycle**: archive (Phase 6)

## Rules (verbatim from rules.md — MUST enforce all)

### Structural MUST
- FEATURE follows `template.md` structure exactly
- All flows, algorithms, states, DoD items have unique IDs
- All IDs follow `cpt-{system}-{kind}-{slug}` pattern
- All IDs have priority markers (`p1`–`p9`)
- Include feature slug in `{slug}` portion of IDs
- CDSL instructions: `N. [ ] - \`pN\` - Description - \`inst-slug\``
- No placeholder content (TODO, TBD, FIXME)
- No duplicate IDs within document or across prior phases (1–3)

### Structural MUST NOT
- MUST NOT redefine system-level types — **ARCH-FDESIGN-NO-001**
- MUST NOT define new API endpoints — **ARCH-FDESIGN-NO-002**
- MUST NOT include architectural decisions — **ARCH-FDESIGN-NO-003**
- MUST NOT include product requirements — **BIZ-FDESIGN-NO-001**
- MUST NOT include sprint/task breakdowns — **BIZ-FDESIGN-NO-002**
- MUST NOT include code snippets — **MAINT-FDESIGN-NO-001**
- MUST NOT include test implementation — **TEST-FDESIGN-NO-001**
- MUST NOT include security secrets — **SEC-FDESIGN-NO-001**
- MUST NOT include infrastructure code — **OPS-FDESIGN-NO-001**

### Versioning MUST
- New artifact: version `1.0` in frontmatter; changelog with initial entry

### Semantic MUST
- Actor flows define complete user journeys
- Algorithms specify processing logic clearly
- State machines capture all valid transitions (use for connection pool lifecycle)
- DoD items are testable and traceable
- CDSL instructions describe "what" not "how"
- Control flow keywords: IF, RETURN, FROM/TO/WHEN, FOR EACH, TRY/CATCH

### Featstatus MUST
- `cpt-orm-featstatus-{feature-slug}` defined directly under H1, before `## Feature Context`
- Checkbox `[ ]` (unchecked)

### Checkbox Management
- All flow/algo/state/dod checkboxes `[ ]`

## Source Material

### Thread Safety (from future_work.tex lines 35–47)
- Current `db<DB>` handle holds mutable reference to connection object
- Concurrent `db::execute()` calls on same instance → race condition on underlying connection handle
- Planned measures:
  - **Connection pool**: `connection_pool<DB, N>` — N independent connection objects, provided via RAII guards; each `db<DB>` instance exclusively owns one connection during query
  - **Thread-local connections**: `thread_local` connection per DB type eliminates all synchronisation overhead (thread-per-request model)
  - **Atomic transaction**: `db::transaction()` RAII guard wraps sequence of `execute()` calls in single atomic unit; auto-rollback on destruction if `commit()` not called
  - **Compile-time thread-safety tagging**: `supports_concurrent_execute` capability tag in `connector_trait<DB>`; `connection_pool` requires this tag → `static_assert` if connector not declared thread-safe

### Wire Protocol Optimisations (from future_work.tex lines 49–59)
- Current SQLite connector calls standard `sqlite3_*` C API
- **io_uring / IOCP async I/O**: for client-server databases (PostgreSQL, MySQL, MongoDB), blocking network I/O; integrate `io_uring` (Linux) / IOCP (Windows) → `co_await db.execute(q, args...)` coroutine interface
- **Zero-copy result parsing**: rows currently materialised into `std::tuple` values via copy from driver buffer; zero-copy path exposes raw column memory via `std::span` views, defer copy until app takes ownership
- **Batch INSERT optimisation**: inserting many rows one-by-one causes per-row round-trip on network databases; `batch_insert` query type accumulates rows in local buffer → emits single multi-row `INSERT INTO ... VALUES (...), (...), ...` or uses native bulk-load API
- **Compile-time SQL string generation**: for connectors where SQL is fully determined by IR (no runtime variance), compute entire SQL as `constexpr char[]` and embed as string literal in binary → eliminates `std::format` call on hot path

### ORM Architecture context
- `connector_trait<DB>`: dumb tag type, trait holds all logic
- Capability tags as nested types: `supports_joins`, `supports_transactions`, `supports_aggregation`
- Missing capability + usage = `static_assert` compile error
- Query IR: fully type-level constexpr fluent builder

## Task

### Step 1 — Verify prior IDs loaded
Read `out/phase-01-ids.md`, `out/phase-02-ids.md`, `out/phase-03-ids.md`. Keep combined ID list in context.

### Step 2 — Generate FEATURE-thread-safety.md

**Section 1: Feature Context**
- Feature ID: `cpt-orm-feature-thread-safety`
- Overview: Thread safety infrastructure for the ORM — `connection_pool<DB,N>`, thread-local connections, `db::transaction()` RAII guard, and compile-time `supports_concurrent_execute` capability tagging
- Purpose: Eliminate race conditions in multi-threaded applications without imposing synchronisation overhead on single-threaded usage

**Section 2: Actor Flows (CDSL)**
- Flow 1: Developer acquires a connection from pool and executes a query
  - ID: `cpt-orm-flow-thread-safety-pool-execute`
  - Steps: request RAII guard from pool → pool assigns idle connection → execute query → guard destructor returns connection to pool
- Flow 2: Developer wraps queries in a transaction
  - ID: `cpt-orm-flow-thread-safety-transaction`
  - Steps: call `db::transaction()` → execute multiple statements → IF all succeed: call `commit()` → ELSE: destructor triggers auto-rollback

**Section 3: Processes / Business Logic (CDSL)**
- Algo 1: Connection pool acquire — ID: `cpt-orm-algo-thread-safety-pool-acquire`
  - Input: pool of N connections, thread request
  - Output: RAII guard holding exclusive connection lease
  - Steps: scan pool for idle connection → IF found: mark in-use, return guard → ELSE: block until available
- Algo 2: Compile-time capability check — ID: `cpt-orm-algo-thread-safety-capability-check`
  - Input: `connector_trait<DB>` at compile time
  - Output: compile success or `static_assert` failure
  - Steps: check for `supports_concurrent_execute` nested type → static_assert if absent when pool is instantiated

**Section 4: States — Connection State Machine**
- State machine ID: `cpt-orm-state-thread-safety-connection`
- States: `Idle`, `InUse`, `Closed`
- Initial state: `Idle`
- Transitions:
  - FROM `Idle` TO `InUse` WHEN pool guard acquired
  - FROM `InUse` TO `Idle` WHEN guard destroyed (connection returned)
  - FROM `InUse` TO `Closed` WHEN connection error detected
  - FROM `Idle` TO `Closed` WHEN pool destructor runs

**Section 5: Definitions of Done**
- DoD 1: `cpt-orm-dod-thread-safety-connection-pool` — `connection_pool<DB,N>` instantiates and provides connections via RAII guards
- DoD 2: `cpt-orm-dod-thread-safety-thread-local` — `thread_local` connection pattern compiles and isolates per-thread state
- DoD 3: `cpt-orm-dod-thread-safety-transaction-raii` — `db::transaction()` guard commits or rolls back correctly
- DoD 4: `cpt-orm-dod-thread-safety-capability-tag` — `supports_concurrent_execute` tag enforced via `static_assert` at pool instantiation

**Section 6: Acceptance Criteria** — one testable criterion per DoD

### Step 3 — Generate FEATURE-wire-protocol.md

**Section 1: Feature Context**
- Feature ID: `cpt-orm-feature-wire-protocol`
- Overview: Wire-protocol-level performance optimisations — async I/O via `io_uring`/IOCP, zero-copy result parsing, batch INSERT, and compile-time SQL string generation
- Purpose: Eliminate per-row round-trips, blocking I/O, and runtime string formatting on the hot path for network database backends

**Section 2: Actor Flows (CDSL)**
- Flow 1: Developer executes a query asynchronously
  - ID: `cpt-orm-flow-wire-protocol-async-execute`
  - Steps: build query → `co_await db.execute(q, args...)` → io_uring submits request → coroutine suspends → io_uring completion wakes coroutine → result returned
- Flow 2: Developer bulk-inserts a collection of rows
  - ID: `cpt-orm-flow-wire-protocol-batch-insert`
  - Steps: build `batch_insert` query → accumulate rows → call execute → connector emits single multi-row INSERT → return affected row count

**Section 3: Processes / Business Logic (CDSL)**
- Algo 1: io_uring async dispatch — ID: `cpt-orm-algo-wire-protocol-io-uring-dispatch`
  - Input: prepared query + params, io_uring ring
  - Output: coroutine awaitable resolving to result
  - Steps: submit SQE to ring → co_return awaitable → on CQE: hydrate result
- Algo 2: Zero-copy result exposure — ID: `cpt-orm-algo-wire-protocol-zero-copy-result`
  - Input: driver buffer pointer + column offsets
  - Output: `std::span<const std::byte>` views per column
  - Steps: map column memory regions → return spans without copying → copy occurs only when app calls `.value()` or `.to_vector()`
- Algo 3: Batch INSERT accumulation — ID: `cpt-orm-algo-wire-protocol-batch-insert-render`
  - Input: collection of entity values
  - Output: single multi-row INSERT SQL or native bulk-load call
  - Steps: accumulate rows in local buffer → render `VALUES (...), (...), ...` → execute as single statement
- Algo 4: Compile-time SQL generation — ID: `cpt-orm-algo-wire-protocol-constexpr-sql`
  - Input: query IR (fully constexpr, no runtime-variant parts)
  - Output: `constexpr char[]` SQL string embedded in binary
  - Constraint: applicable only when IR contains no runtime-variant nodes; connector capability tag `supports_constexpr_sql` required

**Section 4: States** — Not applicable (optimisations are stateless transforms; note explicitly)

**Section 5: Definitions of Done**
- DoD 1: `cpt-orm-dod-wire-protocol-async-io` — `co_await db.execute()` compiles and completes correctly with io_uring/IOCP backend
- DoD 2: `cpt-orm-dod-wire-protocol-zero-copy` — result columns accessible via `std::span` without copy
- DoD 3: `cpt-orm-dod-wire-protocol-batch-insert` — `batch_insert` query emits single multi-row INSERT
- DoD 4: `cpt-orm-dod-wire-protocol-constexpr-sql` — `constexpr char[]` SQL string computed at compile time for eligible IR

**Section 6: Acceptance Criteria** — one testable criterion per DoD

### Step 4 — Write intermediate ID registry
Write `.cypilot/.plans/implement-orm-features/out/phase-04-ids.md` listing all new IDs.

### Step 5 — Update artifacts.toml
Add both FEATURE artifact paths to `[[systems]]` entry for `orm`.

### Step 6 — Confirm before writing
Present summary and ask `yes/no/modify`.

## Acceptance Criteria

- [ ] Both FEATURE files written to `architecture/features/`
- [ ] Template structure followed (sections 1–6)
- [ ] `cpt-orm-featstatus-{slug}` defined under H1 in each file
- [ ] No IDs duplicated from Phases 1–3
- [ ] No duplicate IDs within this phase
- [ ] All IDs `cpt-orm-{kind}-{feature-slug}-{slug}` with priority markers
- [ ] No placeholder content; no MUST NOT violations
- [ ] Connection state machine present in thread-safety FEATURE (Section 4)
- [ ] All checkboxes `[ ]`
- [ ] `out/phase-04-ids.md` written
- [ ] `artifacts.toml` updated

## Output Format

```text
Phase 4/6: Infrastructure — DONE

Files written:
  ✓ architecture/features/FEATURE-thread-safety.md  (~N lines)
  ✓ architecture/features/FEATURE-wire-protocol.md  (~N lines)
  ✓ .cypilot/.plans/implement-orm-features/out/phase-04-ids.md  (N IDs)
  ✓ .cypilot/config/artifacts.toml  (2 entries added)

IDs defined (Phase 4):
  [list all cpt-orm-* IDs]

Validation:
  Deterministic gate: SKIPPED
  Validator availability proof: cpt script not installed; no registered artifact path for validate --artifact
  Skip reason: cypilot.py scripts directory absent from this installation
  Validator-backed evidence note: none; deterministic validation was skipped
  Semantic review: template structure followed; no placeholders; no MUST NOT violations; state machine present in thread-safety; all IDs unique across phases 1–4
```

## Phase Handoff

After completion, update `plan.toml`: set `phases[3].status = "done"`.

```text
I have a Cypilot execution plan at:
  .cypilot/.plans/implement-orm-features/plan.toml

Phase 4 is complete (done).
Please read the plan manifest, confirm the next executable phase, and execute it.
The expected next phase file is: .cypilot/.plans/implement-orm-features/phase-05-language-tooling.md
The phase file is self-contained — follow its instructions exactly.
After completion, report results and generate the prompt for Phase 6.
```
