# Feature: MongoDB Connector

- [ ] `p1` - **ID**: `cpt-orm-featstatus-mongodb-connector`

- [ ] `p2` - **ID**: `cpt-orm-feature-mongodb-connector`

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
  - [Execute a Select Query Against MongoDB](#execute-a-select-query-against-mongodb)
- [3. Processes / Business Logic (CDSL)](#3-processes--business-logic-cdsl)
  - [IR-to-BSON Filter Rendering](#ir-to-bson-filter-rendering)
  - [BSON Projection Rendering](#bson-projection-rendering)
  - [Result Hydration from libmongoc Cursor](#result-hydration-from-libmongoc-cursor)
- [4. States](#4-states)
- [5. Definitions of Done](#5-definitions-of-done)
  - [connector\_trait\<MongoDB\> Specialisation](#connector_traitmongodb-specialisation)
  - [BSON Filter Rendering](#bson-filter-rendering)
  - [BSON Projection Rendering](#bson-projection-rendering-1)
  - [ObjectID Type Mapping](#objectid-type-mapping)
  - [Capability Tags](#capability-tags)
  - [Test Coverage Guidance](#test-coverage-guidance)
- [6. Acceptance Criteria](#6-acceptance-criteria)

<!-- /toc -->

## 1. Feature Context

### 1.1 Overview

`connector_trait<MongoDB>` specialisation that translates the ORM compile-time query IR to BSON filter and projection documents via the MongoDB C Driver (`libmongoc`), enabling the ORM to target MongoDB document collections with full compile-time query structure.

### 1.2 Purpose

MongoDB is the dominant document-oriented database. This connector maps the ORM's relational-superset query IR onto BSON operators: WHERE predicate trees become `$and`/`$or`/`$eq`/`$gt` BSON documents, selected columns become BSON projection documents, and result rows are hydrated into C++ entity instances. BSON-specific types (`bson_oid_t`, `int64_t`) are handled through the ORM type system.

**Performance**: Hot-path optimisations (async I/O, zero-copy result parsing, batch INSERT) are delegated to `FEATURE-wire-protocol`. This connector's responsibility is correct translation of the compile-time IR to `libmongoc` calls.

**Reliability**: Retry logic, circuit breakers, and connection failover are not applicable to this connector layer; they are the responsibility of the caller-supplied `mongoc_collection_t*` handle or a higher-level infrastructure component.

**Requirements**: DESIGN pending — no PRD or DESIGN artifact registered yet.

**Principles**: DESIGN pending.

### 1.3 Actors

| Actor | Role in Feature |
|-------|-----------------|
| Developer | Specialises `connector_trait<MongoDB>`, constructs compile-time queries, and calls `execute()` to run them against a MongoDB deployment via `libmongoc`. |

### 1.4 References

- **PRD**: DESIGN pending — no PRD registered.
- **Design**: DESIGN pending — no DESIGN artifact registered.
- **Dependencies**: MongoDB C Driver (`libmongoc`); BSON library (`libbson`).
- **Security boundary**: This connector accepts a caller-supplied `mongoc_collection_t*` handle. Authentication, TLS configuration, and credential management are entirely the caller's responsibility; this connector does not handle or store credentials.

### 1.5 Scope & Boundaries

**In scope**:
- Translation of the ORM compile-time WHERE predicate IR to BSON filter documents.
- BSON projection document rendering from the query column list.
- Result hydration from `libmongoc` cursor into `orm::result<Row...>` lazy ranges.
- ObjectID type mapping (`bson_oid_t` ↔ entity ObjectID field).
- Capability tag declarations applicable to MongoDB.

**Out of scope**:
- Connection management, pooling, and authentication — see `FEATURE-thread-safety`.
- Async I/O and wire-level optimisations — see `FEATURE-wire-protocol`.
- Schema management (MongoDB is schema-free; DDL migration not applicable).
- MongoDB aggregation pipeline, change streams, and gridFS operations beyond the core ORM query IR.

### 1.6 Configuration

Not applicable — this connector has no runtime configuration options or feature flags. All connector behaviour is determined at compile time by the query IR and the `connector_trait<MongoDB>` specialisation.

### Non-Applicability Declarations

- **UX**: Not applicable — this is a C++ library connector with no user-facing interface.
- **COMPL (Compliance)**: Not applicable — no regulatory or standards-compliance requirements apply to this connector layer. Data privacy (PII/GDPR): Not applicable at the connector layer — the connector transmits only what the caller supplies; PII handling is the application's responsibility.
- **OPS (Observability)**: Not applicable — logging, metrics, and tracing are not applicable to this connector; instrumentation is the responsibility of the caller.
- **OPS (Rollout/Rollback)**: Not applicable — this is a C++ library feature distributed as source; rollout and rollback are governed by the consuming application's dependency management.
- **SEC (Authentication/Authorization)**: Not applicable to this connector — authentication is delegated to the caller-supplied `mongoc_collection_t*` handle (see §1.4 Security boundary).
- **PERF (Hot paths / N+1 prevention)**: Not applicable to this connector — performance optimisations are delegated to `FEATURE-wire-protocol`.
- **REL (Retry / circuit breaker)**: Not applicable to this connector — retry and failover are the responsibility of the caller or infrastructure layer.

---

## 2. Actor Flows (CDSL)

### Execute a Select Query Against MongoDB

- [ ] `p1` - **ID**: `cpt-orm-flow-mongodb-connector-execute-select`

**Actor**: Developer

**Success Scenarios**:
- Developer executes a compile-time SELECT with WHERE predicates and receives a lazy result range of hydrated C++ entity instances.
- Developer selects a subset of columns; the connector emits a BSON projection that restricts the fields returned by MongoDB.

**Error Scenarios**:
- MongoDB server unreachable: `execute()` propagates `mongoc_error_t` details through `orm::result` error state.
- BSON document construction fails due to type mismatch: error surfaced at connector boundary.
- Network call exceeds caller-configured socket timeout: `libmongoc` sets a cursor error; connector propagates it through `orm::result` error state without retry.

**Steps**:
1. [ ] - `p1` - Developer instantiates `db<MongoDB>` with a valid `mongoc_collection_t*` handle. - `inst-mongo-connect`
2. [ ] - `p1` - Developer constructs a compile-time query using `select(...)`, `.where(...)` — IR fully encoded in template parameters. - `inst-mongo-build-query`
3. [ ] - `p1` - Developer calls `db.execute(query, runtime_args...)`. - `inst-mongo-call-execute`
4. [ ] - `p1` - Connector invokes BSON filter rendering to produce a `bson_t*` filter document from the WHERE predicate IR. - `inst-mongo-render-filter`
5. [ ] - `p1` - Connector invokes BSON projection rendering to produce a `bson_t*` projection document from the selected column list. - `inst-mongo-render-projection`
6. [ ] - `p1` - Connector calls `mongoc_collection_find_with_opts(collection, filter, opts_with_projection, NULL)` to obtain a `mongoc_cursor_t*`. - `inst-mongo-find`
7. [ ] - `p1` - **IF** cursor creation fails or `mongoc_cursor_error` indicates a timeout — propagate error through `orm::result` and **RETURN**. - `inst-mongo-find-error`
8. [ ] - `p1` - Connector wraps cursor in RAII holder and invokes result hydration process on each document. - `inst-mongo-hydrate`
9. [ ] - `p1` - **RETURN** lazy `orm::result<Row...>` range to caller. - `inst-mongo-return`

---

## 3. Processes / Business Logic (CDSL)

### IR-to-BSON Filter Rendering

- [ ] `p2` - **ID**: `cpt-orm-algo-mongodb-connector-render-bson`

**Input**: WHERE predicate IR tree (compile-time structure) + runtime parameter values.

**Output**: `bson_t*` filter document using BSON operators matching the predicate semantics.

**Steps**:
1. [ ] - `p1` - Walk the WHERE predicate IR tree recursively. - `inst-bson-walk-tree`
2. [ ] - `p1` - **IF** node is `AND` conjunction — open a `$and` BSON array, recurse into children, close array. - `inst-bson-and`
3. [ ] - `p1` - **IF** node is `OR` disjunction — open a `$or` BSON array, recurse into children, close array. - `inst-bson-or`
4. [ ] - `p1` - **IF** node is equality comparison — emit `{ "field": { "$eq": value } }` BSON subdocument. - `inst-bson-eq`
5. [ ] - `p1` - **IF** node is greater-than comparison — emit `{ "field": { "$gt": value } }`. - `inst-bson-gt`
6. [ ] - `p1` - **IF** node is less-than comparison — emit `{ "field": { "$lt": value } }`. - `inst-bson-lt`
7. [ ] - `p1` - Bind runtime parameter values (from `Placeholder<T>` or `orm::ph`) at their respective leaf positions. - `inst-bson-bind-values`
8. [ ] - `p1` - **RETURN** the completed `bson_t*` filter document. - `inst-bson-return`

### BSON Projection Rendering

- [ ] `p2` - **ID**: `cpt-orm-algo-mongodb-connector-render-projection`

**Input**: Selected column list from query IR (compile-time).

**Output**: `bson_t*` projection document specifying which fields MongoDB should include in returned documents.

**Steps**:
1. [ ] - `p1` - Create a new `bson_t` projection document. - `inst-proj-create`
2. [ ] - `p1` - **FOR EACH** selected column in the IR column list — append `{ "column_name": 1 }` to the projection document. - `inst-proj-include-field`
3. [ ] - `p1` - Suppress `_id` field in projection when the entity has no `_id` property mapped (append `{ "_id": 0 }`). - `inst-proj-suppress-id`
4. [ ] - `p1` - **RETURN** the completed `bson_t*` projection document. - `inst-proj-return`

### Result Hydration from libmongoc Cursor

- [ ] `p2` - **ID**: `cpt-orm-algo-mongodb-connector-hydrate-result`

**Input**: `mongoc_cursor_t*` from a successful `mongoc_collection_find_with_opts` call.

**Output**: Lazy range of C++ entity instances.

**Steps**:
1. [ ] - `p1` - Wrap `mongoc_cursor_t*` in RAII holder that calls `mongoc_cursor_destroy` on destruction. - `inst-hydrate-raii`
2. [ ] - `p1` - **FOR EACH** document returned by `mongoc_cursor_next` — obtain `const bson_t* doc`. - `inst-hydrate-next-doc`
3. [ ] - `p1` - **FOR EACH** selected column in the IR — look up the field in `doc` using `bson_iter_find` and the column name string. - `inst-hydrate-iter-field`
4. [ ] - `p1` - Map the BSON value to the corresponding C++ type: `bson_oid_t` → entity ObjectID field; `int64_t` → `int64_t` entity field; UTF-8 string → `std::u8string`; bool → `bool`. - `inst-hydrate-type-map`
5. [ ] - `p1` - Construct the C++ entity instance from the mapped field values. - `inst-hydrate-construct`
6. [ ] - `p1` - **RETURN** the entity instance to the lazy range consumer. - `inst-hydrate-return`

---

## 4. States

Not applicable — the MongoDB connector is a stateless transform from query IR to `libmongoc` calls. Collection and client lifecycle are managed by the caller-supplied `mongoc_collection_t*` handle.

---

## 5. Definitions of Done

### `connector_trait<MongoDB>` Specialisation

- [ ] `p1` - **ID**: `cpt-orm-dod-mongodb-connector-trait-specialisation`

The system **MUST** provide a complete `connector_trait<MongoDB>` specialisation that compiles cleanly with C++23 and satisfies the `ConnectorTrait` concept required by the ORM core.

**Implements**:
- `cpt-orm-flow-mongodb-connector-execute-select`
- `cpt-orm-algo-mongodb-connector-render-bson`

**Touches**:
- Entities: `connector_trait<MongoDB>`, `MongoDB` tag type
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### BSON Filter Rendering

- [ ] `p1` - **ID**: `cpt-orm-dod-mongodb-connector-bson-filter-render`

The system **MUST** correctly translate all supported WHERE predicate node types (AND, OR, equality, greater-than, less-than) to their BSON operator equivalents, with runtime values bound at leaf nodes.

**Implements**:
- `cpt-orm-algo-mongodb-connector-render-bson`

**Touches**:
- Entities: `connector_trait<MongoDB>` render path
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### BSON Projection Rendering

- [ ] `p1` - **ID**: `cpt-orm-dod-mongodb-connector-projection-render`

The system **MUST** emit a BSON projection document that includes exactly the columns named in the query IR's select list and excludes `_id` when not mapped.

**Implements**:
- `cpt-orm-algo-mongodb-connector-render-projection`

**Touches**:
- Entities: `connector_trait<MongoDB>` render path
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### ObjectID Type Mapping

- [ ] `p1` - **ID**: `cpt-orm-dod-mongodb-connector-oid-type-mapping`

The system **MUST** correctly map `bson_oid_t` BSON values to the entity's ObjectID field type during result hydration, and correctly encode the entity's ObjectID field as `bson_oid_t` in filter documents.

**Implements**:
- `cpt-orm-algo-mongodb-connector-hydrate-result`

**Touches**:
- Entities: `connector_trait<MongoDB>` type mapping
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Capability Tags

- [ ] `p2` - **ID**: `cpt-orm-dod-mongodb-connector-capability-tags`

The system **MUST** declare the appropriate capability tags as nested types within `connector_trait<MongoDB>`. Tags not applicable to MongoDB (e.g. `supports_joins` for cross-collection joins) **MUST NOT** be declared, producing a `static_assert` compile error if used.

**Implements**:
- `cpt-orm-feature-mongodb-connector`

**Touches**:
- Entities: `connector_trait<MongoDB>`
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Test Coverage Guidance

**Unit test targets**:
- `connector_trait<MongoDB>::execute()` with a mock `mongoc_collection_t*` handle that stubs `mongoc_collection_find_with_opts` and `mongoc_cursor_next`.
- BSON filter rendering: verify `$and`, `$or`, `$eq`, `$gt`, `$lt` BSON operator output for each supported predicate node type.
- BSON projection rendering: verify inclusion fields and `_id: 0` suppression.
- Timeout/error propagation: mock `mongoc_cursor_error` returning an error and verify `orm::result` carries the error details.

**Integration test targets**:
- Full round-trip against a live MongoDB instance (find with filter, insert).
- ObjectID round-trip: insert entity with `bson_oid_t` field, retrieve by ObjectID, verify hydration.

**Mock boundaries**: The `mongoc_collection_t*` handle and `libmongoc` function pointers are the primary mock boundaries.

**Test isolation**: Unit tests must not require a live MongoDB server; use a thin mock layer over `libmongoc`.

---

## 6. Acceptance Criteria

- [ ] `connector_trait<MongoDB>` compiles with no errors or warnings under C++23 with `-Wall -Wextra`.
- [ ] A SELECT query with a single equality WHERE predicate produces a BSON filter `{ "field": { "$eq": value } }` verified via `bson_as_canonical_extended_json`.
- [ ] A SELECT query with an AND of two predicates produces a `$and` BSON array with two child documents.
- [ ] A SELECT query for two columns produces a BSON projection `{ "col1": 1, "col2": 1, "_id": 0 }` when `_id` is not in the select list.
- [ ] A document with a `bson_oid_t` field is correctly hydrated into the entity's ObjectID property.
- [ ] `mongoc_cursor_destroy` is called exactly once per cursor, including on the error path.
