```toml
[phase]
plan = "implement-code-orm-connectors"
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

## Preamble

This is a self-contained phase file. All rules, constraints, and kit content
are included below. Project files listed in the Task section must be read
at runtime. Follow the instructions exactly, run any EXECUTE commands as
written, and report results against the acceptance criteria at the end.

## What

Implement `connector_trait<Neo4jDB>` as a header-only C++23 specialisation. Neo4j uses
Cypher query language: SELECT → `MATCH (n:Label) RETURN n.field`; JOINs → additional
MATCH patterns; WHERE predicates → Cypher WHERE clause. The `.traverse<Relationship>()`
IR extension renders `(n)-[:REL]->(m:Label)` relationship patterns. Named `$p1`, `$p2`, ...
parameters are used in the Cypher string. A `MockNeo4jConnection` stubs the
`libneo4j-client` surface inline. `supports_aggregation` MUST NOT be declared unless
aggregation is implemented.

## Prior Context

- Phase 1 created the stub header.
- Cypher differs fundamentally from SQL: MATCH/RETURN instead of SELECT/FROM.
- `.traverse<Relationship>()` is a custom IR extension unique to the Neo4j connector.
- Named parameters `$p1`, `$p2`, ... emitted in order of first occurrence; parameter map stores values.
- libneo4j-client API: `neo4j_run(conn, cypher, params)` → `neo4j_result_stream_t*`; `neo4j_fetch_next` → `neo4j_result_t*`; `neo4j_close_results`.
- Known limitation (documented in FEATURE spec): Cypher syntax errors from unsupported IR constructs only detected at execute time.
- Capability tags: `supports_transactions` MAY be declared; `supports_aggregation` MUST NOT.
- Traceability: DOCS-ONLY.

## User Decisions

### Already Decided (pre-resolved during planning)
- **Traceability mode**: DOCS-ONLY
- **Mock strategy**: inline `MockNeo4jConnection` with `last_cypher`, `last_params_map`, `close_results_count`
- **traverse<R>() implementation**: add a `traverse_node<Relationship>` IR wrapper type in `neo4j_detail` namespace; `.traverse<Relationship>()` on the query builder appends this node to the IR

## Rules

### Structural
- MUST: `connector_trait<Neo4jDB>` MUST satisfy `is_connector<Neo4jDB>` (wire_type + cursor_type)
- MUST NOT: declare `using supports_aggregation` (unless implemented)
- MUST: IR-to-Cypher rendering MUST emit `MATCH (n:Label) RETURN n.fields` for SELECT
- MUST: `.traverse<Relationship>()` nodes MUST render as `(n)-[:RELATIONSHIP_LABEL]->(m:TargetLabel)` in the MATCH clause
- MUST: Named parameters `$p1`, `$p2`, ... assigned in order of first occurrence; stored in parameter map
- MUST: `neo4j_close_results` called exactly once per result stream (RAII)
- MUST: Code implements all DoD items from FEATURE-neo4j-connector.md §5
- MUST NOT: leave TODO/TBD/FIXME

### Engineering
- MUST: TDD — tests for MATCH/RETURN structure and traverse rendering
- MUST: RAII for `neo4j_result_stream_t*` — `MockResultStreamRAII`, destructor increments `close_results_count`
- MUST: KISS — parameter map as `std::unordered_map<std::string, std::string>` (`last_params_map`)
- MUST NOT: use raw new/delete

### Quality
- MUST: Tests cover: (a) `is_connector<Neo4jDB>`; (b) simple SELECT → MATCH/RETURN Cypher; (c) `.traverse<R>()` → relationship pattern in MATCH; (d) named params `$p1`, `$p2`; (e) `neo4j_close_results` called once

## Input

### DoD items from FEATURE-neo4j-connector.md §5:

**DoD: connector_trait<Neo4jDB> Specialisation** — C++23, satisfies ConnectorTrait.
Implements: `cpt-orm-flow-neo4j-connector-execute-traversal`, `cpt-orm-algo-neo4j-connector-render-cypher`

**DoD: Cypher Rendering — SELECT and JOIN Translation** — SELECT cols → `RETURN` clause; JOIN nodes → additional `MATCH` patterns; WHERE predicates → Cypher `WHERE` clause with named `$param` parameters.

**DoD: .traverse<Relationship>() Extension** — compile + correctly render as `(n)-[:RELATIONSHIP_LABEL]->(m:TargetLabel)` in MATCH clause.

**DoD: Bolt Protocol Dispatch** — `neo4j_run` API; RAII `neo4j_result_stream_t*` lifecycle.

**DoD: Capability Tags** — `supports_transactions` MAY be declared; `supports_aggregation` MUST NOT.

### TDD acceptance criteria from FEATURE §6:
- Compiles C++23 `-Wall -Wextra`.
- SELECT without `.traverse<R>()` → Cypher of form `MATCH (n:Label) RETURN n.field WHERE ...`
- Query with `.traverse<User, Friend>()` → MATCH pattern contains `(n)-[:Friend]->(m:User)`
- Runtime parameters encoded as `$p1`, `$p2`, ...
- `neo4j_close_results` called exactly once per result stream.

## Task

1. **Read input files** — Read `architecture/features/FEATURE-neo4j-connector.md` (full), `lib/include/ORM/connector/capabilities.hpp`, `lib/include/ORM/connector/trait.hpp`, existing stub `lib/include/ORM/db/connectors/Neo4jDB/neo4j_db.hpp`.

2. **Implement `neo4j_db.hpp`**:
   - Define `struct MockResultStreamRAII` with `mutable int close_results_count = 0`. Destructor increments counter.
   - Define `struct MockNeo4jConnection` with `last_cypher` (`std::string`), `last_params_map` (`std::unordered_map<std::string,std::string>`), `close_results_count`. Stub `run(cypher, params)`.
   - Define `struct Neo4jDB` tag type.
   - `namespace neo4j_detail {}`:
     - `render_cypher_match(entity_label, traverse_nodes)` — emits MATCH clause
     - `render_cypher_return(columns)` — emits RETURN clause
     - `render_cypher_where(wheres, param_counter)` — emits WHERE clause with `$pN` params
     - `render_cypher_order_by`, `render_cypher_limit`
   - `connector_trait<Neo4jDB>` in `namespace orm {}`:
     - MAY declare `using supports_transactions = void`; NO `supports_aggregation`
     - `wire_type`, `cursor_type`
     - `execute()` overloads for SELECT (no params), SELECT (with params) — build Cypher string + param map, store in `conn`, return empty result

3. **Write unit tests** — Create `tests/unit/test_neo4j_connector.cpp`:
   - `TEST(Neo4jConnector, SatisfiesIsConnector)` — `static_assert`
   - `TEST(Neo4jConnector, SimpleCypher)` — SELECT without traverse → `last_cypher` contains "MATCH" and "RETURN"
   - `TEST(Neo4jConnector, TraverseRelationship)` — query with traverse → `last_cypher` contains "]->[" or "]->"
   - `TEST(Neo4jConnector, NamedParams)` — SELECT with one param → `last_params_map` contains `"p1"` key
   - `TEST(Neo4jConnector, CloseResultsCalledOnce)` — `close_results_count == 1`

4. **Update `tests/unit/CMakeLists.txt`** — Add `test_neo4j_connector.cpp`.

5. **Write `out/phase-07-neo4j-summary.md`**.

6. **Self-verify**.

## Acceptance Criteria

- [ ] `neo4j_db.hpp` contains full `connector_trait<Neo4jDB>` with Cypher rendering
- [ ] `is_connector<Neo4jDB>` satisfied
- [ ] `supports_aggregation` NOT declared
- [ ] Simple SELECT → MATCH/RETURN Cypher test present
- [ ] Traverse → relationship pattern test present
- [ ] Named params `$p1` test present
- [ ] `close_results_count` RAII test present
- [ ] `test_neo4j_connector.cpp` with ≥ 4 TEST() cases; CMakeLists updated
- [ ] No TODO/TBD/FIXME; no unresolved `{variable}` outside code fences
- [ ] `out/phase-07-neo4j-summary.md` exists

## Output Format

```text
PHASE 7/12 COMPLETE
Status: PASS | FAIL
Files created/modified: {list}
Acceptance criteria: {checklist}
Notes: {any issues}
```

```text
I have a Cypilot execution plan at:
  .cypilot/.plans/implement-code-orm-connectors/plan.toml

Phase 7 is complete (PASS).
Please read the plan manifest, then execute Phase 8: "Thread Safety — connection_pool + thread_local_db + db::transaction() RAII + tests".
The phase file is: .cypilot/.plans/implement-code-orm-connectors/phase-08-thread-safety.md
It is self-contained — follow its instructions exactly.
After completion, report results and generate the prompt for the next phase.
```
