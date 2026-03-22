```toml
[phase]
plan = "implement-code-orm-connectors"
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

## Preamble

This is a self-contained phase file. All rules, constraints, and kit content
are included below. Project files listed in the Task section must be read
at runtime. Follow the instructions exactly, run any EXECUTE commands as
written, and report results against the acceptance criteria at the end.

## What

Implement `connector_trait<PostgreSQLDB>` as a complete header-only C++23 specialisation in
`lib/include/ORM/db/connectors/PostgreSQLDB/postgresql_db.hpp`. Key differentiator from MySQL:
PostgreSQL uses `$N` numbered parameters — indexed placeholder reuse is native (no argument
duplication; the same `$N` token is emitted at each reuse site). A `MockPGconn` struct stubs
the libpq surface inline. Write unit tests in `tests/unit/test_postgresql_connector.cpp` and
update `tests/unit/CMakeLists.txt`.

## Prior Context

- Phase 1 created the stub header.
- PostgreSQL libpq API: `PQprepare` → `PQexecPrepared` → `PQclear` (RAII on `PGresult*`).
- `$N` parameter syntax: `$1`, `$2`, ... ; indexed placeholder `_1` used twice maps to same `$1` — `nparams` equals number of distinct placeholder indices.
- Result hydration: `PQntuples`, `PQnfields`, `PQgetvalue`, `PQfname` — wrap `PGresult*` in RAII.
- Capability tags: `supports_joins`, `supports_transactions`, `supports_aggregation`.
- Traceability mode: DOCS-ONLY — no `@cpt-*` markers.

## User Decisions

### Already Decided (pre-resolved during planning)
- **Traceability mode**: DOCS-ONLY
- **Mock strategy**: inline `MockPGconn` struct with last_sql, last_params, pq_clear_count fields
- **Language**: C++23; test framework: GoogleTest

## Rules

### Structural
- MUST: `connector_trait<PostgreSQLDB>` MUST satisfy `is_connector<PostgreSQLDB>` (wire_type + cursor_type)
- MUST: Declare `using supports_joins = void; using supports_transactions = void; using supports_aggregation = void;`
- MUST: Render positional `Placeholder<T>` as `$1`, `$2`, ... incrementing; render indexed `orm::ph<T, _K>` as `$M` where M is the previously assigned number for index K (no new values array entry at reuse sites)
- MUST: `nparams` passed to `PQexecPrepared` MUST equal the number of distinct placeholder indices (not total occurrences)
- MUST: `PQclear` MUST be called exactly once per `PGresult*`, including on the error path (RAII wrapper)
- MUST: SQL injection prevented — parameters passed as separate values array; MUST NOT concatenate user data into SQL string
- MUST: Implement result hydration algo (`cpt-orm-algo-postgresql-connector-hydrate-result`): `PQntuples`, `PQnfields`, `PQgetvalue` mapped to C++ entity fields via column name
- MUST: Code implements all DoD items from FEATURE-postgresql-connector.md §5
- MUST NOT: leave TODO/TBD/FIXME

### Engineering
- MUST: TDD — test assertions written alongside implementation
- MUST: RAII for `PGresult*` — define `struct PGresultRAII` wrapping pointer with destructor calling `PQclear`
- MUST: KISS — reuse the same render_columns/render_wheres pattern from MockDB; only change placeholder emission
- MUST NOT: use raw new/delete

### Quality
- MUST: Tests cover: (a) `is_connector<PostgreSQLDB>`; (b) `$N` parameter assignment for positional placeholders; (c) indexed placeholder reuse — `_1` twice → same `$1`, `nparams = 1`; (d) `PQclear` called once (via `pq_clear_count`)

## Input

### DoD items (from FEATURE-postgresql-connector.md §5):

**DoD: `connector_trait<PostgreSQLDB>` Specialisation** — MUST compile C++23, satisfy ConnectorTrait concept.

**DoD: Prepared-Statement Lifecycle via libpq** — `PQprepare` → check `PGRES_COMMAND_OK` → `PQexecPrepared` → RAII `PQclear`.

**DoD: Dollar-Parameter Rendering** — positional `Placeholder<T>` nodes rendered as `$1`, `$2`, ...; runtime args in corresponding positions of `PQexecPrepared` values array.

**DoD: Indexed Placeholder Native Reuse** — same `$N` token at every occurrence of indexed placeholder with index N; `nparams` equals number of distinct indices.

**DoD: Capability Tags** — `supports_joins`, `supports_transactions`, `supports_aggregation` declared; others not declared.

### TDD acceptance criteria from FEATURE §6:
- `connector_trait<PostgreSQLDB>` compiles C++23 `-Wall -Wextra`.
- SELECT with positional params: correct rows returned.
- `_1` used twice in WHERE: SQL has same `$1` at both positions; `nparams = 1`.
- `PQclear` called exactly once per `PGresult*`, including error path.
- Undeclared capability tag → `static_assert` compile error.

## Task

1. **Read input files** — Read `architecture/features/FEATURE-postgresql-connector.md` (full), `lib/include/ORM/connector/capabilities.hpp`, `lib/include/ORM/connector/trait.hpp`, existing stub `lib/include/ORM/db/connectors/PostgreSQLDB/postgresql_db.hpp`.

2. **Implement `postgresql_db.hpp`** — Replace stub with full implementation:
   - `struct MockPGconn` with `last_sql`, `last_params` (values array as `std::vector<std::string>`), `pq_clear_count`, `nparams_used`. Stub methods: `prepare(name, sql, nparams)`, `exec_prepared(name, nparams, params)`, `clear_result()`.
   - `struct PGresultRAII` RAII wrapper: constructor stores `MockPGconn*` to increment `pq_clear_count` on destruction.
   - `struct PostgreSQLDB` tag type.
   - `connector_trait<PostgreSQLDB>` specialisation in `namespace orm {}`:
     - Capability aliases; `wire_type`; `cursor_type`.
     - SQL rendering helpers in `namespace pg_detail {}`: `render_columns`, `render_wheres` (emit `$N` for Placeholder nodes; emit `$M` for indexed placeholders using a seen-index map), `render_joins`, `render_order_by`, `render_limits`.
     - `execute()` overloads: SELECT (no params), SELECT (with params), INSERT, UPDATE, DELETE (all storing SQL + params in `conn` fields and returning empty result).

3. **Write unit tests** — Create `tests/unit/test_postgresql_connector.cpp`:
   - `TEST(PGConnector, SatisfiesIsConnector)` — `static_assert`
   - `TEST(PGConnector, DollarParamRendering)` — SELECT with two positional params → `$1 ... $2`
   - `TEST(PGConnector, IndexedPlaceholderNativeReuse)` — `_1` twice → single `$1` in SQL, `nparams_used == 1`
   - `TEST(PGConnector, PQClearCalledOnce)` — execute, verify `pq_clear_count == 1`
   - `TEST(PGConnector, CapabilityAggregation)` — `static_assert(has_capability<PostgreSQLDB, cap::supports_aggregation>)`

4. **Update `tests/unit/CMakeLists.txt`** — Add `test_postgresql_connector.cpp` to sources.

5. **Write intermediate output** — Create `out/phase-03-postgresql-summary.md`.

6. **Self-verify** — All acceptance criteria.

## Acceptance Criteria

- [ ] `postgresql_db.hpp` contains full `connector_trait<PostgreSQLDB>` with `$N` dollar-param rendering
- [ ] `is_connector<PostgreSQLDB>` satisfied (static_assert test present)
- [ ] Indexed placeholder native reuse test present: same `$1` emitted twice, `nparams == 1`
- [ ] RAII `pq_clear_count` test present
- [ ] `supports_joins`, `supports_transactions`, `supports_aggregation` declared
- [ ] `test_postgresql_connector.cpp` exists with ≥ 4 TEST() cases
- [ ] `tests/unit/CMakeLists.txt` updated
- [ ] No TODO/TBD/FIXME; no unresolved `{variable}` outside code fences
- [ ] `out/phase-03-postgresql-summary.md` exists

## Output Format

When complete, report results in this exact format:
```text
PHASE 3/12 COMPLETE
Status: PASS | FAIL
Files created: {list}
Files modified: {list}
Acceptance criteria:
  [x] Criterion 1 — PASS
  ...
Notes: {any issues}
```

```text
Next phase prompt (copy-paste into new chat if needed):
```

```text
I have a Cypilot execution plan at:
  .cypilot/.plans/implement-code-orm-connectors/plan.toml

Phase 3 is complete (PASS).
Please read the plan manifest, then execute Phase 4: "MongoDB Connector — connector_trait<MongoDB> + tests".
The phase file is: .cypilot/.plans/implement-code-orm-connectors/phase-04-mongodb-connector.md
It is self-contained — follow its instructions exactly.
After completion, report results and generate the prompt for the next phase.
```
