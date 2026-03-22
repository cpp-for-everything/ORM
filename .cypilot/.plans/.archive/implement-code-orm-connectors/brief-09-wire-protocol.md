# Compilation Brief: Phase 9/12 — Wire Protocol

--- CONTEXT BOUNDARY ---
Disregard all previous context. This brief is self-contained.
Read ONLY the files listed below. Follow the instructions exactly.
---

## Phase Metadata
```toml
[phase]
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

## Load Instructions

1. **Codebase rules**: Read `.cypilot/config/kits/sdlc/codebase/rules.md` (lines 60–260, ~200 lines)
   - Inline → Rules section; skip Traceability (DOCS-ONLY), Checkbox Cascade

2. **FEATURE spec**: Read `architecture/features/FEATURE-wire-protocol.md` (whole file, ~299 lines)
   - Runtime read → Task step 1 (extract four optimisation algos: io_uring dispatch, zero-copy result, batch INSERT render, constexpr SQL gen; two flows; DoD)

3. **capabilities.hpp + db.hpp**: Runtime read → Task step 1 (understand db<DB> execute overloads to add async overload)

4. **Existing stub**: Read `lib/include/ORM/db/connectors/WireProtocol/wire_protocol.hpp` (whole file) → Task step 2

**Do NOT load**: other connector headers, other FEATURE specs, checklist.md.

## Compile Phase File
Write to: `.cypilot/.plans/implement-code-orm-connectors/phase-09-wire-protocol.md`

Key deliverables (all header-only, platform-guarded):
1. `orm::io_uring_awaitable<DB>` (Linux `#ifdef __linux__`): `await_ready()` returns false; `await_suspend()` submits SQE; `await_resume()` hydrates result.
2. `orm::iocp_awaitable<DB>` (Windows `#ifdef _WIN32`): equivalent for IOCP overlapped I/O.
3. `co_await db.execute()` overload in `db<DB>` that returns the platform awaitable.
4. `orm::zero_copy_result<DB>` span accessor wrapping driver buffer.
5. `orm::batch_insert<DB, Entity>` query type accumulating rows → single multi-row INSERT.
6. `constexpr` SQL renderer: `if constexpr` path for fully-static IRs on connectors with `supports_constexpr_sql`.
7. Unit tests: await_ready=false; batch_insert N rows → 1 execute call; constexpr SQL static_assert; zero-copy span data() pointer equality (mock driver buffer).

## Context Budget
- Phase file target: ≤ 700 lines
- Total execution context: phase (~700) + FEATURE (~299) + capabilities (~67) + db.hpp (~85) = ~1,151 lines — within budget
