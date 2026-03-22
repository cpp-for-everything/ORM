```toml
[phase]
plan = "implement-code-orm-connectors"
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

## Preamble

This is a self-contained phase file. All rules, constraints, and kit content
are included below. Project files listed in the Task section must be read
at runtime. Follow the instructions exactly, run any EXECUTE commands as
written, and report results against the acceptance criteria at the end.

## What

Create stub header files and INTERFACE CMakeLists.txt entries for all 9 new
ORM modules (6 database connectors + ThreadSafety + WireProtocol + migration).
Each stub header declares the DB tag type and leaves `connector_trait<DB>` to
be specialised in Phase 2–11. Each module gets a minimal INTERFACE CMakeLists
that wires into `orm::orm`. No business logic is implemented here — only the
directory/file skeleton that later phases fill in.

## Prior Context

- Project: brownfield C++ ORM library (`orm::orm` INTERFACE target, CMake 3.20+, C++23).
- Existing connectors: `MockDB` (header-only INTERFACE, `orm_mockdb`), `SQLite` (optional INTERFACE, `orm_sqlite`). Both follow the pattern: `add_library(orm_X INTERFACE)` + `target_link_libraries(orm_X INTERFACE orm::orm)`.
- `connector_trait<DB>` primary template is in `lib/include/ORM/connector/capabilities.hpp`. Unspecialised instantiation is a `static_assert(false)` hard error.
- Test framework: GoogleTest 1.14.0 via FetchContent. Test target: `test_unit` in `tests/unit/`.
- Traceability mode: DOCS-ONLY (`codebase = []` in `artifacts.toml`). No `@cpt-*` markers anywhere.
- All new connector headers follow Modern CMake: target-centric, `INTERFACE` visibility, no global commands.

## User Decisions

### Already Decided (pre-resolved during planning)
- **Traceability mode**: DOCS-ONLY — no `@cpt-*` markers in any generated code
- **Language standard**: C++23 (`target_compile_features(... cxx_std_23)`)
- **Test framework**: GoogleTest via FetchContent (already configured in `tests/CMakeLists.txt`)
- **CMake style**: Modern CMake — INTERFACE libraries, target-centric, no global commands
- **Lifecycle**: archive on completion

### Decisions Needed During This Phase
#### Confirmation Points
- [ ] **Confirm stub header structure** — review the list of created stub headers before proceeding to Phase 2

## Rules

### Structural
- MUST: Code follows project conventions — Modern CMake INTERFACE libraries, `#pragma once`, `namespace orm {}`
- MUST: Each stub header compiles cleanly under C++23 with `-Wall -Wextra -Wpedantic` (no warnings)
- MUST: No placeholder content (`TODO`, `TBD`, `FIXME`) in scaffolding headers
- MUST: Each module CMakeLists creates an `orm::{module_alias}` alias target linking to `orm::orm`

### Engineering
- MUST: **TDD** — stub headers provide the type declarations that test files will include; they must compile
- MUST: **SOLID / Interface Segregation** — each connector header is a separate INTERFACE library; no cross-module coupling in scaffolding
- MUST: **KISS** — stub headers are minimal: `#pragma once`, includes, tag type, forward declaration of specialisation only
- MUST: **YAGNI** — no business logic, no mock API structs in scaffold phase; those belong in Phase 2–11
- MUST NOT: use global CMake commands (`include_directories`, `link_libraries`, `add_compile_options`)
- MUST NOT: use raw `new`/`delete` in any C++ code
- MUST NOT: leave unresolved `{variable}` references outside code fences

### Quality
- MUST: Functions/methods are appropriately sized (scaffold = type declarations only, no function bodies)
- MUST: Tests cover implemented requirements — Phase 1 has no behavioural tests; structure validated by compiler

## Input

### INTERFACE CMakeLists.txt pattern (from MockDB):
```cmake
add_library(orm_XXXXX INTERFACE)
add_library(orm::XXXXX ALIAS orm_XXXXX)
target_link_libraries(orm_XXXXX INTERFACE orm::orm)
```

### Stub header pattern:
```cpp
#pragma once
#include "ORM/connector/capabilities.hpp"

namespace orm {

    // ── XXXDBTag ──────────────────────────────────────────────────────────────
    // Database tag type. Pass as DB template argument to orm::db<DB>.
    struct XXXDB {};

} // namespace orm
```

### lib/CMakeLists.txt `add_subdirectory` pattern:
Every new connector module must be added with `add_subdirectory(src/ORM/db/connectors/XXXXX)`.

## Task

1. **Read existing files** — Read `lib/CMakeLists.txt`, `lib/include/ORM/connector/capabilities.hpp`, `lib/include/ORM/db/connectors/MockDB/mock_db.hpp` (lines 1–30), and `tests/unit/CMakeLists.txt` to confirm the existing patterns before creating any new files.

2. **Create stub headers** — For each of the following modules, create the stub header at the given path using the pattern above:
   - `lib/include/ORM/db/connectors/MySQLDB/mysql_db.hpp` — tag type `MySQLDB`
   - `lib/include/ORM/db/connectors/PostgreSQLDB/postgresql_db.hpp` — tag type `PostgreSQLDB`
   - `lib/include/ORM/db/connectors/MongoDB/mongodb_db.hpp` — tag type `MongoDB`
   - `lib/include/ORM/db/connectors/RedisDB/redis_db.hpp` — tag type `RedisDB`
   - `lib/include/ORM/db/connectors/CassandraDB/cassandra_db.hpp` — tag type `CassandraDB`
   - `lib/include/ORM/db/connectors/Neo4jDB/neo4j_db.hpp` — tag type `Neo4jDB`
   - `lib/include/ORM/db/connectors/ThreadSafety/thread_safety.hpp` — note: declares `connection_pool`, `connection_guard`, `thread_local_db` forward declarations only
   - `lib/include/ORM/db/connectors/WireProtocol/wire_protocol.hpp` — note: declares `batch_insert`, `zero_copy_result` forward declarations only
   - `lib/include/ORM/db/migration/migration.hpp` — note: declares `orm::migrate<DB>`, `orm::ddl_op`, `orm::live_schema` forward declarations only

3. **Create module CMakeLists.txt files** — For each of the 9 modules, create a minimal INTERFACE CMakeLists at `lib/src/ORM/db/connectors/{Module}/CMakeLists.txt` (and `lib/src/ORM/db/migration/CMakeLists.txt` for migration):
   ```cmake
   add_library(orm_{module} INTERFACE)
   add_library(orm::{module} ALIAS orm_{module})
   target_link_libraries(orm_{module} INTERFACE orm::orm)
   ```
   Module names: `orm_mysql`, `orm_postgresql`, `orm_mongodb`, `orm_redis`, `orm_cassandra`, `orm_neo4j`, `orm_thread_safety`, `orm_wire_protocol`, `orm_migration`.

4. **Update `lib/CMakeLists.txt`** — Add `add_subdirectory` calls for all 9 new modules after the existing MockDB `add_subdirectory` line.

5. **Write intermediate output** — Create `.cypilot/.plans/implement-code-orm-connectors/out/phase-01-scaffolding-summary.md` listing all created files, module alias names, and confirming no business logic was added.

6. **Self-verify** — Confirm all acceptance criteria are met.

## Acceptance Criteria

- [ ] All 9 stub header files exist with correct `#pragma once`, tag type struct, and `namespace orm {}`
- [ ] All 9 module CMakeLists.txt files exist with `add_library(orm_X INTERFACE)` + `add_library(orm::X ALIAS orm_X)` + `target_link_libraries`
- [ ] `lib/CMakeLists.txt` contains `add_subdirectory` for all 9 new modules
- [ ] No TODO/TBD/FIXME in any created file
- [ ] No unresolved `{variable}` references outside code fences
- [ ] No global CMake commands used
- [ ] `out/phase-01-scaffolding-summary.md` exists and lists all created files

## Output Format

When complete, report results in this exact format:
```text
PHASE 1/12 COMPLETE
Status: PASS | FAIL
Files created: {list}
Files modified: {list}
Acceptance criteria:
  [x] Criterion 1 — PASS
  [ ] Criterion 2 — FAIL: {reason}
  ...
Line count: {actual}/{budget}
Notes: {any issues or decisions made}
```

Then generate a **copy-pasteable prompt** for the next phase inside a single code fence:

```text
Next phase prompt (copy-paste into new chat if needed):
```

```text
I have a Cypilot execution plan at:
  .cypilot/.plans/implement-code-orm-connectors/plan.toml

Phase 1 is complete (PASS).
Please read the plan manifest, then execute Phase 2: "MySQL Connector — connector_trait<MySQLDB> + tests".
The phase file is: .cypilot/.plans/implement-code-orm-connectors/phase-02-mysql-connector.md
It is self-contained — follow its instructions exactly.
After completion, report results and generate the prompt for the next phase.
```

If this is the **last phase**, instead of a next-phase prompt output:

```text
ALL PHASES COMPLETE (12/12)
Plan: .cypilot/.plans/implement-code-orm-connectors/plan.toml
Lifecycle: archive
```

Then ask: `Continue in this chat? [y/n]`
