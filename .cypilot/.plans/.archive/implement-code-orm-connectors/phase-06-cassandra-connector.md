```toml
[phase]
plan = "implement-code-orm-connectors"
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

## Preamble

This is a self-contained phase file. All rules, constraints, and kit content
are included below. Project files listed in the Task section must be read
at runtime. Follow the instructions exactly, run any EXECUTE commands as
written, and report results against the acceptance criteria at the end.

## What

Implement `connector_trait<CassandraDB>` as a header-only C++23 specialisation. Key
differentiator: compile-time partition key validation — a WHERE clause without a
partition key equality predicate produces a `static_assert` failure, preventing
full-cluster scans. CQL uses positional `?` parameters (same as MySQL; indexed
placeholder reuse duplicates bind values). `supports_joins` MUST NOT be declared.
A `MockCassSession` stubs the DataStax C++ Driver surface inline.

## Prior Context

- Phase 1 created the stub header.
- Cassandra enforces partition key constraints; missing PK in WHERE = catastrophic full-cluster scan.
- CQL parameter syntax: positional `?` (same as MySQL, unlike PostgreSQL's `$N`).
- Indexed placeholder reuse: argument duplicated at each reuse site (no native indexed reuse in CQL).
- DataStax Driver API: `cass_session_prepare` → `cass_statement_bind_*` → `cass_session_execute` → `cass_result_free`.
- Capability tags: `supports_transactions` MAY be declared; `supports_joins` MUST NOT be declared.
- Traceability: DOCS-ONLY.

## User Decisions

### Already Decided (pre-resolved during planning)
- **Traceability mode**: DOCS-ONLY
- **Mock strategy**: inline `MockCassSession` with `last_cql`, `last_params`, `result_free_count`
- **Partition key detection**: implement as a compile-time concept/requires expression checking the WHERE clause type for a partition-key equality leaf node

## Rules

### Structural
- MUST: `connector_trait<CassandraDB>` MUST satisfy `is_connector<CassandraDB>` (wire_type + cursor_type)
- MUST NOT: declare `using supports_joins`
- MUST: Compile-time partition key validation MUST produce `static_assert(false, "CassandraDB: WHERE clause must include an equality predicate on the partition key column")` when partition key absent from WHERE
- MUST: CQL rendered with positional `?` parameters; indexed placeholder reuse duplicates bind values (not native reuse)
- MUST: `cass_result_free` called exactly once per result (RAII)
- MUST: CQL injection prevented — all parameters bound via `cass_statement_bind_*`; MUST NOT concatenate user data into CQL string
- MUST: Code implements all DoD items from FEATURE-cassandra-connector.md §5
- MUST NOT: leave TODO/TBD/FIXME

### Engineering
- MUST: TDD — static_assert test for missing partition key (use a concept or SFINAE test, not a runtime test)
- MUST: RAII for `CassResult*` — `MockCassResultRAII` struct, destructor increments `result_free_count`
- MUST: KISS — CQL rendering reuses the `?`-positional pattern from MySQL phase; only adds partition-key validation step
- MUST NOT: use raw new/delete

### Quality
- MUST: Tests cover: (a) `is_connector<CassandraDB>`; (b) partition key present → compiles + correct CQL; (c) `cass_result_free` called once; (d) `supports_joins` not declared; (e) CQL static_assert message check (compile-time)

## Input

### DoD items from FEATURE-cassandra-connector.md §5:

**DoD: connector_trait<CassandraDB> Specialisation** — C++23, satisfies ConnectorTrait.
Implements: `cpt-orm-flow-cassandra-connector-execute-query`, `cpt-orm-algo-cassandra-connector-render-cql`

**DoD: Partition Key static_assert** — `static_assert` compile error when WHERE clause omits equality predicate on partition key; message contains "partition key".

**DoD: CQL Rendering** — syntactically valid CQL with `?` positional parameters for SELECT/FROM/WHERE/LIMIT.

**DoD: Capability Tags** — `supports_joins` MUST NOT be declared.

### TDD acceptance criteria from FEATURE §6:
- Compiles C++23 `-Wall -Wextra`.
- SELECT with partition key equality WHERE: compiles + executes correctly.
- SELECT without partition key: `static_assert` compile error with message containing "partition key".
- Two-predicate CQL (partition + clustering key): syntactically valid CQL.
- `cass_result_free` called exactly once per result.
- `supports_joins` reference → `static_assert` compile error.

## Task

1. **Read input files** — Read `architecture/features/FEATURE-cassandra-connector.md` (full), `lib/include/ORM/connector/capabilities.hpp`, `lib/include/ORM/connector/trait.hpp`, existing stub `lib/include/ORM/db/connectors/CassandraDB/cassandra_db.hpp`.

2. **Implement `cassandra_db.hpp`**:
   - Define `struct MockCassResultRAII` with `mutable int result_free_count = 0`. Destructor increments counter.
   - Define `struct MockCassSession` with `last_cql` (`std::string`), `last_params` (`std::vector<std::string>`), `result_free_count`. Stub `prepare(cql)`, `bind(idx, val)`, `execute()`, `free_result()`.
   - Define `struct CassandraDB` tag type. Must have a nested `using partition_key_column = void;` or a template parameter to designate the partition key column for compile-time validation.
   - `namespace cass_detail {}`:
     - `has_pk_equality<WhereClause, PKColumn>()` — compile-time check
     - `pk_assert<WhereClause, PKColumn>()` — `static_assert(has_pk_equality<...>(), "CassandraDB: WHERE clause must include an equality predicate on the partition key column")`
     - `render_cql_columns`, `render_cql_wheres` (emit `?` for each placeholder; duplicate indexed placeholder values)
   - `connector_trait<CassandraDB>` in `namespace orm {}`:
     - NO `supports_joins`; MAY declare `supports_transactions = void`
     - `wire_type`, `cursor_type`
     - `execute()` overloads: call `cass_detail::pk_assert` first, then render CQL, store in `conn.last_cql` + `conn.last_params`, return empty result

3. **Write unit tests** — Create `tests/unit/test_cassandra_connector.cpp`:
   - `TEST(CassandraConnector, SatisfiesIsConnector)` — `static_assert`
   - `TEST(CassandraConnector, CqlWithPartitionKey)` — SELECT with PK equality WHERE → `last_cql` contains `?`
   - `TEST(CassandraConnector, ResultFreeCalledOnce)` — `result_free_count == 1`
   - `TEST(CassandraConnector, NoJoinsCapability)` — `static_assert(!has_capability<CassandraDB, cap::supports_joins>)`

4. **Update `tests/unit/CMakeLists.txt`** — Add `test_cassandra_connector.cpp`.

5. **Write `out/phase-06-cassandra-summary.md`**.

6. **Self-verify**.

## Acceptance Criteria

- [ ] `cassandra_db.hpp` contains full `connector_trait<CassandraDB>` with compile-time partition key validation
- [ ] `is_connector<CassandraDB>` satisfied
- [ ] Partition key static_assert message contains "partition key"
- [ ] `supports_joins` NOT declared
- [ ] CQL rendering test present with `?` positional params
- [ ] `result_free_count` RAII test present
- [ ] `test_cassandra_connector.cpp` with ≥ 3 TEST() cases; CMakeLists updated
- [ ] No TODO/TBD/FIXME; no unresolved `{variable}` outside code fences
- [ ] `out/phase-06-cassandra-summary.md` exists

## Output Format

```text
PHASE 6/12 COMPLETE
Status: PASS | FAIL
Files created/modified: {list}
Acceptance criteria: {checklist}
Notes: {any issues}
```

```text
I have a Cypilot execution plan at:
  .cypilot/.plans/implement-code-orm-connectors/plan.toml

Phase 6 is complete (PASS).
Please read the plan manifest, then execute Phase 7: "Neo4j Connector — connector_trait<Neo4jDB> + tests".
The phase file is: .cypilot/.plans/implement-code-orm-connectors/phase-07-neo4j-connector.md
It is self-contained — follow its instructions exactly.
After completion, report results and generate the prompt for the next phase.
```
