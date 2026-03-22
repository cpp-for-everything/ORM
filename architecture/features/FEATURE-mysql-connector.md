# Feature: MySQL / MariaDB Connector

- [ ] `p1` - **ID**: `cpt-orm-featstatus-mysql-connector`

- [ ] `p2` - **ID**: `cpt-orm-feature-mysql-connector`

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
  - [Execute a Query Against MySQL](#execute-a-query-against-mysql)
- [3. Processes / Business Logic (CDSL)](#3-processes--business-logic-cdsl)
  - [IR-to-MySQL-SQL Rendering](#ir-to-mysql-sql-rendering)
  - [Indexed-Placeholder Rewrite](#indexed-placeholder-rewrite)
- [4. States](#4-states)
- [5. Definitions of Done](#5-definitions-of-done)
  - [connector\_trait\<MySQLDB\> Specialisation](#connector_traitmysqldb-specialisation)
  - [Prepared-Statement Lifecycle](#prepared-statement-lifecycle)
  - [Indexed Placeholder Rewrite](#indexed-placeholder-rewrite-1)
  - [Capability Tags](#capability-tags)
  - [Test Coverage Guidance](#test-coverage-guidance)
- [6. Acceptance Criteria](#6-acceptance-criteria)

<!-- /toc -->

## 1. Feature Context

### 1.1 Overview

`connector_trait<MySQLDB>` specialisation that translates the ORM compile-time query IR to MySQL C API prepared-statement calls (`mysql_stmt_prepare`, `mysql_stmt_bind_param`, `mysql_stmt_execute`), enabling the ORM to target MySQL and MariaDB databases without any runtime SQL string construction.

### 1.2 Purpose

MySQL and MariaDB are the most widely deployed open-source relational databases. This connector allows the ORM library to target them while preserving all compile-time query safety guarantees. Because MySQL does not support native indexed-parameter syntax (`?NNN`), the connector implements a rewrite pass that maps indexed placeholders to positional `?` tokens by duplicating arguments at each reuse site.

**Injection prevention**: SQL injection is prevented by construction — all query parameters are bound via `mysql_stmt_bind_param` using prepared statements; no user-supplied data is ever concatenated into the SQL string.

**Performance**: Hot-path optimisations (async I/O, zero-copy result parsing, batch INSERT) are delegated to `FEATURE-wire-protocol`. This connector's responsibility is correct translation of the compile-time IR to MySQL C API calls.

**Reliability**: Retry logic, circuit breakers, and connection failover are not applicable to this connector layer; they are the responsibility of the caller-supplied connection handle or a higher-level infrastructure component.

**Requirements**: DESIGN pending — no PRD or DESIGN artifact registered yet.

**Principles**: DESIGN pending.

### 1.3 Actors

| Actor | Role in Feature |
|-------|-----------------|
| Developer | Specialises `connector_trait<MySQLDB>`, constructs compile-time queries, and calls `execute()` to run them against a MySQL/MariaDB server. |

### 1.4 References

- **PRD**: DESIGN pending — no PRD registered.
- **Design**: DESIGN pending — no DESIGN artifact registered.
- **Dependencies**: MySQL C API (`libmysqlclient`); MariaDB Connector/C (API-compatible).
- **Security boundary**: This connector accepts a caller-supplied `MYSQL*` connection handle. Authentication, TLS configuration, and credential management are entirely the caller's responsibility; this connector does not handle or store credentials.

### 1.5 Scope & Boundaries

**In scope**:
- Translation of the ORM compile-time query IR to MySQL C API prepared-statement calls.
- Indexed-placeholder rewrite pass (`?NNN` → positional `?` with argument duplication).
- Result hydration into `orm::result<Row...>` lazy ranges.
- Capability tag declarations (`supports_joins`, `supports_transactions`).

**Out of scope**:
- Connection management, pooling, and authentication — see `FEATURE-thread-safety`.
- Async I/O and wire-level optimisations — see `FEATURE-wire-protocol`.
- Schema introspection and DDL generation — see `FEATURE-schema-migration`.
- Support for MySQL-specific extensions (JSON columns, spatial types) beyond the core ORM type system.

### 1.6 Configuration

Not applicable — this connector has no runtime configuration options or feature flags. All connector behaviour is determined at compile time by the query IR and the `connector_trait<MySQLDB>` specialisation.

### Non-Applicability Declarations

- **UX**: Not applicable — this is a C++ library connector with no user-facing interface.
- **COMPL (Compliance)**: Not applicable — no regulatory or standards-compliance requirements apply to this connector layer. Data privacy (PII/GDPR): Not applicable at the connector layer — the connector transmits only what the caller supplies; PII handling is the application's responsibility.
- **OPS (Observability)**: Not applicable — logging, metrics, and tracing are not applicable to this connector; instrumentation is the responsibility of the caller.
- **OPS (Rollout/Rollback)**: Not applicable — this is a C++ library feature distributed as source; rollout and rollback are governed by the consuming application's dependency management.
- **SEC (Authentication/Authorization)**: Not applicable to this connector — authentication is delegated to the caller-supplied `MYSQL*` handle (see §1.4 Security boundary).
- **PERF (Hot paths / N+1 prevention)**: Not applicable to this connector — performance optimisations are delegated to `FEATURE-wire-protocol`.
- **REL (Retry / circuit breaker)**: Not applicable to this connector — retry and failover are the responsibility of the caller or infrastructure layer.

---

## 2. Actor Flows (CDSL)

### Execute a Query Against MySQL

- [ ] `p1` - **ID**: `cpt-orm-flow-mysql-connector-execute-query`

**Actor**: Developer

**Success Scenarios**:
- Developer executes a compile-time SELECT query with positional parameters against a MySQL server and receives a typed result range.
- Developer executes a compile-time SELECT query with indexed (reused) parameters; the connector rewrites them to positional duplicates transparently.

**Error Scenarios**:
- MySQL server unreachable: `execute()` propagates connection error.
- Parameter bind type mismatch detected at MySQL C API level: connector surfaces error through return type.
- Network call exceeds caller-configured connection timeout: MySQL C API returns a timeout error; connector propagates it through `orm::result` error state without retry.

**Steps**:
1. [ ] - `p1` - Developer instantiates `db<MySQLDB>` with a valid MySQL connection handle. - `inst-mysql-connect`
2. [ ] - `p1` - Developer constructs a compile-time query using `select(...)`, `.where(...)`, `.limit(...)`, etc. — query IR fully encoded in template parameters. - `inst-mysql-build-query`
3. [ ] - `p1` - Developer calls `db.execute(query, runtime_args...)`. - `inst-mysql-call-execute`
4. [ ] - `p1` - Connector invokes IR-to-MySQL-SQL rendering process to produce SQL string and positional bind list. - `inst-mysql-render`
5. [ ] - `p1` - Connector calls `mysql_stmt_prepare(stmt, sql, sql_len)` with the rendered SQL. - `inst-mysql-prepare`
6. [ ] - `p1` - Connector calls `mysql_stmt_bind_param(stmt, bind_array)` with the reordered/duplicated parameter array. - `inst-mysql-bind`
7. [ ] - `p1` - Connector calls `mysql_stmt_execute(stmt)`. - `inst-mysql-exec`
8. [ ] - `p1` - **IF** `mysql_stmt_execute` returns a timeout or network error — propagate error through `orm::result` error state and **RETURN**. - `inst-mysql-timeout`
9. [ ] - `p1` - **IF** execution succeeds — bind result buffers and return lazy `orm::result` range. - `inst-mysql-return-result`
10. [ ] - `p1` - **ELSE** — propagate MySQL error code through `orm::result` error state. - `inst-mysql-error`
11. [ ] - `p1` - **RETURN** `orm::result<Row...>` to caller. - `inst-mysql-return`

---

## 3. Processes / Business Logic (CDSL)

### IR-to-MySQL-SQL Rendering

- [ ] `p2` - **ID**: `cpt-orm-algo-mysql-connector-render-sql`

**Input**: Compile-time query IR (constexpr structure) + runtime parameter value list.

**Output**: MySQL SQL string with `?` positional placeholders + ordered `MYSQL_BIND` array.

**Steps**:
1. [ ] - `p1` - Walk query IR tree top-down: SELECT columns → FROM clause → JOIN clauses → WHERE predicate tree → ORDER BY → GROUP BY → LIMIT. - `inst-render-walk-ir`
2. [ ] - `p1` - For each IR node emit the corresponding SQL token or clause fragment into a runtime string buffer. - `inst-render-emit-token`
3. [ ] - `p1` - **FOR EACH** `Placeholder<T>` node in the IR — emit `?` into the SQL string and record the parameter position. - `inst-render-positional-ph`
4. [ ] - `p1` - **FOR EACH** indexed placeholder `orm::ph<T, std::placeholders::_N>` — invoke the indexed-placeholder rewrite process to expand into one or more `?` tokens. - `inst-render-indexed-ph`
5. [ ] - `p1` - Construct the `MYSQL_BIND` array in the order matching the `?` positions in the rendered SQL string. - `inst-render-bind-array`
6. [ ] - `p1` - **RETURN** `{sql_string, bind_array}` pair to the caller. - `inst-render-return`

### Indexed-Placeholder Rewrite

- [ ] `p2` - **ID**: `cpt-orm-algo-mysql-connector-rewrite-indexed-ph`

**Input**: Indexed placeholder list extracted from query IR; map from placeholder index to runtime argument value.

**Output**: Expanded positional `?` list with the same runtime argument duplicated at each reuse site in the `MYSQL_BIND` array.

**Steps**:
1. [ ] - `p1` - Scan the IR for all occurrences of each `orm::ph<T, std::placeholders::_N>` by index `N`. - `inst-rewrite-scan`
2. [ ] - `p1` - **FOR EACH** occurrence — emit one `?` into the SQL string at that position. - `inst-rewrite-emit-question-mark`
3. [ ] - `p1` - **FOR EACH** occurrence — append the corresponding runtime argument value to the `MYSQL_BIND` array at the matching position. - `inst-rewrite-append-bind`
4. [ ] - `p1` - **RETURN** the expanded bind array. - `inst-rewrite-return`

---

## 4. States

Not applicable — the MySQL connector is a stateless transform from query IR to C API calls. Connection lifecycle is managed by the caller-supplied `MYSQL*` handle.

---

## 5. Definitions of Done

### `connector_trait<MySQLDB>` Specialisation

- [ ] `p1` - **ID**: `cpt-orm-dod-mysql-connector-trait-specialisation`

The system **MUST** provide a complete `connector_trait<MySQLDB>` specialisation that compiles cleanly with C++23 and satisfies the `ConnectorTrait` concept required by the ORM core.

**Implements**:
- `cpt-orm-flow-mysql-connector-execute-query`
- `cpt-orm-algo-mysql-connector-render-sql`

**Touches**:
- Entities: `connector_trait<MySQLDB>`, `MySQLDB` tag type
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Prepared-Statement Lifecycle

- [ ] `p1` - **ID**: `cpt-orm-dod-mysql-connector-prepared-stmt`

The system **MUST** correctly manage the full MySQL prepared-statement lifecycle: `mysql_stmt_init` → `mysql_stmt_prepare` → `mysql_stmt_bind_param` → `mysql_stmt_execute` → `mysql_stmt_bind_result` → `mysql_stmt_fetch` → `mysql_stmt_close`, including RAII cleanup on destruction.

**Implements**:
- `cpt-orm-flow-mysql-connector-execute-query`

**Touches**:
- Entities: `MySQLDB` connector internals
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Indexed Placeholder Rewrite

- [ ] `p1` - **ID**: `cpt-orm-dod-mysql-connector-indexed-ph-rewrite`

The system **MUST** correctly rewrite indexed placeholders (`orm::ph<T, std::placeholders::_N>`) to positional `?` tokens, duplicating the runtime argument in `MYSQL_BIND` at each reuse site, such that the bound values match the SQL string positions exactly.

**Implements**:
- `cpt-orm-algo-mysql-connector-rewrite-indexed-ph`

**Touches**:
- Entities: `connector_trait<MySQLDB>` render path
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Capability Tags

- [ ] `p2` - **ID**: `cpt-orm-dod-mysql-connector-capability-tags`

The system **MUST** declare the appropriate capability tags as nested types within `connector_trait<MySQLDB>`: at minimum `supports_joins`, `supports_transactions`. Capability tags not declared **MUST NOT** be declared.

**Implements**:
- `cpt-orm-feature-mysql-connector`

**Touches**:
- Entities: `connector_trait<MySQLDB>`
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Test Coverage Guidance

**Unit test targets**:
- `connector_trait<MySQLDB>::execute()` with a mock `MYSQL*` handle that stubs `mysql_stmt_prepare`, `mysql_stmt_bind_param`, `mysql_stmt_execute`, `mysql_stmt_fetch`, and `mysql_stmt_close`.
- Indexed-placeholder rewrite algorithm: verify `MYSQL_BIND` array positions match the expanded `?` positions in the SQL string for various reuse patterns.
- Timeout/error propagation: mock `mysql_stmt_execute` returning `CR_SERVER_LOST` and verify `orm::result` carries the error code.

**Integration test targets**:
- Full round-trip against a live MySQL/MariaDB instance (SELECT, INSERT, UPDATE, DELETE).
- Indexed-placeholder reuse end-to-end: one `_1` placeholder appearing twice in WHERE produces correct results.

**Mock boundaries**: The `MYSQL*` handle is the primary mock boundary; all MySQL C API function pointers should be injectable for unit testing.

**Test isolation**: Unit tests must not require a live MySQL server; use a thin mock layer over the MySQL C API.

---

## 6. Acceptance Criteria

- [ ] `connector_trait<MySQLDB>` compiles with no errors or warnings under C++23 with `-Wall -Wextra`.
- [ ] A SELECT query with positional `Placeholder<T>` parameters executes against a MySQL server and returns the correct rows.
- [ ] A SELECT query using `orm::ph<T, std::placeholders::_1>` reused in two WHERE conditions executes correctly; the bound value appears at both positions in the `MYSQL_BIND` array.
- [ ] `mysql_stmt_close` is called exactly once per prepared statement, including on the error path.
- [ ] Accessing a capability tag not declared in `connector_trait<MySQLDB>` produces a `static_assert` compile error.
