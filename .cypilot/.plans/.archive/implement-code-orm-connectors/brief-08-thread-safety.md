# Compilation Brief: Phase 8/12 — Thread Safety

--- CONTEXT BOUNDARY ---
Disregard all previous context. This brief is self-contained.
Read ONLY the files listed below. Follow the instructions exactly.
---

## Phase Metadata
```toml
[phase]
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
  "tests/unit/test_thread_safety.cpp",
]
outputs = ["out/phase-08-thread-safety-summary.md"]
inputs = ["out/phase-01-scaffolding-summary.md"]
```

## Load Instructions

1. **Codebase rules**: Read `.cypilot/config/kits/sdlc/codebase/rules.md` (lines 60–260, ~200 lines)
   - Inline → Rules section; skip Traceability (DOCS-ONLY), Checkbox Cascade

2. **FEATURE spec**: Read `architecture/features/FEATURE-thread-safety.md` (whole file, ~276 lines)
   - Runtime read → Task step 1 (extract pool-acquire algo, capability-check algo, connection state machine, DoD)

3. **capabilities.hpp + db.hpp**: Read `lib/include/ORM/connector/capabilities.hpp` (~67 lines) and `lib/include/ORM/connector/db.hpp` (~85 lines)
   - Runtime read → Task step 1 (understand existing db<DB> class to extend with transaction())

4. **MockDB reference**: Read `lib/include/ORM/db/connectors/MockDB/mock_db.hpp` (lines 244–260, ~16 lines)
   - Runtime read → Task step 1 (understand capability tag declarations pattern)

5. **Existing stub**: Read `lib/include/ORM/db/connectors/ThreadSafety/thread_safety.hpp` (whole file)
   - Runtime read → Task step 2

**Do NOT load**: other connector headers (except MockDB capabilities), other FEATURE specs, checklist.md.

## Compile Phase File
Write to: `.cypilot/.plans/implement-code-orm-connectors/phase-08-thread-safety.md`

Key deliverables:
- `orm::connection_pool<DB, N>` template with RAII `connection_guard<DB>` (mutex + condition_variable).
- `static_assert` at pool instantiation when `connector_trait<DB>` lacks `supports_concurrent_execute`.
- `orm::thread_local_db<DB>` helper (thread_local pattern).
- `db::transaction()` RAII guard: BEGIN on construction, COMMIT on commit(), ROLLBACK on destruction.
- `static_assert` at `db::transaction()` when `connector_trait<DB>` lacks `supports_transactions`.
- Connection state machine: Idle → InUse → Closed.
- Tests: pool N=1 two-thread acquire (blocking), static_assert for missing tag, transaction auto-rollback, thread_local_db independence.
- Add `supports_concurrent_execute` to `MockDB` capabilities so tests compile.

## Context Budget
- Phase file target: ≤ 700 lines (larger scope; budget extended per decomposition rules)
- Total execution context: phase (~700) + FEATURE (~276) + db.hpp (~85) + capabilities (~67) = ~1,128 lines — within budget
