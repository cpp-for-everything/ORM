# Compilation Brief: Phase 2/12 — MySQL Connector

--- CONTEXT BOUNDARY ---
Disregard all previous context. This brief is self-contained.
Read ONLY the files listed below. Follow the instructions exactly.
---

## Phase Metadata
```toml
[phase]
number = 2
total = 12
type = "implement"
title = "MySQL Connector — connector_trait<MySQLDB> + tests"
depends_on = [1]
input_files = [
  "architecture/features/FEATURE-mysql-connector.md",
  "lib/include/ORM/connector/capabilities.hpp",
  "lib/include/ORM/connector/trait.hpp",
  "lib/include/ORM/db/connectors/MockDB/mock_db.hpp",
  "lib/include/ORM/db/connectors/MySQLDB/mysql_db.hpp",
]
output_files = [
  "lib/include/ORM/db/connectors/MySQLDB/mysql_db.hpp",
  "tests/unit/test_mysql_connector.cpp",
]
outputs = ["out/phase-02-mysql-summary.md"]
inputs = ["out/phase-01-scaffolding-summary.md"]
```

## Load Instructions

1. **Codebase rules**: Read `.cypilot/config/kits/sdlc/codebase/rules.md` (lines 60–260, ~200 lines)
   - Inline → Rules section (Structural, Engineering, Quality; Tasks Phase 2 Implementation)
   - Skip Traceability (DOCS-ONLY), Checkbox Cascade, Versioning details

2. **FEATURE spec**: Read `architecture/features/FEATURE-mysql-connector.md` (whole file, ~247 lines)
   - Runtime read → Task step 1 (extract flows, algos, DoD items to implement)

3. **capabilities.hpp**: Read `lib/include/ORM/connector/capabilities.hpp` (whole file, ~67 lines)
   - Runtime read → Task step 1 (understand connector_trait primary template and cap:: tags)

4. **MockDB reference**: Read `lib/include/ORM/db/connectors/MockDB/mock_db.hpp` (lines 244–378, ~134 lines)
   - Runtime read → Task step 1 (connector_trait specialisation pattern to follow)

5. **Existing stub**: Read `lib/include/ORM/db/connectors/MySQLDB/mysql_db.hpp` (whole file)
   - Runtime read → Task step 2 (complete from scaffold)

**Do NOT load**: other FEATURE specs, PostgreSQL/MongoDB headers, checklist.md.

## Compile Phase File
Write to: `.cypilot/.plans/implement-code-orm-connectors/phase-02-mysql-connector.md`

Required sections:
1. TOML frontmatter
2. Preamble (verbatim)
3. What — implement `connector_trait<MySQLDB>` header-only (no libmysqlclient dependency; mock MySQL C API surface inline) + unit tests
4. Prior Context — Phase 1 created stub; MockDB pattern is the reference; `connector_trait<MySQLDB>` must satisfy `is_connector<MySQLDB>`; C API calls are mocked inline for testing (no live MySQL required)
5. User Decisions — traceability=DOCS-ONLY, no @cpt-* markers; mock boundary = inline MockMySQLHandle struct
6. Rules — inline from codebase/rules.md: Structural (code implements FEATURE), Engineering (TDD, SOLID, KISS), Quality (no TODO/FIXME)
7. Input — inline FEATURE DoD items verbatim (§5 DoD from FEATURE spec); inline key acceptance criteria (§6) for TDD anchor
8. Task — 7 steps: read FEATURE spec → extract DoD → implement wire_type + cursor_type + MockMySQLHandle struct → implement execute() overloads (SELECT/INSERT/UPDATE/DELETE) → implement indexed-placeholder rewrite → write unit tests → self-verify
9. Acceptance Criteria — connector_trait<MySQLDB> satisfies is_connector concept; indexed placeholder rewrite test passes; RAII test present; no TODO/FIXME
10. Output Format (verbatim)

## Context Budget
- Phase file target: ≤ 600 lines
- Inlined content estimate: ~200 lines (rules + DoD items)
- Total execution context: phase (~600) + FEATURE (~247) + MockDB (~134) + capabilities (~67) = ~1,048 lines
- Within budget
