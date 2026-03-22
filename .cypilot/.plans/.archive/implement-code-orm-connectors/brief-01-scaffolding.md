# Compilation Brief: Phase 1/12 — Scaffolding

--- CONTEXT BOUNDARY ---
Disregard all previous context. This brief is self-contained.
Read ONLY the files listed below. Follow the instructions exactly.
---

## Phase Metadata
```toml
[phase]
number = 1
total = 12
type = "implement"
title = "Scaffolding — directories, CMakeLists, and stub headers"
depends_on = []
input_files = [
  "lib/CMakeLists.txt",
  "lib/include/ORM/connector/capabilities.hpp",
  "lib/include/ORM/db/connectors/MockDB/mock_db.hpp",
  "tests/unit/CMakeLists.txt",
]
output_files = [
  "lib/include/ORM/db/connectors/MySQLDB/mysql_db.hpp",
  "lib/include/ORM/db/connectors/PostgreSQLDB/postgresql_db.hpp",
  "lib/include/ORM/db/connectors/MongoDB/mongodb_db.hpp",
  "lib/include/ORM/db/connectors/RedisDB/redis_db.hpp",
  "lib/include/ORM/db/connectors/CassandraDB/cassandra_db.hpp",
  "lib/include/ORM/db/connectors/Neo4jDB/neo4j_db.hpp",
  "lib/include/ORM/db/connectors/ThreadSafety/thread_safety.hpp",
  "lib/include/ORM/db/connectors/WireProtocol/wire_protocol.hpp",
  "lib/include/ORM/db/migration/migration.hpp",
  "lib/src/ORM/db/connectors/MySQLDB/CMakeLists.txt",
  "lib/src/ORM/db/connectors/PostgreSQLDB/CMakeLists.txt",
  "lib/src/ORM/db/connectors/MongoDB/CMakeLists.txt",
  "lib/src/ORM/db/connectors/RedisDB/CMakeLists.txt",
  "lib/src/ORM/db/connectors/CassandraDB/CMakeLists.txt",
  "lib/src/ORM/db/connectors/Neo4jDB/CMakeLists.txt",
  "lib/src/ORM/db/connectors/ThreadSafety/CMakeLists.txt",
  "lib/src/ORM/db/connectors/WireProtocol/CMakeLists.txt",
  "lib/src/ORM/db/migration/CMakeLists.txt",
]
outputs = ["out/phase-01-scaffolding-summary.md"]
inputs = []
```

## Load Instructions

1. **Codebase rules**: Read `.cypilot/config/kits/sdlc/codebase/rules.md` (lines 60–160, ~100 lines)
   - Inline → Rules section (Requirements: Structural, Engineering, Quality; Tasks: Phase 1 Setup)
   - Skip Prerequisites details, Traceability (DOCS-ONLY mode), Checkbox Cascade, Validation

2. **Existing lib CMakeLists.txt**: Read `lib/CMakeLists.txt` (whole file, ~78 lines)
   - Runtime read → Task step 1 (understand existing connector pattern)

3. **capabilities.hpp**: Read `lib/include/ORM/connector/capabilities.hpp` (whole file, ~67 lines)
   - Runtime read → Task step 1 (understand connector_trait + cap:: tags)

4. **MockDB pattern**: Read `lib/include/ORM/db/connectors/MockDB/mock_db.hpp` (lines 1–30, ~30 lines)
   - Runtime read → Task step 1 (understand INTERFACE library + header-only pattern)

5. **tests/unit/CMakeLists.txt**: Read `tests/unit/CMakeLists.txt` (whole file, ~18 lines)
   - Runtime read → Task step 3 (understand test target pattern)

**Do NOT load**: checklist.md, FEATURE templates, example files, integration test files.

## Compile Phase File
Write to: `.cypilot/.plans/implement-code-orm-connectors/phase-01-scaffolding.md`

Required sections:
1. TOML frontmatter
2. Preamble (verbatim)
3. What — create all stub headers + CMakeLists for 8 new connector modules + migration module
4. Prior Context — brownfield C++ ORM project; MockDB + SQLite already exist as INTERFACE libraries
5. User Decisions — traceability=DOCS-ONLY, language=C++23, test framework=GoogleTest
6. Rules — inline MUST/MUST NOT from codebase/rules.md Structural + Engineering
7. Input — inline the INTERFACE CMakeLists.txt pattern from MockDB
8. Task — 5 steps: read existing, create headers, create CMakeLists, update lib/CMakeLists.txt, write out/
9. Acceptance Criteria — all files exist, compile cleanly, no TODO/placeholders in scaffolding
10. Output Format (verbatim)

## Context Budget
- Phase file target: ≤ 600 lines
- Inlined content estimate: ~120 lines (rules excerpt + CMake pattern)
- Total execution context: ≤ 1500 lines
- If Rules exceeds 300 lines, narrow scope — NEVER drop rules
