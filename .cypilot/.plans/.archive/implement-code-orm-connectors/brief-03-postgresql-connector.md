# Compilation Brief: Phase 3/12 — PostgreSQL Connector

--- CONTEXT BOUNDARY ---
Disregard all previous context. This brief is self-contained.
Read ONLY the files listed below. Follow the instructions exactly.
---

## Phase Metadata
```toml
[phase]
number = 3
total = 12
type = "implement"
title = "PostgreSQL Connector — connector_trait<PostgreSQLDB> + tests"
depends_on = [1]
input_files = [
  "architecture/features/FEATURE-postgresql-connector.md",
  "lib/include/ORM/connector/capabilities.hpp",
  "lib/include/ORM/connector/trait.hpp",
  "lib/include/ORM/db/connectors/PostgreSQLDB/postgresql_db.hpp",
]
output_files = [
  "lib/include/ORM/db/connectors/PostgreSQLDB/postgresql_db.hpp",
  "tests/unit/test_postgresql_connector.cpp",
]
outputs = ["out/phase-03-postgresql-summary.md"]
inputs = ["out/phase-01-scaffolding-summary.md"]
```

## Load Instructions

1. **Codebase rules**: Read `.cypilot/config/kits/sdlc/codebase/rules.md` (lines 60–260, ~200 lines)
   - Inline → Rules section; skip Traceability (DOCS-ONLY), Checkbox Cascade

2. **FEATURE spec**: Read `architecture/features/FEATURE-postgresql-connector.md` (whole file, ~261 lines)
   - Runtime read → Task step 1 (extract flows, algos, DoD)

3. **capabilities.hpp**: Read `lib/include/ORM/connector/capabilities.hpp` (whole file, ~67 lines)
   - Runtime read → Task step 1

4. **Existing stub**: Read `lib/include/ORM/db/connectors/PostgreSQLDB/postgresql_db.hpp` (whole file)
   - Runtime read → Task step 2 (complete from scaffold)

**Do NOT load**: MySQLDB header, other FEATURE specs, checklist.md.

## Compile Phase File
Write to: `.cypilot/.plans/implement-code-orm-connectors/phase-03-postgresql-connector.md`

Key differentiator from MySQL phase: PostgreSQL uses `$N` numbered parameters (native indexed-placeholder reuse — no argument duplication). Mock libpq surface inline (MockPGconn struct, stub PQprepare/PQexecPrepared/PQclear). Result hydration algo (`cpt-orm-algo-postgresql-connector-hydrate-result`) must also be implemented.

## Context Budget
- Phase file target: ≤ 600 lines
- Total execution context: phase (~600) + FEATURE (~261) + capabilities (~67) = ~928 lines — within budget
