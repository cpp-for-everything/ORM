# Feature: Redis Connector

- [ ] `p1` - **ID**: `cpt-orm-featstatus-redis-connector`

- [ ] `p2` - **ID**: `cpt-orm-feature-redis-connector`

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
  - [Store and Retrieve a Cached Entity via Redis](#store-and-retrieve-a-cached-entity-via-redis)
- [3. Processes / Business Logic (CDSL)](#3-processes--business-logic-cdsl)
  - [Entity-to-Redis Key Mapping](#entity-to-redis-key-mapping)
  - [Partial IR Translation](#partial-ir-translation)
  - [Client-Side Materialisation](#client-side-materialisation)
- [4. States](#4-states)
- [5. Definitions of Done](#5-definitions-of-done)
  - [connector\_trait\<RedisDB\> Specialisation](#connector_traitredisd-specialisation)
  - [Key and Hash Field Mapping](#key-and-hash-field-mapping)
  - [Primary-Key-Only WHERE Enforcement](#primary-key-only-where-enforcement)
  - [Hash Field Round-Trip](#hash-field-round-trip)
  - [Capability Tags](#capability-tags)
  - [Test Coverage Guidance](#test-coverage-guidance)
- [6. Acceptance Criteria](#6-acceptance-criteria)

<!-- /toc -->

## 1. Feature Context

### 1.1 Overview

`orm::redis` connector (`connector_trait<RedisDB>`) that maps entity structs onto Redis string keys (single-column entities) or Redis Hash fields (multi-column entities), enabling the ORM to be used as a compile-time-safe caching layer against Redis.

### 1.2 Purpose

Redis is the dominant in-memory key-value store used as a caching layer. This connector allows the ORM's compile-time query structure to enforce consistent key formatting across the codebase — a common source of bugs in hand-written caching code. IR translation is intentionally partial: WHERE predicates are restricted to primary-key equality, and ORDER BY / GROUP BY require explicit opt-in client-side materialisation. The primary use case is cache read/write, not ad-hoc querying.

**Performance**: Hot-path optimisations are not applicable to this connector — Redis is an in-memory store and the primary performance concern is correct key derivation. Wire-level async I/O may be addressed in `FEATURE-wire-protocol` if extended to Redis.

**Reliability**: Retry logic, circuit breakers, and connection failover are not applicable to this connector layer; they are the responsibility of the caller-supplied `redisContext*` handle or a higher-level infrastructure component.

**Known limitations**:
- Client-side materialisation (ORDER BY / GROUP BY) uses `SCAN 0 MATCH prefix:* COUNT 100` iteratively; this is a best-effort pattern and may miss keys or return stale results under concurrent writes. It is not intended for production use without careful consideration of Redis keyspace size and consistency requirements.

**Requirements**: DESIGN pending — no PRD or DESIGN artifact registered yet.

**Principles**: DESIGN pending.

### 1.3 Actors

| Actor | Role in Feature |
|-------|-----------------|
| Developer | Uses `db<RedisDB>` to store and retrieve cached entities; queries are restricted to primary-key lookups. |

### 1.4 References

- **PRD**: DESIGN pending — no PRD registered.
- **Design**: DESIGN pending — no DESIGN artifact registered.
- **Dependencies**: hiredis (Redis C client library).
- **Security boundary**: This connector accepts a caller-supplied `redisContext*` connection handle. Authentication (AUTH command), TLS configuration, and credential management are entirely the caller's responsibility; this connector does not handle or store credentials.

### 1.5 Scope & Boundaries

**In scope**:
- Mapping single-column entities to Redis string keys and multi-column entities to Redis Hash fields.
- Compile-time enforcement that WHERE predicates are restricted to primary-key equality.
- Client-side materialisation for ORDER BY / GROUP BY with explicit opt-in flag.
- Capability tag declarations applicable to Redis (no `supports_joins`, `supports_transactions`, `supports_aggregation`).

**Out of scope**:
- Connection management, pooling, and authentication — see `FEATURE-thread-safety`.
- Async I/O and wire-level optimisations — see `FEATURE-wire-protocol`.
- Ad-hoc querying, secondary index lookups, and full-text search.
- Redis Streams, Pub/Sub, sorted sets, and other Redis data structures beyond strings and Hashes.

### 1.6 Configuration

Not applicable — this connector has no runtime configuration options or feature flags beyond the `client_side_materialisation` compile-time flag. All connector behaviour is determined at compile time by the query IR and the `connector_trait<RedisDB>` specialisation.

### Non-Applicability Declarations

- **UX**: Not applicable — this is a C++ library connector with no user-facing interface.
- **COMPL (Compliance)**: Not applicable — no regulatory or standards-compliance requirements apply to this connector layer. Data privacy (PII/GDPR): Not applicable at the connector layer — the connector transmits only what the caller supplies; PII handling is the application's responsibility.
- **OPS (Observability)**: Not applicable — logging, metrics, and tracing are not applicable to this connector; instrumentation is the responsibility of the caller.
- **OPS (Rollout/Rollback)**: Not applicable — this is a C++ library feature distributed as source; rollout and rollback are governed by the consuming application's dependency management.
- **SEC (Authentication/Authorization)**: Not applicable to this connector — authentication is delegated to the caller-supplied `redisContext*` handle (see §1.4 Security boundary).
- **PERF (Hot paths / N+1 prevention)**: Not applicable to this connector — Redis is an in-memory store; latency-sensitive path optimisations are outside this connector's scope.
- **REL (Retry / circuit breaker)**: Not applicable to this connector — retry and failover are the responsibility of the caller or infrastructure layer.

---

## 2. Actor Flows (CDSL)

### Store and Retrieve a Cached Entity via Redis

- [ ] `p1` - **ID**: `cpt-orm-flow-redis-connector-cache-entity`

**Actor**: Developer

**Success Scenarios**:
- Developer inserts a single-column entity; connector stores it as a Redis string with a compile-time-derived key prefix.
- Developer inserts a multi-column entity; connector stores fields as a Redis Hash.
- Developer queries by primary key; connector retrieves the string or Hash and hydrates the entity.

**Error Scenarios**:
- Redis server unreachable: `execute()` propagates hiredis error through `orm::result` error state.
- Query contains a non-primary-key WHERE predicate: `static_assert` compile error prevents the query from compiling.
- Network call exceeds caller-configured connection timeout: hiredis returns a timeout error; connector propagates it through `orm::result` error state without retry.

**Steps**:
1. [ ] - `p1` - Developer instantiates `db<RedisDB>` with a valid `redisContext*` handle. - `inst-redis-connect`
2. [ ] - `p1` - Developer constructs a compile-time `insert(entity_value)` or `select(...)` query with a primary-key equality WHERE clause. - `inst-redis-build-query`
3. [ ] - `p1` - Developer calls `db.execute(query, runtime_args...)`. - `inst-redis-call-execute`
4. [ ] - `p1` - Connector invokes partial IR translation to verify the WHERE constraint at compile time. - `inst-redis-translate-ir`
5. [ ] - `p1` - Connector invokes entity-to-Redis key mapping to derive the key string. - `inst-redis-map-key`
6. [ ] - `p1` - **IF** operation is INSERT and entity is single-column — issue `SET key value` via hiredis. - `inst-redis-set`
7. [ ] - `p1` - **IF** operation is INSERT and entity is multi-column — issue `HSET key field1 val1 field2 val2 …` via hiredis. - `inst-redis-hset`
8. [ ] - `p1` - **IF** operation is SELECT and entity is single-column — issue `GET key`, hydrate scalar value. - `inst-redis-get`
9. [ ] - `p1` - **IF** operation is SELECT and entity is multi-column — issue `HGETALL key`, hydrate Hash fields into entity. - `inst-redis-hgetall`
10. [ ] - `p1` - **IF** hiredis returns a timeout or network error — propagate error through `orm::result` error state and **RETURN**. - `inst-redis-timeout`
11. [ ] - `p1` - **RETURN** `orm::result<Row...>` (or affected-count result for INSERT) to caller. - `inst-redis-return`

---

## 3. Processes / Business Logic (CDSL)

### Entity-to-Redis Key Mapping

- [ ] `p2` - **ID**: `cpt-orm-algo-redis-connector-map-key`

**Input**: Entity type (compile-time), primary-key runtime value.

**Output**: Redis key string with compile-time-derived type prefix and runtime primary-key suffix.

**Steps**:
1. [ ] - `p1` - Derive the compile-time key prefix from the entity type name (e.g. `"User:"` for a `User` entity). - `inst-key-prefix`
2. [ ] - `p1` - Append the runtime primary-key value as a string to the prefix. - `inst-key-pk-append`
3. [ ] - `p1` - **IF** entity is single-column — the key maps directly to a Redis string value. - `inst-key-single-col`
4. [ ] - `p1` - **IF** entity is multi-column — the key maps to a Redis Hash; each non-primary-key column becomes a Hash field. - `inst-key-multi-col`
5. [ ] - `p1` - **RETURN** the key string. - `inst-key-return`

### Partial IR Translation

- [ ] `p2` - **ID**: `cpt-orm-algo-redis-connector-translate-ir`

**Input**: Query IR (compile-time) — WHERE predicate, ORDER BY, GROUP BY nodes.

**Output**: Compile success, or `static_assert` failure if unsupported IR constructs are present.

**Steps**:
1. [ ] - `p1` - Inspect the WHERE predicate IR at compile time. - `inst-ir-inspect-where`
2. [ ] - `p1` - **IF** WHERE predicate is not a single primary-key equality — emit `static_assert` failure: "RedisDB connector supports only primary-key equality WHERE predicates". - `inst-ir-assert-pk-only`
3. [ ] - `p1` - Inspect ORDER BY and GROUP BY IR nodes. - `inst-ir-inspect-order`
4. [ ] - `p1` - **IF** ORDER BY or GROUP BY is present and the `client_side_materialisation` flag is not set — emit `static_assert` failure: "RedisDB: ORDER BY / GROUP BY require client-side materialisation; set client_side_materialisation = true". - `inst-ir-assert-no-order`
5. [ ] - `p1` - **RETURN** compile success when all constraints are satisfied. - `inst-ir-return`

### Client-Side Materialisation

- [ ] `p2` - **ID**: `cpt-orm-algo-redis-connector-materialise`

**Input**: Redis key pattern, ORDER BY / GROUP BY spec, `client_side_materialisation = true` flag.

**Output**: Sorted or grouped result set, materialised in the client after a Redis SCAN.

**Steps**:
1. [ ] - `p1` - Issue `SCAN 0 MATCH prefix:* COUNT 100` iteratively until cursor returns 0, collecting all matching keys. - `inst-mat-scan`
2. [ ] - `p1` - **FOR EACH** collected key — issue GET or HGETALL and hydrate the entity. - `inst-mat-fetch`
3. [ ] - `p1` - Apply ORDER BY comparator to the in-memory entity collection. - `inst-mat-sort`
4. [ ] - `p1` - **IF** GROUP BY is present — partition the sorted collection by the grouping key. - `inst-mat-group`
5. [ ] - `p1` - **RETURN** the materialised result range. - `inst-mat-return`

---

## 4. States

Not applicable — the Redis connector is a stateless transform from query IR to hiredis commands. Connection lifecycle is managed by the caller-supplied `redisContext*` handle.

---

## 5. Definitions of Done

### `connector_trait<RedisDB>` Specialisation

- [ ] `p1` - **ID**: `cpt-orm-dod-redis-connector-trait-specialisation`

The system **MUST** provide a complete `connector_trait<RedisDB>` specialisation that compiles cleanly with C++23 and satisfies the `ConnectorTrait` concept required by the ORM core.

**Implements**:
- `cpt-orm-flow-redis-connector-cache-entity`
- `cpt-orm-algo-redis-connector-map-key`

**Touches**:
- Entities: `connector_trait<RedisDB>`, `RedisDB` tag type
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Key and Hash Field Mapping

- [ ] `p1` - **ID**: `cpt-orm-dod-redis-connector-key-mapping`

The system **MUST** derive Redis key strings from the entity type prefix and the runtime primary-key value, mapping single-column entities to Redis strings and multi-column entities to Redis Hashes, with round-trip fidelity (SET then GET produces the original value).

**Implements**:
- `cpt-orm-algo-redis-connector-map-key`

**Touches**:
- Entities: `connector_trait<RedisDB>` key derivation path
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Primary-Key-Only WHERE Enforcement

- [ ] `p1` - **ID**: `cpt-orm-dod-redis-connector-pk-only-where`

The system **MUST** produce a `static_assert` compile error when a query targeting `RedisDB` contains a WHERE predicate that is not a simple primary-key equality comparison.

**Implements**:
- `cpt-orm-algo-redis-connector-translate-ir`

**Touches**:
- Entities: `connector_trait<RedisDB>` IR validation
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Hash Field Round-Trip

- [ ] `p1` - **ID**: `cpt-orm-dod-redis-connector-hash-field-mapping`

The system **MUST** correctly store all non-primary-key fields of a multi-column entity as Redis Hash fields via `HSET` and retrieve them via `HGETALL`, hydrating them back into the entity with no data loss.

**Implements**:
- `cpt-orm-flow-redis-connector-cache-entity`

**Touches**:
- Entities: `connector_trait<RedisDB>` Hash path
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Capability Tags

- [ ] `p2` - **ID**: `cpt-orm-dod-redis-connector-capability-tags`

The system **MUST** declare only the capability tags applicable to Redis within `connector_trait<RedisDB>`. Tags such as `supports_joins`, `supports_transactions`, and `supports_aggregation` **MUST NOT** be declared, producing a `static_assert` compile error if referenced.

**Implements**:
- `cpt-orm-feature-redis-connector`

**Touches**:
- Entities: `connector_trait<RedisDB>`
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Test Coverage Guidance

**Unit test targets**:
- `connector_trait<RedisDB>::execute()` with a mock `redisContext*` handle that stubs `redisCommand`.
- Key derivation: verify type-prefix + primary-key-value formatting for single-column and multi-column entities.
- Compile-time WHERE enforcement: verify `static_assert` fires for non-primary-key predicates.
- Timeout/error propagation: mock hiredis returning `REDIS_ERR` and verify `orm::result` carries the error.

**Integration test targets**:
- Full round-trip against a live Redis instance (SET/GET for single-column, HSET/HGETALL for multi-column).
- Client-side materialisation: SCAN + ORDER BY on a small key set.

**Mock boundaries**: The `redisContext*` handle and hiredis `redisCommand` function pointer are the primary mock boundaries.

**Test isolation**: Unit tests must not require a live Redis server; use a thin mock layer over hiredis.

---

## 6. Acceptance Criteria

- [ ] `connector_trait<RedisDB>` compiles with no errors or warnings under C++23 with `-Wall -Wextra`.
- [ ] A single-column entity inserted via `db<RedisDB>` can be retrieved with the same primary key and produces the original value.
- [ ] A multi-column entity stored via `HSET` can be retrieved via `HGETALL` and hydrates correctly into the entity struct.
- [ ] A SELECT query with a non-primary-key WHERE predicate targeting `RedisDB` produces a `static_assert` compile error.
- [ ] Referencing `supports_joins` in `connector_trait<RedisDB>` produces a `static_assert` compile error.
