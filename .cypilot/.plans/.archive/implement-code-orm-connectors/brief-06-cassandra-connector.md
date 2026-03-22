# Compilation Brief: Phase 6/12 — Cassandra Connector

--- CONTEXT BOUNDARY ---
Disregard all previous context. This brief is self-contained.
Read ONLY the files listed below. Follow the instructions exactly.
---

## Phase Metadata
```toml
[phase]
number = 6
total = 12
type = "implement"
title = "Cassandra Connector — connector_trait<CassandraDB> + tests"
depends_on = [1]
input_files = [
  "architecture/features/FEATURE-cassandra-connector.md",
  "lib/include/ORM/connector/capabilities.hpp",
  "lib/include/ORM/connector/trait.hpp",
  "lib/include/ORM/db/connectors/CassandraDB/cassandra_db.hpp",
]
output_files = [
  "lib/include/ORM/db/connectors/CassandraDB/cassandra_db.hpp",
  "tests/unit/test_cassandra_connector.cpp",
]
outputs = ["out/phase-06-cassandra-summary.md"]
inputs = ["out/phase-01-scaffolding-summary.md"]
```

## Load Instructions

1. **Codebase rules**: Read `.cypilot/config/kits/sdlc/codebase/rules.md` (lines 60–260, ~200 lines)
   - Inline → Rules section; skip Traceability (DOCS-ONLY), Checkbox Cascade

2. **FEATURE spec**: Read `architecture/features/FEATURE-cassandra-connector.md` (whole file, ~270 lines)
   - Runtime read → Task step 1 (extract partition-key validation algo, CQL rendering algo, hydration algo, DoD)

3. **capabilities.hpp**: Read `lib/include/ORM/connector/capabilities.hpp` (whole file, ~67 lines)
   - Runtime read → Task step 1

4. **Existing stub**: Read `lib/include/ORM/db/connectors/CassandraDB/cassandra_db.hpp` (whole file)
   - Runtime read → Task step 2

**Do NOT load**: other connector headers, other FEATURE specs, checklist.md.

## Compile Phase File
Write to: `.cypilot/.plans/implement-code-orm-connectors/phase-06-cassandra-connector.md`

Key differentiators:
- Compile-time partition key validation: `static_assert` when WHERE clause lacks partition key equality predicate.
- CQL uses positional `?` parameters (same as MySQL) — indexed placeholder reuse duplicates bind values.
- Mock DataStax C++ Driver surface inline (MockCassSession struct; stub cass_session_prepare, cass_statement_bind_*, cass_session_execute, cass_result_free).
- `supports_joins` MUST NOT be declared.
- Test: verify `static_assert` fires for missing partition key (use SFINAE/concept test, not a runtime test).

## Context Budget
- Phase file target: ≤ 600 lines
- Total execution context: phase (~600) + FEATURE (~270) + capabilities (~67) = ~937 lines — within budget
