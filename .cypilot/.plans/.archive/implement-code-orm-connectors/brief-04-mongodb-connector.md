# Compilation Brief: Phase 4/12 — MongoDB Connector

--- CONTEXT BOUNDARY ---
Disregard all previous context. This brief is self-contained.
Read ONLY the files listed below. Follow the instructions exactly.
---

## Phase Metadata
```toml
[phase]
number = 4
total = 12
type = "implement"
title = "MongoDB Connector — connector_trait<MongoDB> + tests"
depends_on = [1]
input_files = [
  "architecture/features/FEATURE-mongodb-connector.md",
  "lib/include/ORM/connector/capabilities.hpp",
  "lib/include/ORM/connector/trait.hpp",
  "lib/include/ORM/db/connectors/MongoDB/mongodb_db.hpp",
]
output_files = [
  "lib/include/ORM/db/connectors/MongoDB/mongodb_db.hpp",
  "tests/unit/test_mongodb_connector.cpp",
]
outputs = ["out/phase-04-mongodb-summary.md"]
inputs = ["out/phase-01-scaffolding-summary.md"]
```

## Load Instructions

1. **Codebase rules**: Read `.cypilot/config/kits/sdlc/codebase/rules.md` (lines 60–260, ~200 lines)
   - Inline → Rules section; skip Traceability (DOCS-ONLY), Checkbox Cascade

2. **FEATURE spec**: Read `architecture/features/FEATURE-mongodb-connector.md` (whole file, ~279 lines)
   - Runtime read → Task step 1 (extract BSON rendering algos, hydration algo, DoD)

3. **capabilities.hpp**: Read `lib/include/ORM/connector/capabilities.hpp` (whole file, ~67 lines)
   - Runtime read → Task step 1

4. **Existing stub**: Read `lib/include/ORM/db/connectors/MongoDB/mongodb_db.hpp` (whole file)
   - Runtime read → Task step 2

**Do NOT load**: MySQL/PostgreSQL headers, other FEATURE specs, checklist.md.

## Compile Phase File
Write to: `.cypilot/.plans/implement-code-orm-connectors/phase-04-mongodb-connector.md`

Key differentiator: Document-oriented store — no SQL string generation. IR → BSON filter doc using `$and`/`$or`/`$eq`/`$gt`/`$lt` operators. Projection rendering. Mock libmongoc surface inline (MockCollection struct, stub mongoc_collection_find_with_opts / mongoc_cursor_next / mongoc_cursor_destroy). ObjectID type mapping test required. `supports_joins` MUST NOT be declared.

## Context Budget
- Phase file target: ≤ 600 lines
- Total execution context: phase (~600) + FEATURE (~279) + capabilities (~67) = ~946 lines — within budget
