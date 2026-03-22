```toml
[phase]
plan = "implement-code-orm-connectors"
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

## Preamble

This is a self-contained phase file. All rules, constraints, and kit content
are included below. Project files listed in the Task section must be read
at runtime. Follow the instructions exactly, run any EXECUTE commands as
written, and report results against the acceptance criteria at the end.

## What

Implement `connector_trait<MySQLDB>` as a complete header-only C++23 specialisation in
`lib/include/ORM/db/connectors/MySQLDB/mysql_db.hpp`. The connector translates the ORM
compile-time query IR to MySQL C API prepared-statement calls. Because no live MySQL
server is required, a `MockMySQLHandle` struct that stubs the MySQL C API surface is
defined inline in the header (guarded by the test/mock path). Write unit tests in
`tests/unit/test_mysql_connector.cpp` using GoogleTest. Update `tests/unit/CMakeLists.txt`
to include the new test file.

## Prior Context

- Phase 1 created `lib/include/ORM/db/connectors/MySQLDB/mysql_db.hpp` as a stub.
- `connector_trait<DB>` primary template in `capabilities.hpp` fires `static_assert(false)` if not specialised.
- `is_connector<DB>` concept requires: `connector_trait<DB>::wire_type<int>::type` and `connector_trait<DB>::cursor_type`.
- MockDB (reference): `connector_trait<MockDB>` has `supports_joins`, `supports_transactions`, `wire_type<T>{using type=T;}`, `cursor_type`, and `execute()` overloads for SELECT/INSERT/UPDATE/DELETE with and without runtime params.
- Traceability mode: DOCS-ONLY — no `@cpt-*` markers anywhere.
- All connector IDs in FEATURE spec: `cpt-orm-flow-mysql-connector-*`, `cpt-orm-algo-mysql-connector-*`, `cpt-orm-dod-mysql-connector-*`.

## User Decisions

### Already Decided (pre-resolved during planning)
- **Traceability mode**: DOCS-ONLY — no `@cpt-*` markers in code
- **Mock strategy**: inline `MockMySQLHandle` struct in the header file (no external MySQL headers needed to compile)
- **Live MySQL**: not required — unit tests use `MockMySQLHandle` exclusively
- **Language**: C++23; test framework: GoogleTest

### Decisions Needed During This Phase
#### Review Gates
- [ ] **Review connector_trait<MySQLDB> execute() overloads** — confirm SELECT/INSERT/UPDATE/DELETE are present before writing test file

## Rules

### Structural
- MUST: `connector_trait<MySQLDB>` specialisation MUST satisfy the `is_connector<MySQLDB>` concept:
  - MUST provide `template<typename T> struct wire_type { using type = T; }`
  - MUST provide `struct cursor_type { bool has_next() const noexcept; }`
- MUST: `connector_trait<MySQLDB>` MUST declare capability tags as nested `using` aliases:
  - MUST declare `using supports_joins = void;`
  - MUST declare `using supports_transactions = void;`
  - MUST NOT declare capability tags not supported by MySQL at this connector layer
- MUST: Indexed-placeholder rewrite MUST expand `orm::ph<T, std::placeholders::_N>` to positional `?` tokens, duplicating the runtime argument in `MYSQL_BIND` at each reuse site
- MUST: SQL injection is prevented by construction — parameters MUST be bound via `mysql_stmt_bind_param`; MUST NOT concatenate user-supplied values into the SQL string
- MUST: Code implements all DoD items from FEATURE-mysql-connector.md §5
- MUST: Code follows project C++23 conventions (4-space indent, 120-char line limit, CamelCase types, lower_case functions)

### Engineering
- MUST: **TDD** — write test assertions before/alongside implementation; tests must be runnable
- MUST: **SOLID / Dependency Inversion** — `MockMySQLHandle` is injected as the connection handle; no global MySQL state
- MUST: **KISS** — use `std::string` + `std::format` for SQL rendering (same pattern as MockDB); no premature optimisation
- MUST: **Error handling** — all `execute()` overloads return `orm::result<...>`; propagate error state without throwing
- MUST NOT: leave `TODO`/`TBD`/`FIXME` in delivered files
- MUST NOT: use raw `new`/`delete`

### Quality
- MUST: Unit tests cover: (a) execute() returns correct SQL string for SELECT with positional params; (b) indexed-placeholder rewrite — `_1` appearing twice produces two `?` in SQL; (c) RAII cleanup — `mysql_stmt_close` called exactly once on normal and error paths; (d) `connector_trait<MySQLDB>` satisfies `is_connector<MySQLDB>`
- MUST: No placeholder content in delivered files

## Input

### Inlined DoD items from FEATURE-mysql-connector.md §5 (reference for implementation):

**DoD: `connector_trait<MySQLDB>` Specialisation** (`cpt-orm-dod-mysql-connector-trait-specialisation`)
- MUST provide a complete `connector_trait<MySQLDB>` specialisation compiling cleanly with C++23, satisfying `ConnectorTrait` concept.
- Implements: `cpt-orm-flow-mysql-connector-execute-query`, `cpt-orm-algo-mysql-connector-render-sql`

**DoD: Prepared-Statement Lifecycle** (`cpt-orm-dod-mysql-connector-prepared-stmt`)
- MUST correctly manage: `mysql_stmt_init` → `mysql_stmt_prepare` → `mysql_stmt_bind_param` → `mysql_stmt_execute` → `mysql_stmt_bind_result` → `mysql_stmt_fetch` → `mysql_stmt_close`, including RAII cleanup on destruction.

**DoD: Indexed Placeholder Rewrite** (`cpt-orm-dod-mysql-connector-indexed-ph-rewrite`)
- MUST correctly rewrite indexed placeholders to positional `?` tokens, duplicating the runtime argument at each reuse site, such that bound values match SQL string positions exactly.

**DoD: Capability Tags** (`cpt-orm-dod-mysql-connector-capability-tags`)
- MUST declare `supports_joins`, `supports_transactions` as nested types. Undeclared tags MUST NOT be declared.

### Acceptance criteria from FEATURE §6 (TDD anchors):
- `connector_trait<MySQLDB>` compiles with no errors or warnings under C++23 with `-Wall -Wextra`.
- SELECT with positional `Placeholder<T>` executes and returns correct rows.
- SELECT using `orm::ph<T, std::placeholders::_1>` reused in two WHERE conditions: bound value appears at both positions.
- `mysql_stmt_close` called exactly once per prepared statement, including error path.
- Accessing undeclared capability tag produces `static_assert` compile error.

## Task

1. **Read input files** — Read `architecture/features/FEATURE-mysql-connector.md` (full), `lib/include/ORM/connector/capabilities.hpp`, `lib/include/ORM/connector/trait.hpp`, `lib/include/ORM/db/connectors/MockDB/mock_db.hpp` (lines 244–378 for the specialisation pattern), and the existing stub `lib/include/ORM/db/connectors/MySQLDB/mysql_db.hpp`.

2. **Implement `mysql_db.hpp`** — Replace/complete the stub with the full implementation:
   - Define `struct MockMySQLHandle` with fields: `mutable std::string last_sql`, `mutable std::vector<std::string> last_params`, `mutable int stmt_close_count = 0`. Add stub methods: `prepare(sql)`, `bind_param(arr)`, `execute()`, `fetch()`, `close_stmt()` that record calls.
   - Define `struct MySQLDB` (the real tag, holds no state — caller provides handle).
   - Implement `namespace orm { template<> struct connector_trait<MySQLDB> { ... }; }` with:
     - `using supports_joins = void; using supports_transactions = void;`
     - `template<typename T> struct wire_type { using type = T; };`
     - `struct cursor_type { bool has_next() const noexcept { return false; } };`
     - SQL rendering helpers in `namespace mysql_detail {}` (render_columns, render_wheres, render_order_by, render_limits — can reuse the mockdb pattern structure but emit `?` positional placeholders)
     - Indexed-placeholder rewrite: `render_indexed_ph()` function that scans the placeholder index, emits `?` per occurrence, and duplicates the bind value
     - `execute()` overloads for SELECT (no params), SELECT (with params), INSERT (no params), INSERT (with params) — each renders SQL to `conn.last_sql` and params to `conn.last_params`; returns `orm::result<...>{}`

3. **Review gate** — Confirm all four `execute()` overloads are present (SELECT no-params, SELECT with-params, INSERT no-params, INSERT with-params). Add UPDATE and DELETE overloads for completeness.

4. **Write unit tests** — Create `tests/unit/test_mysql_connector.cpp`:
   - Include `<gtest/gtest.h>` and `"ORM/db/connectors/MySQLDB/mysql_db.hpp"`
   - `TEST(MySQLConnector, SatisfiesIsConnector)` — `static_assert(orm::is_connector<orm::MySQLDB>)`
   - `TEST(MySQLConnector, SelectPositionalPlaceholder)` — build a SELECT with one `Placeholder<int>`, execute, verify `last_sql` contains `?` and `last_params` contains the value
   - `TEST(MySQLConnector, IndexedPlaceholderRewrite)` — `_1` used twice in WHERE: verify SQL has two `?` and `last_params` has the value duplicated
   - `TEST(MySQLConnector, RaiiStmtClose)` — execute, then destroy; verify `stmt_close_count == 1`
   - `TEST(MySQLConnector, CapabilityTagsPresent)` — `static_assert(orm::has_capability<orm::MySQLDB, orm::cap::supports_joins>)`

5. **Update `tests/unit/CMakeLists.txt`** — Add `test_mysql_connector.cpp` to `add_executable(test_unit ...)` sources.

6. **Write intermediate output** — Create `.cypilot/.plans/implement-code-orm-connectors/out/phase-02-mysql-summary.md` with: files created/modified, DoD items implemented, test count.

7. **Self-verify** — Check all acceptance criteria.

## Acceptance Criteria

- [ ] `lib/include/ORM/db/connectors/MySQLDB/mysql_db.hpp` contains full `connector_trait<MySQLDB>` specialisation
- [ ] `static_assert(orm::is_connector<orm::MySQLDB>)` compiles (verified by test)
- [ ] Indexed-placeholder rewrite test present and logically correct (two `?` for one `_1` reused twice)
- [ ] RAII `stmt_close_count` test present and verifies cleanup on normal path
- [ ] Capability tags `supports_joins` and `supports_transactions` declared; no other tags declared
- [ ] `tests/unit/test_mysql_connector.cpp` exists with ≥ 4 TEST() cases
- [ ] `tests/unit/CMakeLists.txt` updated to include new test file
- [ ] No TODO/TBD/FIXME in delivered files
- [ ] No unresolved `{variable}` references outside code fences
- [ ] `out/phase-02-mysql-summary.md` exists

## Output Format

When complete, report results in this exact format:
```text
PHASE 2/12 COMPLETE
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

Phase 2 is complete (PASS).
Please read the plan manifest, then execute Phase 3: "PostgreSQL Connector — connector_trait<PostgreSQLDB> + tests".
The phase file is: .cypilot/.plans/implement-code-orm-connectors/phase-03-postgresql-connector.md
It is self-contained — follow its instructions exactly.
After completion, report results and generate the prompt for the next phase.
```
