# Feature: PostgreSQL Connector

- [ ] `p1` - **ID**: `cpt-orm-featstatus-postgresql-connector`

- [ ] `p2` - **ID**: `cpt-orm-feature-postgresql-connector`

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
  - [Execute a Prepared Query Against PostgreSQL](#execute-a-prepared-query-against-postgresql)
- [3. Processes / Business Logic (CDSL)](#3-processes--business-logic-cdsl)
  - [IR-to-PostgreSQL-SQL Rendering](#ir-to-postgresql-sql-rendering)
  - [Result Hydration](#result-hydration)
- [4. States](#4-states)
- [5. Definitions of Done](#5-definitions-of-done)
  - [connector\_trait\<PostgreSQLDB\> Specialisation](#connector_traitpostgresqldb-specialisation)
  - [Prepared-Statement Lifecycle via libpq](#prepared-statement-lifecycle-via-libpq)
  - [Dollar-Parameter Rendering](#dollar-parameter-rendering)
  - [Indexed Placeholder Native Reuse](#indexed-placeholder-native-reuse)
  - [Capability Tags](#capability-tags)
  - [Test Coverage Guidance](#test-coverage-guidance)
- [6. Acceptance Criteria](#6-acceptance-criteria)

<!-- /toc -->

## 1. Feature Context

### 1.1 Overview

`connector_trait<PostgreSQLDB>` specialisation that translates the ORM compile-time query IR to PostgreSQL libpq prepared-statement calls (`PQprepare`, `PQexecPrepared`), using `$1`, `$2`, … positional parameter syntax and natively supporting indexed placeholder reuse by emitting the same `$N` token at each reuse site.

### 1.2 Purpose

PostgreSQL offers the most expressive SQL dialect among open-source relational databases, including window functions, CTEs, JSON operators, and array types. This connector enables the ORM to target PostgreSQL with full compile-time query safety. Because PostgreSQL uses `$N` numbered parameters, indexed placeholder reuse is natively supported without a rewrite pass — the same `$N` token is simply emitted wherever the same indexed placeholder appears in the IR.

**Injection prevention**: SQL injection is prevented by construction — all query parameters are passed as separate values arrays to `PQexecPrepared`; no user-supplied data is ever concatenated into the SQL string.

**Performance**: Hot-path optimisations (async I/O, zero-copy result parsing, batch INSERT) are delegated to `FEATURE-wire-protocol`. This connector's responsibility is correct translation of the compile-time IR to libpq calls.

**Reliability**: Retry logic, circuit breakers, and connection failover are not applicable to this connector layer; they are the responsibility of the caller-supplied connection handle or a higher-level infrastructure component.

**Requirements**: DESIGN pending — no PRD or DESIGN artifact registered yet.

**Principles**: DESIGN pending.

### 1.3 Actors

| Actor | Role in Feature |
|-------|-----------------|
| Developer | Specialises `connector_trait<PostgreSQLDB>`, constructs compile-time queries, and calls `execute()` to run them against a PostgreSQL server via libpq. |

### 1.4 References

- **PRD**: DESIGN pending — no PRD registered.
- **Design**: DESIGN pending — no DESIGN artifact registered.
- **Dependencies**: libpq (PostgreSQL C client library).
- **Security boundary**: This connector accepts a caller-supplied `PGconn*` connection handle. Authentication, TLS configuration, and credential management are entirely the caller's responsibility; this connector does not handle or store credentials.

### 1.5 Scope & Boundaries

**In scope**:
- Translation of the ORM compile-time query IR to libpq prepared-statement calls (`PQprepare`, `PQexecPrepared`).
- `$N`-parameter rendering with native indexed-placeholder reuse.
- Result hydration into `orm::result<Row...>` lazy ranges via `PGresult*`.
- Capability tag declarations (`supports_joins`, `supports_transactions`, `supports_aggregation`).

**Out of scope**:
- Connection management, pooling, and authentication — see `FEATURE-thread-safety`.
- Async I/O and wire-level optimisations — see `FEATURE-wire-protocol`.
- Schema introspection and DDL generation — see `FEATURE-schema-migration`.
- PostgreSQL-specific extensions (LISTEN/NOTIFY, logical replication, COPY protocol) beyond the core ORM type system.

### 1.6 Configuration

Not applicable — this connector has no runtime configuration options or feature flags. All connector behaviour is determined at compile time by the query IR and the `connector_trait<PostgreSQLDB>` specialisation.

### Non-Applicability Declarations

- **UX**: Not applicable — this is a C++ library connector with no user-facing interface.
- **COMPL (Compliance)**: Not applicable — no regulatory or standards-compliance requirements apply to this connector layer. Data privacy (PII/GDPR): Not applicable at the connector layer — the connector transmits only what the caller supplies; PII handling is the application's responsibility.
- **OPS (Observability)**: Not applicable — logging, metrics, and tracing are not applicable to this connector; instrumentation is the responsibility of the caller.
- **OPS (Rollout/Rollback)**: Not applicable — this is a C++ library feature distributed as source; rollout and rollback are governed by the consuming application's dependency management.
- **SEC (Authentication/Authorization)**: Not applicable to this connector — authentication is delegated to the caller-supplied `PGconn*` handle (see §1.4 Security boundary).
- **PERF (Hot paths / N+1 prevention)**: Not applicable to this connector — performance optimisations are delegated to `FEATURE-wire-protocol`.
- **REL (Retry / circuit breaker)**: Not applicable to this connector — retry and failover are the responsibility of the caller or infrastructure layer.

---

## 2. Actor Flows (CDSL)

### Execute a Prepared Query Against PostgreSQL

- [ ] `p1` - **ID**: `cpt-orm-flow-postgresql-connector-execute-prepared`

**Actor**: Developer

**Success Scenarios**:
- Developer executes a compile-time SELECT query with positional parameters and receives a typed result range.
- Developer executes a query with an indexed placeholder (`orm::ph<T, std::placeholders::_1>`) reused in two conditions; the same `$1` token appears at both positions without argument duplication.

**Error Scenarios**:
- PostgreSQL server unreachable: `execute()` propagates libpq error string.
- Parameter type mismatch: libpq rejects bind and `orm::result` carries the error.
- Network call exceeds caller-configured connection timeout: libpq returns a connection-timeout error; connector propagates it through `orm::result` error state without retry.

**Steps**:
1. [ ] - `p1` - Developer instantiates `db<PostgreSQLDB>` with a valid `PGconn*` handle. - `inst-pg-connect`
2. [ ] - `p1` - Developer constructs a compile-time query using `select(...)`, `.where(...)`, etc. — query IR fully encoded in template parameters. - `inst-pg-build-query`
3. [ ] - `p1` - Developer calls `db.execute(query, runtime_args...)`. - `inst-pg-call-execute`
4. [ ] - `p1` - Connector invokes IR-to-PostgreSQL-SQL rendering process to produce a `$N`-parameterised SQL string and a values array. - `inst-pg-render`
5. [ ] - `p1` - Connector calls `PQprepare(conn, stmt_name, sql, nparams, NULL)`. - `inst-pg-prepare`
6. [ ] - `p1` - **IF** `PQresultStatus` is not `PGRES_COMMAND_OK` — propagate error and **RETURN** error result. - `inst-pg-prepare-error`
7. [ ] - `p1` - Connector calls `PQexecPrepared(conn, stmt_name, nparams, param_values, param_lengths, param_formats, result_format)`. - `inst-pg-exec-prepared`
8. [ ] - `p1` - **IF** `PQexecPrepared` returns NULL or status indicates network/timeout error — propagate error through `orm::result` error state and **RETURN**. - `inst-pg-timeout`
9. [ ] - `p1` - **IF** execution succeeds — wrap `PGresult*` in RAII holder and return lazy `orm::result` range. - `inst-pg-return-result`
10. [ ] - `p1` - **ELSE** — propagate libpq error message through `orm::result` error state. - `inst-pg-error`
11. [ ] - `p1` - **RETURN** `orm::result<Row...>` to caller. - `inst-pg-return`

---

## 3. Processes / Business Logic (CDSL)

### IR-to-PostgreSQL-SQL Rendering

- [ ] `p2` - **ID**: `cpt-orm-algo-postgresql-connector-render-sql`

**Input**: Compile-time query IR (constexpr structure) + runtime parameter value list.

**Output**: PostgreSQL SQL string with `$N` positional parameters + ordered `const char*` values array for `PQexecPrepared`.

**Steps**:
1. [ ] - `p1` - Walk query IR tree: SELECT columns → FROM clause → JOIN clauses → WHERE predicate tree → ORDER BY → GROUP BY → LIMIT. - `inst-pg-render-walk-ir`
2. [ ] - `p1` - Maintain a monotonically increasing parameter counter `N` starting at 1. - `inst-pg-render-counter`
3. [ ] - `p1` - **FOR EACH** `Placeholder<T>` node — emit `$N` into the SQL string, increment `N`, append the runtime argument to the values array. - `inst-pg-render-positional-ph`
4. [ ] - `p1` - **FOR EACH** indexed placeholder `orm::ph<T, std::placeholders::_K>` — **IF** index `K` not yet seen: assign it parameter number `N`, emit `$N`, increment `N`, append runtime arg to values array. **ELSE**: emit `$M` where `M` is the previously assigned number for index `K` — no new entry added to values array. - `inst-pg-render-indexed-ph`
5. [ ] - `p1` - **RETURN** `{sql_string, values_array, nparams}` triple. - `inst-pg-render-return`

### Result Hydration

- [ ] `p2` - **ID**: `cpt-orm-algo-postgresql-connector-hydrate-result`

**Input**: `PGresult*` from a successful `PQexecPrepared` call.

**Output**: Lazy range of C++ entity instances matching the query's selected columns.

**Steps**:
1. [ ] - `p1` - Wrap `PGresult*` in an RAII holder that calls `PQclear` on destruction. - `inst-pg-hydrate-raii`
2. [ ] - `p1` - Read row count via `PQntuples` and column count via `PQnfields`. - `inst-pg-hydrate-meta`
3. [ ] - `p1` - **FOR EACH** row index `r` from 0 to row count − 1 — construct a result row by mapping each `PQgetvalue(result, r, col_idx)` to the corresponding C++ entity field via the type system. - `inst-pg-hydrate-row`
4. [ ] - `p1` - Apply column-to-field mapping using the column name (via `PQfname`) matched against the entity property names from the query IR's selected column list. - `inst-pg-hydrate-col-map`
5. [ ] - `p1` - **RETURN** the hydrated row range. - `inst-pg-hydrate-return`

---

## 4. States

Not applicable — the PostgreSQL connector is a stateless transform from query IR to libpq calls. Connection lifecycle is managed by the caller-supplied `PGconn*` handle.

---

## 5. Definitions of Done

### `connector_trait<PostgreSQLDB>` Specialisation

- [ ] `p1` - **ID**: `cpt-orm-dod-postgresql-connector-trait-specialisation`

The system **MUST** provide a complete `connector_trait<PostgreSQLDB>` specialisation that compiles cleanly with C++23 and satisfies the `ConnectorTrait` concept required by the ORM core.

**Implements**:
- `cpt-orm-flow-postgresql-connector-execute-prepared`
- `cpt-orm-algo-postgresql-connector-render-sql`

**Touches**:
- Entities: `connector_trait<PostgreSQLDB>`, `PostgreSQLDB` tag type
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Prepared-Statement Lifecycle via libpq

- [ ] `p1` - **ID**: `cpt-orm-dod-postgresql-connector-prepared-stmt`

The system **MUST** correctly manage the libpq prepared-statement lifecycle: `PQprepare` → check `PGRES_COMMAND_OK` → `PQexecPrepared` → RAII `PQclear` on result destruction, including error-path cleanup.

**Implements**:
- `cpt-orm-flow-postgresql-connector-execute-prepared`

**Touches**:
- Entities: `PostgreSQLDB` connector internals
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Dollar-Parameter Rendering

- [ ] `p1` - **ID**: `cpt-orm-dod-postgresql-connector-dollar-params`

The system **MUST** render all positional `Placeholder<T>` nodes as `$1`, `$2`, … in the SQL string, with runtime arguments in the corresponding positions of the `PQexecPrepared` values array.

**Implements**:
- `cpt-orm-algo-postgresql-connector-render-sql`

**Touches**:
- Entities: `connector_trait<PostgreSQLDB>` render path
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Indexed Placeholder Native Reuse

- [ ] `p1` - **ID**: `cpt-orm-dod-postgresql-connector-indexed-ph-reuse`

The system **MUST** emit the same `$N` token at every occurrence of an indexed placeholder with index `N` in the IR, without duplicating the runtime argument in the values array, such that `nparams` passed to `PQexecPrepared` equals the number of distinct placeholder indices.

**Implements**:
- `cpt-orm-algo-postgresql-connector-render-sql`

**Touches**:
- Entities: `connector_trait<PostgreSQLDB>` render path
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Capability Tags

- [ ] `p2` - **ID**: `cpt-orm-dod-postgresql-connector-capability-tags`

The system **MUST** declare the appropriate capability tags as nested types within `connector_trait<PostgreSQLDB>`: at minimum `supports_joins`, `supports_transactions`, `supports_aggregation`. Capability tags not declared **MUST NOT** be declared.

**Implements**:
- `cpt-orm-feature-postgresql-connector`

**Touches**:
- Entities: `connector_trait<PostgreSQLDB>`
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Test Coverage Guidance

**Unit test targets**:
- `connector_trait<PostgreSQLDB>::execute()` with a mock `PGconn*` handle that stubs `PQprepare`, `PQexecPrepared`, and `PQclear`.
- `$N`-parameter rendering: verify correct `$1`, `$2`, … assignment and that indexed-placeholder reuse does not duplicate entries in the values array.
- Timeout/error propagation: mock `PQexecPrepared` returning NULL and verify `orm::result` carries the error string.

**Integration test targets**:
- Full round-trip against a live PostgreSQL instance (SELECT, INSERT, UPDATE, DELETE).
- Indexed-placeholder reuse end-to-end: one `_1` placeholder appearing twice in WHERE produces SQL with identical `$1` at both sites and `nparams = 1`.

**Mock boundaries**: The `PGconn*` handle and libpq function pointers are the primary mock boundaries.

**Test isolation**: Unit tests must not require a live PostgreSQL server; use a thin mock layer over libpq.

---

## 6. Acceptance Criteria

- [ ] `connector_trait<PostgreSQLDB>` compiles with no errors or warnings under C++23 with `-Wall -Wextra`.
- [ ] A SELECT query with positional `Placeholder<T>` parameters executes against a PostgreSQL server and returns the correct rows.
- [ ] A SELECT query using `orm::ph<T, std::placeholders::_1>` reused in two WHERE conditions produces SQL with the same `$1` token at both positions, and `nparams = 1` is passed to `PQexecPrepared`.
- [ ] `PQclear` is called exactly once per `PGresult*`, including on the error path.
- [ ] Accessing a capability tag not declared in `connector_trait<PostgreSQLDB>` produces a `static_assert` compile error.
