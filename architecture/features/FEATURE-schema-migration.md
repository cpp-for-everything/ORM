# Feature: Schema Migration & Validation

- [ ] `p1` - **ID**: `cpt-orm-featstatus-schema-migration`

- [ ] `p2` - **ID**: `cpt-orm-feature-schema-migration`

<!-- toc -->

- [1. Feature Context](#1-feature-context)
  - [1.1 Overview](#11-overview)
  - [1.2 Purpose](#12-purpose)
  - [1.3 Actors](#13-actors)
  - [1.4 References](#14-references)
  - [1.5 Scope & Boundaries](#15-scope--boundaries)
  - [1.6 Configuration](#16-configuration)
  - [Non-Applicability Declarations](#non-applicability-declarations)
- [2. Actor Flows (CDSL)](#2-actor-flows-cdsl)
  - [Run Schema Migration Against a Live Database](#run-schema-migration-against-a-live-database)
  - [Detect Schema Drift in CI Pipeline](#detect-schema-drift-in-ci-pipeline)
- [3. Processes / Business Logic (CDSL)](#3-processes--business-logic-cdsl)
  - [Entity-to-DDL Diff](#entity-to-ddl-diff)
  - [Compile-Time Column Reference Safety Check](#compile-time-column-reference-safety-check)
  - [DDL Generation](#ddl-generation)
- [4. States](#4-states)
  - [Migration Run State Machine](#migration-run-state-machine)
- [5. Definitions of Done](#5-definitions-of-done)
  - [Diff Computation](#diff-computation)
  - [DDL Generation Correctness](#ddl-generation-correctness)
  - [Dry-Run Mode](#dry-run-mode)
  - [Compile-Time Column Reference Safety](#compile-time-column-reference-safety)
  - [Connector DDL Trait Specialisations](#connector-ddl-trait-specialisations)
  - [Test Coverage Guidance](#test-coverage-guidance)
- [6. Acceptance Criteria](#6-acceptance-criteria)

<!-- /toc -->

## 1. Feature Context

### 1.1 Overview

`orm::migrate<DB>` tool that compares C++ entity declarations against the live database schema, generates the `ALTER TABLE` / `CREATE TABLE` DDL statements required to bring the database into alignment, and provides compile-time guarantees that no column referenced by a compiled query has been silently deleted or renamed without a corresponding entity struct update.

### 1.2 Purpose

Schema drift — the divergence between the C++ entity model and the live database schema — is a persistent source of runtime failures. Because the ORM encodes schema in C++ types, `orm::migrate<DB>` can introspect the live schema at runtime and diff it against the compile-time entity declarations to produce the exact DDL needed. The compile-time column reference check catches the complementary problem: if an entity field is removed from C++ but still referenced in a query IR, the build fails before the code reaches the database.

**Performance**: Schema introspection queries `INFORMATION_SCHEMA` at runtime; this is a one-time administrative operation, not a hot path. No hot-path or N+1 concerns apply.

**Reliability**: Connection failure during introspection aborts migration with no DDL applied. DDL execution failure stops at the failing statement and reports it. No retry logic is implemented; migration is a deliberate manual or CI action.

**Security**: Migration executes DDL against the live database using the caller-supplied connection; the caller is responsible for ensuring the connection has the necessary DDL privileges. No credentials are stored by this feature.

**Requirements**: DESIGN pending — no PRD or DESIGN artifact registered yet.

**Principles**: DESIGN pending.

### 1.3 Actors

| Actor | Role in Feature |
|-------|-----------------|
| Developer | Runs `orm::migrate<DB>` interactively to review and apply DDL changes. |
| CI Pipeline | Runs `orm::migrate<DB>` in dry-run mode to detect and fail on schema drift. |

### 1.4 References

- **PRD**: DESIGN pending — no PRD registered.
- **Design**: DESIGN pending — no DESIGN artifact registered.
- **Dependencies**: connector's schema introspection API (e.g. `INFORMATION_SCHEMA` for SQL databases); `connector_trait<DB>::ddl_for(op)` extension point.

### 1.5 Scope & Boundaries

**In scope**:
- Runtime diff computation between C++ entity declarations and live database schema.
- DDL generation for CREATE TABLE, ADD COLUMN, DROP COLUMN, ALTER COLUMN TYPE operations via `connector_trait<DB>::ddl_for(op)`.
- Dry-run mode with exit code semantics (0 = in sync, 2 = drift detected) for CI integration.
- Compile-time column reference safety check: build failure when a query IR references a non-existent entity field.
- `connector_trait<DB>::ddl_for(op)` specialisations for `SQLiteDB` and `MockDB`.

**Out of scope**:
- Automatic migration rollback or migration versioning (no migration history table).
- Schema migration for NoSQL connectors (MongoDB is schema-free; Redis, Neo4j, Cassandra use non-DDL schema models).
- Data migration (column renames, value transforms).
- Multi-database or distributed DDL transactions.

### 1.6 Configuration

Not applicable — dry-run mode is a parameter passed at call time, not a persistent configuration option. All migration behaviour is determined at runtime from the live schema and the registered entity list.

### Non-Applicability Declarations

- **UX**: Not applicable — migration output is printed to stdout; no interactive UI is required beyond stdout + developer approval prompt.
- **COMPL (Compliance)**: Not applicable — no regulatory or standards-compliance requirements apply to this feature at the ORM layer. Data privacy (PII/GDPR): Not applicable at the connector layer — the connector transmits only what the caller supplies; PII handling is the application's responsibility.
- **OPS (Observability)**: Not applicable — migration is an administrative operation; no ongoing logging or metrics collection is required.
- **OPS (Rollout/Rollback)**: Not applicable — this is a C++ library feature distributed as source; rollout and rollback are governed by the consuming application's dependency management.
- **SEC (Authentication/Authorization)**: Partially applicable — the caller supplies a connection with DDL privileges; this feature does not manage credentials but the caller must ensure the connection is appropriately authorized before invoking migration.
- **PERF (Hot paths / N+1)**: Not applicable — migration is a one-time administrative operation, not a hot path.
- **REL (Retry / circuit breaker)**: Not applicable — migration aborts on connection failure or DDL error; retrying a failed migration requires manual intervention.

---

## 2. Actor Flows (CDSL)

### Run Schema Migration Against a Live Database

- [ ] `p1` - **ID**: `cpt-orm-flow-schema-migration-run-migration`

**Actor**: Developer

**Success Scenarios**:
- Schema is already in sync: `orm::migrate<DB>` reports no changes needed and exits cleanly.
- Schema drift detected: developer reviews the generated DDL, approves, DDL is applied, schema is now in sync.

**Error Scenarios**:
- Connection failure during introspection: migration exits with an error, no DDL is generated or applied.
- DDL execution fails (e.g. incompatible type change): migration reports the failing statement and stops.

**Steps**:
1. [ ] - `p1` - Developer instantiates `orm::migrate<DB>` with a live database connection and the registered entity list. - `inst-migrate-instantiate`
2. [ ] - `p1` - Migration tool connects to the database and introspects the live schema (e.g. queries `INFORMATION_SCHEMA.COLUMNS`). - `inst-migrate-introspect`
3. [ ] - `p1` - **IF** connection or introspection fails — **RETURN** error; no DDL generated. - `inst-migrate-conn-error`
4. [ ] - `p1` - Migration tool invokes entity-to-DDL diff to compute the ordered list of required schema changes. - `inst-migrate-diff`
5. [ ] - `p1` - **IF** diff is empty — report "schema in sync", **RETURN** success. - `inst-migrate-in-sync`
6. [ ] - `p1` - Migration tool invokes DDL generation to produce the SQL DDL string. - `inst-migrate-gen-ddl`
7. [ ] - `p1` - Migration tool presents the DDL to the developer for review (prints to stdout). - `inst-migrate-review`
8. [ ] - `p1` - **IF** developer approves — execute each DDL statement against the database. - `inst-migrate-execute-ddl`
9. [ ] - `p1` - **IF** a DDL statement fails — report the failing statement and stop execution. - `inst-migrate-ddl-error`
10. [ ] - `p1` - **RETURN** success after all DDL statements execute without error. - `inst-migrate-return`

### Detect Schema Drift in CI Pipeline

- [ ] `p1` - **ID**: `cpt-orm-flow-schema-migration-detect-drift`

**Actor**: CI Pipeline

**Success Scenarios**:
- No drift: migration dry-run produces empty diff, exits with code 0.

**Error Scenarios**:
- Drift detected: migration dry-run produces non-empty diff, exits with code 2, CI step fails.

**Steps**:
1. [ ] - `p1` - CI invokes `orm::migrate<DB>` with `--dry-run` flag (or equivalent API argument). - `inst-ci-dry-run`
2. [ ] - `p1` - Migration tool connects, introspects schema, and computes diff — identical to the interactive flow up to step 5. - `inst-ci-diff`
3. [ ] - `p1` - **IF** diff is non-empty — print the diff to stdout and **RETURN** exit code 2 (schema drift detected). - `inst-ci-drift-fail`
4. [ ] - `p1` - **IF** diff is empty — **RETURN** exit code 0 (schema in sync). - `inst-ci-drift-pass`

---

## 3. Processes / Business Logic (CDSL)

### Entity-to-DDL Diff

- [ ] `p2` - **ID**: `cpt-orm-algo-schema-migration-diff`

**Input**: Compiled entity type list (from ORM registration); live schema (fetched from database via introspection API).

**Output**: Ordered list of DDL operations: CREATE TABLE, ADD COLUMN, DROP COLUMN, ALTER COLUMN TYPE.

**Steps**:
1. [ ] - `p1` - Build a map of live tables: `table_name → {column_name → column_type}`. - `inst-diff-build-live-map`
2. [ ] - `p1` - **FOR EACH** registered entity type — obtain the entity's table name and column list from the ORM type system. - `inst-diff-iter-entities`
3. [ ] - `p1` - **IF** the entity's table is absent from the live schema — emit a CREATE TABLE operation covering all columns. - `inst-diff-create-table`
4. [ ] - `p1` - **FOR EACH** column in the entity that is absent from the live table — emit an ADD COLUMN operation. - `inst-diff-add-col`
5. [ ] - `p1` - **FOR EACH** column in the live table that is absent from the entity — emit a DROP COLUMN operation. - `inst-diff-drop-col`
6. [ ] - `p1` - **FOR EACH** column present in both — **IF** the mapped DB type differs — emit an ALTER COLUMN TYPE operation. - `inst-diff-alter-type`
7. [ ] - `p1` - **RETURN** the ordered list of DDL operations. - `inst-diff-return`

### Compile-Time Column Reference Safety Check

- [ ] `p2` - **ID**: `cpt-orm-algo-schema-migration-ref-safety`

**Input**: Query IR referencing specific entity columns at compile time; entity struct declarations.

**Output**: Compile success, or compile error if a referenced column is absent from the entity struct.

**Steps**:
1. [ ] - `p1` - At compile time, for each column reference in the query IR — resolve the referenced field via the entity type's property list. - `inst-safety-resolve-ref`
2. [ ] - `p1` - **IF** the referenced field is absent from the entity struct — emit a compile error: the field does not exist in the entity, ensuring no stale column reference reaches runtime. - `inst-safety-assert`
3. [ ] - `p1` - **RETURN** compile success when all column references resolve correctly. - `inst-safety-return`

### DDL Generation

- [ ] `p2` - **ID**: `cpt-orm-algo-schema-migration-generate-ddl`

**Input**: Ordered list of DDL operations from the diff; `connector_trait<DB>` DDL dialect.

**Output**: DDL SQL string ready for execution.

**Steps**:
1. [ ] - `p1` - **FOR EACH** DDL operation in the list — delegate to `connector_trait<DB>::ddl_for(op)` to produce the dialect-specific SQL fragment. - `inst-ddl-delegate`
2. [ ] - `p1` - Concatenate all fragments, separated by `;\n`. - `inst-ddl-concat`
3. [ ] - `p1` - **RETURN** the complete DDL string. - `inst-ddl-return`

---

## 4. States

### Migration Run State Machine

- [ ] `p2` - **ID**: `cpt-orm-state-schema-migration-run`

**States**: `Idle`, `Connected`, `Diffing`, `Reviewing`, `Executing`, `Done`, `Failed`

**Initial State**: `Idle`

**Transitions**:
1. [ ] - `p1` - **FROM** `Idle` **TO** `Connected` **WHEN** database connection and schema introspection succeed. - `inst-state-idle-connected`
2. [ ] - `p1` - **FROM** `Connected` **TO** `Diffing` **WHEN** schema introspection data is ready for diffing. - `inst-state-connected-diffing`
3. [ ] - `p1` - **FROM** `Diffing` **TO** `Reviewing` **WHEN** diff is non-empty and running in interactive mode. - `inst-state-diffing-reviewing`
4. [ ] - `p1` - **FROM** `Diffing` **TO** `Done` **WHEN** diff is empty (schema in sync) or dry-run mode. - `inst-state-diffing-done`
5. [ ] - `p1` - **FROM** `Reviewing` **TO** `Executing` **WHEN** developer approves the DDL. - `inst-state-reviewing-executing`
6. [ ] - `p1` - **FROM** `Reviewing` **TO** `Done` **WHEN** developer cancels migration. - `inst-state-reviewing-done`
7. [ ] - `p1` - **FROM** `Executing` **TO** `Done` **WHEN** all DDL statements execute without error. - `inst-state-executing-done`
8. [ ] - `p1` - **FROM** `Executing` **TO** `Failed` **WHEN** a DDL statement returns an error. - `inst-state-executing-failed`
9. [ ] - `p1` - **FROM** `Connected` **TO** `Failed` **WHEN** connection error occurs during introspection. - `inst-state-connected-failed`

---

## 5. Definitions of Done

### Diff Computation

- [ ] `p1` - **ID**: `cpt-orm-dod-schema-migration-diff-compute`

The system **MUST** correctly identify CREATE TABLE, ADD COLUMN, and DROP COLUMN operations by comparing the registered C++ entity type list against the live database schema, producing an ordered, deterministic list of DDL operations.

**Implements**:
- `cpt-orm-algo-schema-migration-diff`

**Touches**:
- Entities: `orm::migrate<DB>` diff engine
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### DDL Generation Correctness

- [ ] `p1` - **ID**: `cpt-orm-dod-schema-migration-ddl-generate`

The system **MUST** generate syntactically valid DDL SQL for the SQLite and MockDB connectors via `connector_trait<DB>::ddl_for(op)`, with each generated statement executable against the target database without syntax errors.

**Implements**:
- `cpt-orm-algo-schema-migration-generate-ddl`

**Touches**:
- Entities: `connector_trait<SQLiteDB>::ddl_for`, `connector_trait<MockDB>::ddl_for`
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Dry-Run Mode

- [ ] `p1` - **ID**: `cpt-orm-dod-schema-migration-dry-run`

The system **MUST** provide a dry-run mode that outputs the generated DDL to stdout without executing it, and exits with code 0 when the diff is empty and code 2 when the diff is non-empty, enabling CI schema drift detection.

**Implements**:
- `cpt-orm-flow-schema-migration-detect-drift`

**Touches**:
- Entities: `orm::migrate<DB>` dry-run path
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Compile-Time Column Reference Safety

- [ ] `p1` - **ID**: `cpt-orm-dod-schema-migration-compile-time-safety`

The system **MUST** produce a compile error when a query IR references a column that does not exist in the corresponding entity struct's property list, preventing stale column references from reaching runtime.

**Implements**:
- `cpt-orm-algo-schema-migration-ref-safety`

**Touches**:
- Entities: ORM query IR column resolution
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Connector DDL Trait Specialisations

- [ ] `p1` - **ID**: `cpt-orm-dod-schema-migration-connector-ddl-trait`

The system **MUST** provide `connector_trait<DB>::ddl_for(op)` specialisations for at minimum `SQLiteDB` and `MockDB`, covering CREATE TABLE, ADD COLUMN, DROP COLUMN, and ALTER COLUMN TYPE operations.

**Implements**:
- `cpt-orm-algo-schema-migration-generate-ddl`

**Touches**:
- Entities: `connector_trait<SQLiteDB>`, `connector_trait<MockDB>`
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Test Coverage Guidance

**Unit test targets**:
- Diff engine: given a MockDB with a known live schema map and a set of registered entities, verify that CREATE TABLE, ADD COLUMN, DROP COLUMN, and ALTER COLUMN TYPE operations are generated correctly.
- DDL generation: verify `connector_trait<SQLiteDB>::ddl_for(CREATE_TABLE)` and `ddl_for(ADD_COLUMN)` produce syntactically valid SQLite DDL strings.
- Dry-run mode: verify exit code 0 when diff is empty and exit code 2 when diff is non-empty.
- Compile-time column reference safety: verify `static_assert` fires when a query IR references a column absent from the entity struct.

**Integration test targets**:
- Full round-trip against a live SQLite database: create a table, add a column, drop a column, verify generated DDL executes without errors.
- CI dry-run: verify `orm::migrate<SQLiteDB>` exits with code 2 when the live schema diverges from the entity declarations.

**Mock boundaries**: The live schema introspection API (e.g. `INFORMATION_SCHEMA` queries) is the primary mock boundary; use an in-memory SQLite database for integration tests.

**Test isolation**: Unit tests use `MockDB` and a pre-populated mock schema map; no live database required.

---

## 6. Acceptance Criteria

- [ ] `orm::migrate<SQLiteDB>` correctly identifies a missing table and generates a valid `CREATE TABLE` statement that executes against SQLite without errors.
- [ ] `orm::migrate<SQLiteDB>` correctly identifies a missing column and generates a valid `ALTER TABLE ... ADD COLUMN` statement.
- [ ] Dry-run mode exits with code 2 when drift is detected and code 0 when schema is in sync.
- [ ] A query IR referencing a column absent from the entity struct produces a compile error containing the column name.
- [ ] The migration state machine transitions from `Executing` to `Failed` (not `Done`) when a DDL statement returns an error.
