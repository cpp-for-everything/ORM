# Compilation Brief: Phase 7/12 — Neo4j Connector

--- CONTEXT BOUNDARY ---
Disregard all previous context. This brief is self-contained.
Read ONLY the files listed below. Follow the instructions exactly.
---

## Phase Metadata
```toml
[phase]
number = 7
total = 12
type = "implement"
title = "Neo4j Connector — connector_trait<Neo4jDB> + tests"
depends_on = [1]
input_files = [
  "architecture/features/FEATURE-neo4j-connector.md",
  "lib/include/ORM/connector/capabilities.hpp",
  "lib/include/ORM/connector/trait.hpp",
  "lib/include/ORM/db/connectors/Neo4jDB/neo4j_db.hpp",
]
output_files = [
  "lib/include/ORM/db/connectors/Neo4jDB/neo4j_db.hpp",
  "tests/unit/test_neo4j_connector.cpp",
]
outputs = ["out/phase-07-neo4j-summary.md"]
inputs = ["out/phase-01-scaffolding-summary.md"]
```

## Load Instructions

1. **Codebase rules**: Read `.cypilot/config/kits/sdlc/codebase/rules.md` (lines 60–260, ~200 lines)
   - Inline → Rules section; skip Traceability (DOCS-ONLY), Checkbox Cascade

2. **FEATURE spec**: Read `architecture/features/FEATURE-neo4j-connector.md` (whole file, ~280 lines)
   - Runtime read → Task step 1 (extract IR-to-Cypher rendering algo, Bolt dispatch algo, hydration algo, traverse<R>() extension, DoD)

3. **capabilities.hpp**: Read `lib/include/ORM/connector/capabilities.hpp` (whole file, ~67 lines)
   - Runtime read → Task step 1

4. **Existing stub**: Read `lib/include/ORM/db/connectors/Neo4jDB/neo4j_db.hpp` (whole file)
   - Runtime read → Task step 2

**Do NOT load**: other connector headers, other FEATURE specs, checklist.md.

## Compile Phase File
Write to: `.cypilot/.plans/implement-code-orm-connectors/phase-07-neo4j-connector.md`

Key differentiators:
- Graph store — Cypher query language, not SQL. SELECT → MATCH/RETURN; WHERE → WHERE (Cypher boolean).
- `.traverse<Relationship>()` IR extension: renders `(n)-[:REL]->(m:Label)` patterns in MATCH clause.
- Named `$p1`, `$p2`, ... parameters in Cypher string + parameter map for Bolt.
- Mock libneo4j-client surface inline (MockNeo4jConnection struct; stub neo4j_run, neo4j_fetch_next, neo4j_close_results).
- Known limitation: unsupported IR constructs detected only at execute time — documented in Known Limitations.
- `supports_aggregation` MUST NOT be declared unless aggregation is implemented.

## Context Budget
- Phase file target: ≤ 600 lines
- Total execution context: phase (~600) + FEATURE (~280) + capabilities (~67) = ~947 lines — within budget
