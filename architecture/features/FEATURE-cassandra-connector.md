# Feature: Apache Cassandra Connector

- [ ] `p1` - **ID**: `cpt-orm-featstatus-cassandra-connector`

- [ ] `p2` - **ID**: `cpt-orm-feature-cassandra-connector`

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
  - [Execute a Filtered Query Against Cassandra](#execute-a-filtered-query-against-cassandra)
- [3. Processes / Business Logic (CDSL)](#3-processes--business-logic-cdsl)
  - [Compile-Time Partition Key Validation](#compile-time-partition-key-validation)
  - [IR-to-CQL Rendering](#ir-to-cql-rendering)
  - [Result Hydration from DataStax Cursor](#result-hydration-from-datastax-cursor)
- [4. States](#4-states)
- [5. Definitions of Done](#5-definitions-of-done)
  - [connector\_trait\<CassandraDB\> Specialisation](#connector_traitcassandradb-specialisation)
  - [Partition Key static\_assert](#partition-key-static_assert)
  - [CQL Rendering](#cql-rendering)
  - [Capability Tags](#capability-tags)
  - [Test Coverage Guidance](#test-coverage-guidance)
- [6. Acceptance Criteria](#6-acceptance-criteria)

<!-- /toc -->

## 1. Feature Context

### 1.1 Overview

`connector_trait<CassandraDB>` specialisation using the DataStax C++ Driver that translates the ORM compile-time query IR to CQL (Cassandra Query Language) prepared statements, while enforcing at compile time that every WHERE clause includes a partition key predicate — preventing the full-table-scan queries that are catastrophically expensive in a distributed Cassandra cluster.

### 1.2 Purpose

Apache Cassandra enforces strict partition key constraints in WHERE clauses; queries that omit the partition key trigger a full-cluster scan. This connector makes violating that constraint a compile-time error rather than a runtime catastrophe. The connector handles CQL's positional `?` parameter syntax via the DataStax C++ Driver.

**Injection prevention**: CQL injection is prevented by construction — all query parameters are bound via `cass_statement_bind_*` using the DataStax prepared-statement API; no user-supplied data is ever concatenated into the CQL string.

**Performance**: Hot-path optimisations (async I/O, zero-copy result parsing, batch INSERT) are delegated to `FEATURE-wire-protocol`. This connector's responsibility is correct translation of the compile-time IR to DataStax C++ Driver calls.

**Reliability**: Retry logic, circuit breakers, and connection failover are not applicable to this connector layer; they are the responsibility of the caller-supplied session handle or a higher-level infrastructure component.

**Known limitations**:
- The DataStax C++ Driver duplicates argument values at each reuse site for indexed placeholder reuse (`orm::ph<T, std::placeholders::_N>`), because CQL has no native indexed parameter syntax. This means the bind list grows proportionally to the number of reuse occurrences, not the number of distinct placeholder indices.

**Requirements**: DESIGN pending — no PRD or DESIGN artifact registered yet.

**Principles**: DESIGN pending.

### 1.3 Actors

| Actor | Role in Feature |
|-------|-----------------|
| Developer | Specialises `connector_trait<CassandraDB>`, constructs compile-time queries that must include a partition key predicate in WHERE, and calls `execute()` to run them against a Cassandra cluster. |

### 1.4 References

- **PRD**: DESIGN pending — no PRD registered.
- **Design**: DESIGN pending — no DESIGN artifact registered.
- **Dependencies**: DataStax C++ Driver (`cassandra.h`).
- **Security boundary**: This connector accepts a caller-supplied `CassFuture*` session handle. Authentication, TLS configuration, and credential management are entirely the caller's responsibility; this connector does not handle or store credentials.

### 1.5 Scope & Boundaries

**In scope**:
- Translation of the ORM compile-time query IR to CQL prepared statements via the DataStax C++ Driver.
- Compile-time partition key validation enforcing that every WHERE clause includes a partition key predicate.
- Positional `?` parameter binding via `cass_statement_bind_*` calls.
- Result hydration from DataStax cursor into `orm::result<Row...>` lazy ranges.
- Capability tag declarations applicable to Cassandra.

**Out of scope**:
- Connection management, pooling, and authentication — see `FEATURE-thread-safety`.
- Async I/O and wire-level optimisations — see `FEATURE-wire-protocol`.
- Cassandra-specific features (lightweight transactions, user-defined types, materialized views) beyond the core ORM query IR.
- Multi-partition batch statements.

### 1.6 Configuration

Not applicable — this connector has no runtime configuration options or feature flags. All connector behaviour is determined at compile time by the query IR and the `connector_trait<CassandraDB>` specialisation.

### Non-Applicability Declarations

- **UX**: Not applicable — this is a C++ library connector with no user-facing interface.
- **COMPL (Compliance)**: Not applicable — no regulatory or standards-compliance requirements apply to this connector layer. Data privacy (PII/GDPR): Not applicable at the connector layer — the connector transmits only what the caller supplies; PII handling is the application's responsibility.
- **OPS (Observability)**: Not applicable — logging, metrics, and tracing are not applicable to this connector; instrumentation is the responsibility of the caller.
- **OPS (Rollout/Rollback)**: Not applicable — this is a C++ library feature distributed as source; rollout and rollback are governed by the consuming application's dependency management.
- **SEC (Authentication/Authorization)**: Not applicable to this connector — authentication is delegated to the caller-supplied session handle (see §1.4 Security boundary).
- **PERF (Hot paths / N+1 prevention)**: Not applicable to this connector — performance optimisations are delegated to `FEATURE-wire-protocol`.
- **REL (Retry / circuit breaker)**: Not applicable to this connector — retry and failover are the responsibility of the caller or infrastructure layer.

---

## 2. Actor Flows (CDSL)

### Execute a Filtered Query Against Cassandra

- [ ] `p1` - **ID**: `cpt-orm-flow-cassandra-connector-execute-query`

**Actor**: Developer

**Success Scenarios**:
- Developer executes a compile-time SELECT query with a partition key equality predicate and receives a typed result range.
- Developer adds a clustering key predicate in addition to the partition key; the connector passes both to CQL.

**Error Scenarios**:
- Query omits partition key predicate: `static_assert` compile error — query never reaches runtime.
- Cassandra cluster unreachable: `execute()` propagates DataStax error through `orm::result` error state.
- Network call exceeds caller-configured request timeout: DataStax driver returns a timeout error; connector propagates it through `orm::result` error state without retry.

**Steps**:
1. [ ] - `p1` - Developer instantiates `db<CassandraDB>` with a valid `CassFuture*` session handle. - `inst-cass-connect`
2. [ ] - `p1` - Developer constructs a compile-time query using `select(...)`, `.where(...)` — IR fully encoded in template parameters. - `inst-cass-build-query`
3. [ ] - `p1` - Compile-time partition key validation runs as part of template instantiation; **IF** partition key absent — `static_assert` failure. - `inst-cass-pk-validate`
4. [ ] - `p1` - Developer calls `db.execute(query, runtime_args...)`. - `inst-cass-call-execute`
5. [ ] - `p1` - Connector invokes IR-to-CQL rendering to produce a CQL string with `?` positional parameters. - `inst-cass-render-cql`
6. [ ] - `p1` - Connector calls `cass_session_prepare` to prepare the CQL statement. - `inst-cass-prepare`
7. [ ] - `p1` - Connector binds runtime parameters via `cass_statement_bind_*` calls. - `inst-cass-bind`
8. [ ] - `p1` - Connector calls `cass_session_execute` and awaits the result future. - `inst-cass-exec`
9. [ ] - `p1` - **IF** the result future indicates a timeout or network error — propagate error through `orm::result` error state and **RETURN**. - `inst-cass-timeout`
10. [ ] - `p1` - **IF** execution succeeds — wrap `CassResult*` in RAII holder and return lazy `orm::result` range. - `inst-cass-return-result`
11. [ ] - `p1` - **ELSE** — propagate DataStax error through `orm::result` error state. - `inst-cass-error`
12. [ ] - `p1` - **RETURN** `orm::result<Row...>` to caller. - `inst-cass-return`

---

## 3. Processes / Business Logic (CDSL)

### Compile-Time Partition Key Validation

- [ ] `p2` - **ID**: `cpt-orm-algo-cassandra-connector-validate-pk`

**Input**: WHERE predicate IR tree at compile time; entity's partition key column designation.

**Output**: Compile success or `static_assert` failure with a descriptive message.

**Steps**:
1. [ ] - `p1` - Inspect the WHERE predicate IR tree at compile time by walking its type list. - `inst-pk-inspect-where`
2. [ ] - `p1` - Search the predicate tree for a leaf node that references the entity's designated partition key column with an equality comparison. - `inst-pk-search-eq`
3. [ ] - `p1` - **IF** no such leaf node found — emit `static_assert(false, "CassandraDB: WHERE clause must include an equality predicate on the partition key column")`. - `inst-pk-assert`
4. [ ] - `p1` - **RETURN** compile success when the partition key predicate is found. - `inst-pk-return`

### IR-to-CQL Rendering

- [ ] `p2` - **ID**: `cpt-orm-algo-cassandra-connector-render-cql`

**Input**: Compile-time query IR; runtime parameter values.

**Output**: CQL string with `?` positional parameters + ordered bind value list for the DataStax driver.

**Steps**:
1. [ ] - `p1` - Walk query IR: SELECT columns → FROM clause (table name) → WHERE predicate tree → ORDER BY (clustering columns only) → LIMIT. - `inst-cql-walk-ir`
2. [ ] - `p1` - For each IR node emit the corresponding CQL token or clause fragment. - `inst-cql-emit-token`
3. [ ] - `p1` - **FOR EACH** `Placeholder<T>` node — emit `?` and append runtime value to the bind list. - `inst-cql-positional-ph`
4. [ ] - `p1` - **FOR EACH** indexed placeholder `orm::ph<T, std::placeholders::_N>` — emit `?` and duplicate the runtime argument at each reuse site in the bind list (CQL has no native indexed parameter reuse). - `inst-cql-indexed-ph`
5. [ ] - `p1` - **RETURN** `{cql_string, bind_value_list}` pair. - `inst-cql-return`

### Result Hydration from DataStax Cursor

- [ ] `p2` - **ID**: `cpt-orm-algo-cassandra-connector-hydrate-result`

**Input**: `CassResult*` from a successful `cass_session_execute` call.

**Output**: Lazy range of C++ entity instances.

**Steps**:
1. [ ] - `p1` - Wrap `CassResult*` in RAII holder that calls `cass_result_free` on destruction. - `inst-cass-hydrate-raii`
2. [ ] - `p1` - Obtain a `CassIterator*` via `cass_iterator_from_result`. - `inst-cass-hydrate-iter`
3. [ ] - `p1` - **FOR EACH** row returned by `cass_iterator_next` — obtain `const CassRow*`. - `inst-cass-hydrate-next-row`
4. [ ] - `p1` - **FOR EACH** selected column in the IR — call `cass_row_get_column_by_name` and map the `CassValue*` to the corresponding C++ type. - `inst-cass-hydrate-col`
5. [ ] - `p1` - Construct the C++ entity instance from the mapped field values. - `inst-cass-hydrate-construct`
6. [ ] - `p1` - **RETURN** the entity instance to the lazy range consumer. - `inst-cass-hydrate-return`

---

## 4. States

Not applicable — the Cassandra connector is a stateless transform from query IR to DataStax C++ Driver calls. Session and cluster lifecycle are managed by the caller-supplied session handle.

---

## 5. Definitions of Done

### `connector_trait<CassandraDB>` Specialisation

- [ ] `p1` - **ID**: `cpt-orm-dod-cassandra-connector-trait-specialisation`

The system **MUST** provide a complete `connector_trait<CassandraDB>` specialisation that compiles cleanly with C++23 and satisfies the `ConnectorTrait` concept required by the ORM core.

**Implements**:
- `cpt-orm-flow-cassandra-connector-execute-query`
- `cpt-orm-algo-cassandra-connector-render-cql`

**Touches**:
- Entities: `connector_trait<CassandraDB>`, `CassandraDB` tag type
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Partition Key `static_assert`

- [ ] `p1` - **ID**: `cpt-orm-dod-cassandra-connector-pk-static-assert`

The system **MUST** emit a `static_assert` compile error when a query targeting `CassandraDB` has a WHERE clause that does not include an equality predicate on the partition key column, preventing the query from reaching runtime.

**Implements**:
- `cpt-orm-algo-cassandra-connector-validate-pk`

**Touches**:
- Entities: `connector_trait<CassandraDB>` compile-time validation
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### CQL Rendering

- [ ] `p1` - **ID**: `cpt-orm-dod-cassandra-connector-cql-render`

The system **MUST** produce a syntactically valid CQL string with `?` positional parameters for all supported IR constructs (SELECT columns, FROM, WHERE with partition/clustering key predicates, LIMIT).

**Implements**:
- `cpt-orm-algo-cassandra-connector-render-cql`

**Touches**:
- Entities: `connector_trait<CassandraDB>` render path
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Capability Tags

- [ ] `p2` - **ID**: `cpt-orm-dod-cassandra-connector-capability-tags`

The system **MUST** declare only the capability tags applicable to Cassandra within `connector_trait<CassandraDB>`. `supports_joins` **MUST NOT** be declared (cross-partition joins are unsupported in CQL).

**Implements**:
- `cpt-orm-feature-cassandra-connector`

**Touches**:
- Entities: `connector_trait<CassandraDB>`
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Test Coverage Guidance

**Unit test targets**:
- `connector_trait<CassandraDB>::execute()` with a mock session that stubs `cass_session_prepare`, `cass_statement_bind_*`, `cass_session_execute`, and `cass_result_free`.
- Partition key validation: verify `static_assert` fires when WHERE clause omits the partition key.
- CQL rendering: verify `?`-parameterised CQL string for SELECT with partition key + clustering key predicates.
- Timeout/error propagation: mock DataStax future returning a timeout error and verify `orm::result` carries the error.

**Integration test targets**:
- Full round-trip against a live Cassandra node (SELECT with partition key, INSERT).
- Partition key static_assert end-to-end: confirm compile error message contains "partition key".

**Mock boundaries**: The DataStax `CassFuture*` session handle and driver function pointers are the primary mock boundaries.

**Test isolation**: Unit tests must not require a live Cassandra node; use a thin mock layer over the DataStax C++ Driver.

---

## 6. Acceptance Criteria

- [ ] `connector_trait<CassandraDB>` compiles with no errors or warnings under C++23 with `-Wall -Wextra`.
- [ ] A SELECT query with a partition key equality WHERE predicate compiles and executes correctly against a Cassandra node.
- [ ] A SELECT query whose WHERE clause omits the partition key produces a `static_assert` compile error with a message containing "partition key".
- [ ] Rendered CQL for a two-predicate query (partition key + clustering key) is syntactically valid CQL verified by the DataStax driver's statement parser.
- [ ] `cass_result_free` is called exactly once per result, including on the error path.
- [ ] Referencing `supports_joins` in `connector_trait<CassandraDB>` produces a `static_assert` compile error.
