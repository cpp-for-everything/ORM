# Feature: Neo4j Graph Database Connector

- [ ] `p1` - **ID**: `cpt-orm-featstatus-neo4j-connector`

- [ ] `p2` - **ID**: `cpt-orm-feature-neo4j-connector`

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
  - [Execute a Graph Traversal Query Against Neo4j](#execute-a-graph-traversal-query-against-neo4j)
- [3. Processes / Business Logic (CDSL)](#3-processes--business-logic-cdsl)
  - [IR-to-Cypher Rendering](#ir-to-cypher-rendering)
  - [Bolt Protocol Dispatch](#bolt-protocol-dispatch)
  - [Result Hydration from Bolt Record Stream](#result-hydration-from-bolt-record-stream)
- [4. States](#4-states)
- [5. Definitions of Done](#5-definitions-of-done)
  - [connector\_trait\<Neo4jDB\> Specialisation](#connector_traitneo4jdb-specialisation)
  - [Cypher Rendering — SELECT and JOIN Translation](#cypher-rendering--select-and-join-translation)
  - [traverse\<Relationship\>() Extension](#traverserelationship-extension)
  - [Bolt Protocol Dispatch](#bolt-protocol-dispatch-1)
  - [Capability Tags](#capability-tags)
  - [Test Coverage Guidance](#test-coverage-guidance)
- [6. Acceptance Criteria](#6-acceptance-criteria)

<!-- /toc -->

## 1. Feature Context

### 1.1 Overview

`connector_trait<Neo4jDB>` specialisation that translates the ORM compile-time query IR to Cypher query language via the Neo4j Bolt protocol (`libneo4j-client`), and extends the query IR with a `.traverse<Relationship>()` operation for graph relationship traversal.

### 1.2 Purpose

Neo4j uses Cypher for graph traversal — a fundamentally different query paradigm from relational SQL. This connector provides a rendering pass that maps the ORM's relational-superset IR to Cypher: SELECT → `MATCH`, JOINs → relationship traversal patterns, WHERE predicates remain structurally similar. The `.traverse<Relationship>()` extension exposes graph-specific operations while keeping the same compile-time type safety and fluent query-building style.

**Performance**: Hot-path optimisations (async I/O, zero-copy result parsing) are delegated to `FEATURE-wire-protocol`. This connector's responsibility is correct translation of the compile-time IR to Cypher via the Bolt protocol.

**Reliability**: Retry logic, circuit breakers, and connection failover are not applicable to this connector layer; they are the responsibility of the caller-supplied `neo4j_connection_t*` handle or a higher-level infrastructure component.

**Known limitations**:
- Cypher syntax errors caused by unsupported IR constructs (e.g. IR nodes with no defined Cypher mapping) are only detected at execute time, not at compile time. This is an architectural limitation of the Cypher rendering pass and will be addressed when the IR-to-Cypher mapping is fully specified.

**Requirements**: DESIGN pending — no PRD or DESIGN artifact registered yet.

**Principles**: DESIGN pending.

### 1.3 Actors

| Actor | Role in Feature |
|-------|-----------------|
| Developer | Specialises `connector_trait<Neo4jDB>`, constructs compile-time queries optionally using `.traverse<Relationship>()`, and calls `execute()` to run them against a Neo4j server via Bolt. |

### 1.4 References

- **PRD**: DESIGN pending — no PRD registered.
- **Design**: DESIGN pending — no DESIGN artifact registered.
- **Dependencies**: `libneo4j-client` (Neo4j Bolt protocol client library).
- **Security boundary**: This connector accepts a caller-supplied `neo4j_connection_t*` connection handle. Authentication, TLS configuration, and credential management are entirely the caller's responsibility; this connector does not handle or store credentials.

### 1.5 Scope & Boundaries

**In scope**:
- Translation of the ORM compile-time query IR to Cypher query strings with named `$param` parameters.
- `.traverse<Relationship>()` IR extension rendering as Cypher relationship patterns.
- Bolt protocol dispatch via `libneo4j-client`.
- Result hydration from Bolt record stream into `orm::result<Row...>` lazy ranges.
- Capability tag declarations applicable to Neo4j.

**Out of scope**:
- Connection management, pooling, and authentication — see `FEATURE-thread-safety`.
- Async I/O and wire-level optimisations — see `FEATURE-wire-protocol`.
- Schema management (Neo4j is schema-optional; DDL migration not applicable).
- Neo4j-specific features (APOC procedures, graph algorithms, full-text indexes) beyond the core ORM query IR.

### 1.6 Configuration

Not applicable — this connector has no runtime configuration options or feature flags. All connector behaviour is determined at compile time by the query IR and the `connector_trait<Neo4jDB>` specialisation.

### Non-Applicability Declarations

- **UX**: Not applicable — this is a C++ library connector with no user-facing interface.
- **COMPL (Compliance)**: Not applicable — no regulatory or standards-compliance requirements apply to this connector layer. Data privacy (PII/GDPR): Not applicable at the connector layer — the connector transmits only what the caller supplies; PII handling is the application's responsibility.
- **OPS (Observability)**: Not applicable — logging, metrics, and tracing are not applicable to this connector; instrumentation is the responsibility of the caller.
- **OPS (Rollout/Rollback)**: Not applicable — this is a C++ library feature distributed as source; rollout and rollback are governed by the consuming application's dependency management.
- **SEC (Authentication/Authorization)**: Not applicable to this connector — authentication is delegated to the caller-supplied `neo4j_connection_t*` handle (see §1.4 Security boundary).
- **PERF (Hot paths / N+1 prevention)**: Not applicable to this connector — performance optimisations are delegated to `FEATURE-wire-protocol`.
- **REL (Retry / circuit breaker)**: Not applicable to this connector — retry and failover are the responsibility of the caller or infrastructure layer.

---

## 2. Actor Flows (CDSL)

### Execute a Graph Traversal Query Against Neo4j

- [ ] `p1` - **ID**: `cpt-orm-flow-neo4j-connector-execute-traversal`

**Actor**: Developer

**Success Scenarios**:
- Developer executes a compile-time SELECT query and receives a typed result range of hydrated entity instances.
- Developer chains `.traverse<Relationship>()` to navigate graph edges; the connector renders the traversal as a Cypher relationship pattern in the MATCH clause.

**Error Scenarios**:
- Neo4j server unreachable: `execute()` propagates `libneo4j-client` error through `orm::result` error state.
- Cypher syntax error (caused by an unsupported IR construct): connector surfaces error at execute time.
- Network call exceeds caller-configured connection timeout: `libneo4j-client` returns a connection error; connector propagates it through `orm::result` error state without retry.

**Steps**:
1. [ ] - `p1` - Developer instantiates `db<Neo4jDB>` with a valid `neo4j_connection_t*` handle. - `inst-neo4j-connect`
2. [ ] - `p1` - Developer constructs a compile-time query using `select(...)`, optional `.traverse<Relationship>()`, `.where(...)` — IR fully encoded in template parameters. - `inst-neo4j-build-query`
3. [ ] - `p1` - Developer calls `db.execute(query, runtime_args...)`. - `inst-neo4j-call-execute`
4. [ ] - `p1` - Connector invokes IR-to-Cypher rendering to produce a Cypher string with parameter map. - `inst-neo4j-render-cypher`
5. [ ] - `p1` - Connector invokes Bolt protocol dispatch to send the Cypher string and parameters to Neo4j. - `inst-neo4j-bolt-dispatch`
6. [ ] - `p1` - **IF** Bolt response is an error or timeout — propagate through `orm::result` error state and **RETURN**. - `inst-neo4j-bolt-error`
7. [ ] - `p1` - Connector invokes result hydration over the returned Bolt record stream. - `inst-neo4j-hydrate`
8. [ ] - `p1` - **RETURN** lazy `orm::result<Row...>` range to caller. - `inst-neo4j-return`

---

## 3. Processes / Business Logic (CDSL)

### IR-to-Cypher Rendering

- [ ] `p2` - **ID**: `cpt-orm-algo-neo4j-connector-render-cypher`

**Input**: Compile-time query IR including optional `.traverse<Relationship>()` nodes; runtime parameter values.

**Output**: Cypher query string with named `$param` parameters + parameter map for Bolt dispatch.

**Steps**:
1. [ ] - `p1` - Begin MATCH clause: emit `MATCH (n:EntityLabel)` for the root entity. - `inst-cypher-match-root`
2. [ ] - `p1` - **FOR EACH** `.traverse<Relationship>()` node in the IR — emit a relationship traversal pattern: `(n)-[:RELATIONSHIP_LABEL]->(m:TargetLabel)`. - `inst-cypher-traverse`
3. [ ] - `p1` - **FOR EACH** JOIN node in the IR (non-traverse) — emit an additional `MATCH` pattern for the joined entity. - `inst-cypher-join`
4. [ ] - `p1` - Emit `RETURN` clause from the SELECT column list, prefixed with the appropriate node variable (e.g. `n.field`, `m.field`). - `inst-cypher-return-clause`
5. [ ] - `p1` - **IF** WHERE predicates are present — emit `WHERE` clause, mapping AND/OR/comparison nodes to equivalent Cypher boolean expressions. - `inst-cypher-where`
6. [ ] - `p1` - **FOR EACH** `Placeholder<T>` or `orm::ph` node — assign a named parameter `$p1`, `$p2`, … and record the runtime value in the parameter map. - `inst-cypher-params`
7. [ ] - `p1` - **IF** ORDER BY present — emit `ORDER BY` clause with column references and direction. - `inst-cypher-order`
8. [ ] - `p1` - **IF** LIMIT present — emit `LIMIT $limitN` with the limit value in the parameter map. - `inst-cypher-limit`
9. [ ] - `p1` - **RETURN** `{cypher_string, parameter_map}` pair. - `inst-cypher-return`

### Bolt Protocol Dispatch

- [ ] `p2` - **ID**: `cpt-orm-algo-neo4j-connector-bolt-dispatch`

**Input**: Cypher string; parameter map; `neo4j_connection_t*` handle.

**Output**: `neo4j_result_stream_t*` on success; error on failure.

**Steps**:
1. [ ] - `p1` - Encode Cypher string and parameter map into a Bolt `RUN` message via `neo4j_run`. - `inst-bolt-run`
2. [ ] - `p1` - **IF** `neo4j_run` returns NULL — read error via `neo4j_error_message` and **RETURN** error. - `inst-bolt-run-error`
3. [ ] - `p1` - Wrap `neo4j_result_stream_t*` in RAII holder that calls `neo4j_close_results` on destruction. - `inst-bolt-raii`
4. [ ] - `p1` - **RETURN** the result stream to the hydration process. - `inst-bolt-return`

### Result Hydration from Bolt Record Stream

- [ ] `p2` - **ID**: `cpt-orm-algo-neo4j-connector-hydrate-result`

**Input**: `neo4j_result_stream_t*` from a successful Bolt dispatch.

**Output**: Lazy range of C++ entity instances.

**Steps**:
1. [ ] - `p1` - **FOR EACH** result record returned by `neo4j_fetch_next` — obtain `neo4j_result_t*`. - `inst-hydrate-neo4j-next`
2. [ ] - `p1` - **FOR EACH** selected column in the IR — call `neo4j_result_field` by field index and obtain `neo4j_value_t`. - `inst-hydrate-neo4j-field`
3. [ ] - `p1` - Map `neo4j_value_t` to the corresponding C++ type using `neo4j_type` dispatch: `NEO4J_INT` → `int64_t`; `NEO4J_STRING` → `std::u8string`; `NEO4J_BOOL` → `bool`; `NEO4J_FLOAT` → `double`. - `inst-hydrate-neo4j-type-map`
4. [ ] - `p1` - Construct the C++ entity instance from the mapped field values. - `inst-hydrate-neo4j-construct`
5. [ ] - `p1` - **RETURN** the entity instance to the lazy range consumer. - `inst-hydrate-neo4j-return`

---

## 4. States

Not applicable — the Neo4j connector is a stateless transform from query IR to `libneo4j-client` calls. Connection lifecycle is managed by the caller-supplied `neo4j_connection_t*` handle.

---

## 5. Definitions of Done

### `connector_trait<Neo4jDB>` Specialisation

- [ ] `p1` - **ID**: `cpt-orm-dod-neo4j-connector-trait-specialisation`

The system **MUST** provide a complete `connector_trait<Neo4jDB>` specialisation that compiles cleanly with C++23 and satisfies the `ConnectorTrait` concept required by the ORM core.

**Implements**:
- `cpt-orm-flow-neo4j-connector-execute-traversal`
- `cpt-orm-algo-neo4j-connector-render-cypher`

**Touches**:
- Entities: `connector_trait<Neo4jDB>`, `Neo4jDB` tag type
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Cypher Rendering — SELECT and JOIN Translation

- [ ] `p1` - **ID**: `cpt-orm-dod-neo4j-connector-cypher-render`

The system **MUST** correctly translate SELECT columns to a Cypher `RETURN` clause, JOIN nodes to additional `MATCH` patterns, and WHERE predicates to a Cypher `WHERE` clause with named `$param` parameters.

**Implements**:
- `cpt-orm-algo-neo4j-connector-render-cypher`

**Touches**:
- Entities: `connector_trait<Neo4jDB>` render path
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### `.traverse<Relationship>()` Extension

- [ ] `p1` - **ID**: `cpt-orm-dod-neo4j-connector-traverse-op`

The system **MUST** compile and correctly render `.traverse<Relationship>()` IR nodes as `(n)-[:RELATIONSHIP_LABEL]->(m:TargetLabel)` relationship patterns in the Cypher MATCH clause.

**Implements**:
- `cpt-orm-algo-neo4j-connector-render-cypher`

**Touches**:
- Entities: `connector_trait<Neo4jDB>` traverse IR extension
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Bolt Protocol Dispatch

- [ ] `p1` - **ID**: `cpt-orm-dod-neo4j-connector-bolt-dispatch`

The system **MUST** send Cypher strings and parameter maps to Neo4j via `libneo4j-client`'s `neo4j_run` API and correctly manage the `neo4j_result_stream_t*` lifecycle with RAII cleanup.

**Implements**:
- `cpt-orm-algo-neo4j-connector-bolt-dispatch`

**Touches**:
- Entities: `connector_trait<Neo4jDB>` Bolt dispatch path
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Capability Tags

- [ ] `p2` - **ID**: `cpt-orm-dod-neo4j-connector-capability-tags`

The system **MUST** declare only the capability tags applicable to Neo4j within `connector_trait<Neo4jDB>`. `supports_transactions` may be declared if Neo4j Bolt transactions are implemented; `supports_aggregation` **MUST NOT** be declared unless CQL-equivalent aggregation is implemented.

**Implements**:
- `cpt-orm-feature-neo4j-connector`

**Touches**:
- Entities: `connector_trait<Neo4jDB>`
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Test Coverage Guidance

**Unit test targets**:
- `connector_trait<Neo4jDB>::execute()` with a mock `neo4j_connection_t*` handle that stubs `neo4j_run`, `neo4j_fetch_next`, and `neo4j_close_results`.
- Cypher rendering: verify MATCH/RETURN/WHERE clause structure for SELECT, JOIN, and `.traverse<Relationship>()` nodes.
- Named-parameter map: verify `$p1`, `$p2`, … assignment and correct values in the parameter map.
- Timeout/error propagation: mock `neo4j_run` returning NULL and verify `orm::result` carries the error.

**Integration test targets**:
- Full round-trip against a live Neo4j instance (MATCH with WHERE, CREATE).
- `.traverse<Relationship>()` end-to-end: verify relationship pattern in Cypher MATCH clause.

**Mock boundaries**: The `neo4j_connection_t*` handle and `libneo4j-client` function pointers are the primary mock boundaries.

**Test isolation**: Unit tests must not require a live Neo4j server; use a thin mock layer over `libneo4j-client`.

---

## 6. Acceptance Criteria

- [ ] `connector_trait<Neo4jDB>` compiles with no errors or warnings under C++23 with `-Wall -Wextra`.
- [ ] A SELECT query without `.traverse<Relationship>()` produces a Cypher string of the form `MATCH (n:Label) RETURN n.field WHERE ...`.
- [ ] A query with `.traverse<User, Friend>()` produces a Cypher MATCH pattern containing `(n)-[:Friend]->(m:User)`.
- [ ] Runtime parameters are encoded as named `$p1`, `$p2`, … in the Cypher string and are correctly passed in the parameter map to `neo4j_run`.
- [ ] `neo4j_close_results` is called exactly once per result stream, including on the error path.
