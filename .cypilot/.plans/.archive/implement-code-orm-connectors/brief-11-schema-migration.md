# Compilation Brief: Phase 11/12 — Schema Migration

--- CONTEXT BOUNDARY ---
Disregard all previous context. This brief is self-contained.
Read ONLY the files listed below. Follow the instructions exactly.
---

## Phase Metadata
```toml
[phase]
number = 11
total = 12
type = "implement"
title = "Schema Migration — orm::migrate<DB> diff + DDL generation + dry-run + tests"
depends_on = [2, 3]
input_files = [
  "architecture/features/FEATURE-schema-migration.md",
  "lib/include/ORM/connector/capabilities.hpp",
  "lib/include/ORM/connector/db.hpp",
  "lib/include/ORM/db/connectors/MockDB/mock_db.hpp",
]
output_files = [
  "lib/include/ORM/db/migration/migration.hpp",
  "tests/unit/test_schema_migration.cpp",
]
outputs = ["out/phase-11-migration-summary.md"]
inputs = ["out/phase-01-scaffolding-summary.md"]
```

## Load Instructions

1. **Codebase rules**: Read `.cypilot/config/kits/sdlc/codebase/rules.md` (lines 60–260, ~200 lines)
   - Inline → Rules section; skip Traceability (DOCS-ONLY), Checkbox Cascade

2. **FEATURE spec**: Read `architecture/features/FEATURE-schema-migration.md` (whole file, ~311 lines)
   - Runtime read → Task step 1 (extract two flows, three algos, state machine, DoD)

3. **capabilities.hpp + db.hpp**: Runtime read → Task step 1 (understand db<DB> and connector_trait extension point)

4. **MockDB**: Read `lib/include/ORM/db/connectors/MockDB/mock_db.hpp` (lines 244–260, ~16 lines)
   - Runtime read → Task step 1 (understand connector_trait pattern to add ddl_for() extension)

**Do NOT load**: other connector headers, other FEATURE specs, checklist.md.

## Compile Phase File
Write to: `.cypilot/.plans/implement-code-orm-connectors/phase-11-schema-migration.md`

Key deliverables (all header-only):
1. `orm::ddl_op` variant type: `create_table`, `add_column`, `drop_column`, `alter_column_type`.
2. `connector_trait<DB>::ddl_for(op)` extension point — add to `connector_trait<MockDB>` and `connector_trait<SQLiteDB>` (if SQLite is available).
3. `orm::live_schema` struct: runtime map `table_name → {column_name → column_type}`.
4. `orm::migrate<DB>` class:
   - Constructor: `(DB& conn, std::span<const orm::entity_meta> entities)`.
   - `diff()` → `std::vector<orm::ddl_op>` (entity-to-DDL diff algo).
   - `generate_ddl(const std::vector<orm::ddl_op>&)` → `std::string`.
   - `run(bool dry_run)` → `int` (0=in sync, 2=drift detected; interactive approval skipped in dry-run).
5. `orm::entity_meta` compile-time descriptor struct.
6. Compile-time column reference safety: concept/requires clause ensuring query IR column ref resolves to entity property.
7. Migration state machine: Idle → Connected → Diffing → Reviewing → Executing → Done/Failed.
8. Unit tests: diff engine (mock live schema vs entities → CREATE TABLE, ADD/DROP COLUMN); DDL generation for MockDB; dry-run exit codes; compile-time safety static_assert.

## Context Budget
- Phase file target: ≤ 700 lines
- Total execution context: phase (~700) + FEATURE (~311) + capabilities (~67) + db.hpp (~85) = ~1,163 lines — within budget
