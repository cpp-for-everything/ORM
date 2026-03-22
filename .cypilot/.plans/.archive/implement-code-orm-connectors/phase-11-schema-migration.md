```toml
[phase]
plan = "implement-code-orm-connectors"
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

## Preamble

This is a self-contained phase file. All rules, constraints, and kit content
are included below. Project files listed in the Task section must be read
at runtime. Follow the instructions exactly, run any EXECUTE commands as
written, and report results against the acceptance criteria at the end.

## What

Implement `orm::migrate<DB>` in `lib/include/ORM/db/migration/migration.hpp` — a header-only
C++23 template that compares a registered C++ entity list against a live database schema,
produces an ordered list of DDL operations (CREATE TABLE, ADD COLUMN, DROP COLUMN, ALTER
COLUMN TYPE), generates dialect-specific DDL SQL via `connector_trait<DB>::ddl_for(op)`,
and supports dry-run mode (exit code 0 = in sync, 2 = drift detected). Also implement
compile-time column reference safety: a query IR referencing a non-existent entity field
produces a compile error. Add `ddl_for(op)` specialisations to `connector_trait<MockDB>`.
Write unit tests in `tests/unit/test_schema_migration.cpp`.

## Prior Context

- Phase 1 created the stub `lib/include/ORM/db/migration/migration.hpp`.
- Phases 2–3 (MySQL/PostgreSQL) established connector patterns — migration extends the connector trait.
- `orm::migrate<DB>` is an administrative tool, not a hot path: runtime schema introspection + DDL generation.
- State machine: `Idle → Connected → Diffing → Reviewing → Executing → Done/Failed`.
- `connector_trait<DB>::ddl_for(op)` is a new extension point returning `std::string`.
- Compile-time column safety: query IR references entity property → static concept check at compile time.
- Traceability: DOCS-ONLY.

## User Decisions

### Already Decided (pre-resolved during planning)
- **Traceability mode**: DOCS-ONLY
- **Live schema mock**: `orm::live_schema` is a `std::unordered_map<std::string, std::unordered_map<std::string, std::string>>` (table → column → type); populated manually in tests
- **Interactive approval**: skipped in all unit tests; `run(dry_run=true)` used for test coverage
- **DDL specialisations**: add `ddl_for(op)` to `connector_trait<MockDB>` (SQLiteDB not yet built so MockDB only)

## Rules

### Structural
- MUST: `orm::ddl_op` MUST be a `std::variant` of `create_table_op`, `add_column_op`, `drop_column_op`, `alter_column_type_op`
- MUST: `orm::migrate<DB>::diff(live_schema, entities)` MUST return `std::vector<orm::ddl_op>` in deterministic order (CREATE TABLE first, then ADD/DROP COLUMN, then ALTER TYPE)
- MUST: `orm::migrate<DB>::generate_ddl(ops)` MUST delegate to `connector_trait<DB>::ddl_for(op)` per operation
- MUST: `orm::migrate<DB>::run(dry_run=true)` MUST return `0` when diff is empty; MUST return `2` when diff is non-empty; MUST NOT execute DDL when `dry_run=true`
- MUST: `connector_trait<MockDB>::ddl_for(create_table_op)` MUST return `"CREATE TABLE ..."` string
- MUST: `connector_trait<MockDB>::ddl_for(add_column_op)` MUST return `"ALTER TABLE ... ADD COLUMN ..."` string
- MUST: `connector_trait<MockDB>::ddl_for(drop_column_op)` MUST return `"ALTER TABLE ... DROP COLUMN ..."` string
- MUST: Compile-time column reference safety: `requires` clause on query IR column references ensuring the field exists in the entity's property list; `static_assert` with descriptive message when absent
- MUST: Migration state machine MUST track `Idle → Connected → Diffing → Done/Failed` transitions
- MUST: Code implements all DoD items from FEATURE-schema-migration.md §5
- MUST NOT: leave TODO/TBD/FIXME

### Engineering
- MUST: TDD — diff engine tests, DDL generation tests, dry-run exit code tests, compile-time safety test
- MUST: SOLID / Single Responsibility — diff(), generate_ddl(), run() are distinct methods
- MUST: KISS — `live_schema` is a `std::unordered_map`; no XML/JSON parsing
- MUST NOT: use raw new/delete

### Quality
- MUST: Tests cover: (a) diff produces CREATE TABLE for missing table; (b) diff produces ADD COLUMN for missing column; (c) diff produces DROP COLUMN for extra column; (d) dry-run returns 0 when in sync, 2 when drift; (e) compile-time column safety static_assert fires for absent field

## Input

### DoD items from FEATURE-schema-migration.md §5:

**DoD: Diff Computation** — correctly identifies CREATE TABLE, ADD COLUMN, DROP COLUMN by comparing registered entities vs live schema; ordered, deterministic list.
Implements: `cpt-orm-algo-schema-migration-diff`

**DoD: DDL Generation Correctness** — syntactically valid DDL via `connector_trait<DB>::ddl_for(op)` for MockDB (CREATE TABLE, ADD COLUMN, DROP COLUMN, ALTER COLUMN TYPE).
Implements: `cpt-orm-algo-schema-migration-generate-ddl`

**DoD: Dry-Run Mode** — exit code 0 when diff empty; exit code 2 when diff non-empty; no DDL executed in dry-run.
Implements: `cpt-orm-flow-schema-migration-detect-drift`

**DoD: Compile-Time Column Reference Safety** — compile error when query IR references absent entity column.
Implements: `cpt-orm-algo-schema-migration-ref-safety`

**DoD: Connector DDL Trait Specialisations** — `connector_trait<MockDB>::ddl_for(op)` for CREATE TABLE, ADD COLUMN, DROP COLUMN, ALTER COLUMN TYPE.
Implements: `cpt-orm-algo-schema-migration-generate-ddl`

### TDD acceptance criteria from FEATURE §6:
- `orm::migrate<MockDB>` identifies missing table → valid `CREATE TABLE` DDL.
- Missing column → valid `ALTER TABLE ... ADD COLUMN` DDL.
- Dry-run exits with code 2 when drift detected; code 0 when in sync.
- Query IR referencing absent column → compile error containing column name.
- State machine transitions from `Executing` to `Failed` (not `Done`) when DDL fails.

## Task

1. **Read input files** — Read `architecture/features/FEATURE-schema-migration.md` (full), `lib/include/ORM/connector/capabilities.hpp`, `lib/include/ORM/connector/db.hpp`, `lib/include/ORM/db/connectors/MockDB/mock_db.hpp` (lines 244–378 for trait specialisation pattern).

2. **Implement `migration.hpp`** — Replace the stub with full implementation:
   - **DDL op types**:
     ```cpp
     struct create_table_op { std::string table; std::vector<std::pair<std::string,std::string>> columns; };
     struct add_column_op   { std::string table; std::string column; std::string type; };
     struct drop_column_op  { std::string table; std::string column; };
     struct alter_column_type_op { std::string table; std::string column; std::string new_type; };
     using ddl_op = std::variant<create_table_op, add_column_op, drop_column_op, alter_column_type_op>;
     ```
   - **`live_schema` type**: `using live_schema = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;`
   - **`entity_meta`**: `struct entity_meta { std::string table_name; std::vector<std::pair<std::string,std::string>> columns; };`
   - **`migration_state`**: `enum class migration_state { Idle, Connected, Diffing, Reviewing, Executing, Done, Failed };`
   - **`orm::migrate<DB>` class**:
     - `std::vector<ddl_op> diff(const live_schema& live, std::span<const entity_meta> entities)` — for each entity: if table absent → CREATE TABLE; for each column: if absent in live → ADD COLUMN; for each live column: if absent in entity → DROP COLUMN; for each common column: if type differs → ALTER
     - `std::string generate_ddl(const std::vector<ddl_op>& ops)` — delegate each op to `connector_trait<DB>::ddl_for(op)`, join with `";\n"`
     - `int run(const live_schema& live, std::span<const entity_meta> entities, bool dry_run)` — computes diff; if empty return 0; if dry_run return 2; else execute DDL and return 0/2

3. **Add `ddl_for` to `connector_trait<MockDB>`** in `mock_db.hpp`:
   ```cpp
   static std::string ddl_for(const create_table_op& op) { return "CREATE TABLE " + op.table + " (...)"; }
   static std::string ddl_for(const add_column_op& op) { return "ALTER TABLE " + op.table + " ADD COLUMN " + op.column + " " + op.type; }
   static std::string ddl_for(const drop_column_op& op) { return "ALTER TABLE " + op.table + " DROP COLUMN " + op.column; }
   static std::string ddl_for(const alter_column_type_op& op) { return "ALTER TABLE " + op.table + " ALTER COLUMN " + op.column + " TYPE " + op.new_type; }
   ```

4. **Write unit tests** — Create `tests/unit/test_schema_migration.cpp`:
   - `TEST(SchemaMigration, DiffCreateTable)` — live_schema empty; one entity → diff contains `create_table_op`
   - `TEST(SchemaMigration, DiffAddColumn)` — live table missing one column → diff contains `add_column_op`
   - `TEST(SchemaMigration, DiffDropColumn)` — live table has extra column → diff contains `drop_column_op`
   - `TEST(SchemaMigration, DryRunDriftExitCode2)` — drift detected + dry_run=true → returns 2
   - `TEST(SchemaMigration, DryRunInSyncExitCode0)` — no drift + dry_run=true → returns 0
   - `TEST(SchemaMigration, DdlGenerationCreateTable)` — `generate_ddl` on create_table op → `last_sql` contains "CREATE TABLE"

5. **Update `tests/unit/CMakeLists.txt`** — Add `test_schema_migration.cpp`.

6. **Write `out/phase-11-migration-summary.md`**.

7. **Self-verify**.

## Acceptance Criteria

- [ ] `migration.hpp` contains `orm::ddl_op` variant, `orm::migrate<DB>` with `diff()`, `generate_ddl()`, `run()`
- [ ] `connector_trait<MockDB>` extended with `ddl_for()` overloads for all 4 DDL op types
- [ ] Diff CREATE TABLE test present
- [ ] Diff ADD COLUMN test present
- [ ] Diff DROP COLUMN test present
- [ ] Dry-run exit code 0/2 tests present
- [ ] DDL generation test present
- [ ] `test_schema_migration.cpp` with ≥ 5 TEST() cases; CMakeLists updated
- [ ] No TODO/TBD/FIXME; no unresolved `{variable}` outside code fences
- [ ] `out/phase-11-migration-summary.md` exists

## Output Format

```text
PHASE 11/12 COMPLETE
Status: PASS | FAIL
Files created/modified: {list}
Acceptance criteria: {checklist}
Notes: {any issues}
```

```text
I have a Cypilot execution plan at:
  .cypilot/.plans/implement-code-orm-connectors/plan.toml

Phase 11 is complete (PASS).
Please read the plan manifest, then execute Phase 12: "Archive plan files".
The phase file is: .cypilot/.plans/implement-code-orm-connectors/phase-12-archive.md
It is self-contained — follow its instructions exactly.
After completion, report results and generate the prompt for the next phase.
```
