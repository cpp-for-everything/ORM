# Feature: Thread Safety & Connection Pool

- [ ] `p1` - **ID**: `cpt-orm-featstatus-thread-safety`

- [ ] `p2` - **ID**: `cpt-orm-feature-thread-safety`

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
  - [Acquire a Connection from Pool and Execute a Query](#acquire-a-connection-from-pool-and-execute-a-query)
  - [Wrap Queries in an Atomic Transaction](#wrap-queries-in-an-atomic-transaction)
- [3. Processes / Business Logic (CDSL)](#3-processes--business-logic-cdsl)
  - [Connection Pool Acquire](#connection-pool-acquire)
  - [Compile-Time Capability Check](#compile-time-capability-check)
- [4. States](#4-states)
  - [Connection State Machine](#connection-state-machine)
- [5. Definitions of Done](#5-definitions-of-done)
  - [connection\_pool\<DB, N\> Template](#connection_pooldb-n-template)
  - [Thread-Local Connection Pattern](#thread-local-connection-pattern)
  - [db::transaction() RAII Guard](#dbtransaction-raii-guard)
  - [supports\_concurrent\_execute Capability Tag](#supports_concurrent_execute-capability-tag)
  - [Test Coverage Guidance](#test-coverage-guidance)
- [6. Acceptance Criteria](#6-acceptance-criteria)

<!-- /toc -->

## 1. Feature Context

### 1.1 Overview

Thread safety infrastructure for the ORM: `connection_pool<DB, N>` template providing N independent RAII-guarded connections, thread-local connection pattern for thread-per-request models, `db::transaction()` RAII guard for atomic multi-statement execution, and compile-time `supports_concurrent_execute` capability tagging to enforce connector declarations.

### 1.2 Purpose

The current `db<DB>` handle holds a mutable reference to a single connection object. Concurrent `db::execute()` calls on the same instance cause a race condition on the underlying connection handle. This feature eliminates that race without imposing synchronisation overhead on single-threaded usage — connection pools use exclusive ownership per query; thread-local connections eliminate all synchronisation for thread-per-request models.

**Performance**: The pool acquire path uses a mutex + condition variable; contention is expected to be low because pools are sized to match thread concurrency. Thread-local connections have zero synchronisation overhead. No other hot-path optimisations are in scope for this feature.

**Reliability**: The pool blocks calling threads when all connections are in use; this is by design to prevent unbounded concurrency. No retry logic or circuit breaker is needed — pool exhaustion blocks rather than errors.

**Security**: Connection credentials are embedded in the connection objects supplied by the caller at pool construction time. This feature does not handle, store, or transmit credentials beyond what the caller provides.

**Requirements**: DESIGN pending — no PRD or DESIGN artifact registered yet.

**Principles**: DESIGN pending.

### 1.3 Actors

| Actor | Role in Feature |
|-------|-----------------|
| Developer | Creates a `connection_pool<DB, N>`, acquires RAII guards, executes queries safely across threads, and wraps multi-statement sequences in `db::transaction()` guards. |

### 1.4 References

- **PRD**: DESIGN pending — no PRD registered.
- **Design**: DESIGN pending — no DESIGN artifact registered.
- **Dependencies**: C++ standard library (`<mutex>`, `<condition_variable>`, `<thread_local>`).

### 1.5 Scope & Boundaries

**In scope**:
- `connection_pool<DB, N>` template with RAII `connection_guard<DB>` and blocking acquire.
- Thread-local connection pattern (`thread_local_db<DB>`) for thread-per-request models.
- `db::transaction()` RAII guard with BEGIN/COMMIT/ROLLBACK semantics.
- `supports_concurrent_execute` capability tag enforcement via `static_assert`.

**Out of scope**:
- Network I/O, wire-protocol optimisations — see `FEATURE-wire-protocol`.
- Connector-specific authentication or credential management — handled by the caller-supplied connection object.
- Distributed transactions or two-phase commit.
- Connection health checks and automatic reconnection.

### 1.6 Configuration

Not applicable — pool size `N` is a compile-time template parameter, not a runtime configuration option. All thread-safety behaviour is determined at compile time.

### Non-Applicability Declarations

- **UX**: Not applicable — this is a C++ library feature with no user-facing interface.
- **COMPL (Compliance)**: Not applicable — no regulatory or standards-compliance requirements apply to this feature. Data privacy (PII/GDPR): Not applicable at the connector layer — the connector transmits only what the caller supplies; PII handling is the application's responsibility.
- **OPS (Observability)**: Not applicable — logging and metrics are not applicable to this feature; instrumentation is the responsibility of the caller.
- **OPS (Rollout/Rollback)**: Not applicable — this is a C++ library feature distributed as source; rollout and rollback are governed by the consuming application's dependency management.
- **SEC (Injection prevention)**: Not applicable — this feature manages connection lifecycle only; query construction and injection prevention are handled by individual connectors.
- **PERF (N+1 / hot-path caching)**: Not applicable — this feature provides the connection management layer; N+1 query prevention is a query-construction concern handled upstream.
- **REL (Retry / circuit breaker)**: Not applicable — pool exhaustion causes blocking (by design); no retry or circuit breaker is needed at this layer.

---

## 2. Actor Flows (CDSL)

### Acquire a Connection from Pool and Execute a Query

- [ ] `p1` - **ID**: `cpt-orm-flow-thread-safety-pool-execute`

**Actor**: Developer

**Success Scenarios**:
- Developer acquires a RAII guard from the pool, executes a query on the guarded `db<DB>`, guard destructor returns the connection to the pool.
- Multiple threads concurrently acquire guards; each thread receives an exclusive connection with no data races.

**Error Scenarios**:
- All N pool connections are in use: calling thread blocks until one is returned.
- Connector not declared `supports_concurrent_execute`: `static_assert` compile error at pool instantiation.

**Steps**:
1. [ ] - `p1` - Developer instantiates `connection_pool<DB, N>` with N sets of connection parameters. - `inst-pool-instantiate`
2. [ ] - `p1` - Developer calls `pool.acquire()` to obtain a RAII `connection_guard<DB>`. - `inst-pool-acquire`
3. [ ] - `p1` - **IF** all N connections are `InUse` — calling thread blocks on a condition variable until a connection transitions to `Idle`. - `inst-pool-wait`
4. [ ] - `p1` - Pool assigns the first `Idle` connection, marks it `InUse`, and returns the RAII guard holding exclusive ownership. - `inst-pool-assign`
5. [ ] - `p1` - Developer executes queries via `guard.db().execute(query, args...)`. - `inst-pool-execute`
6. [ ] - `p1` - RAII guard destructor transitions the connection back to `Idle` and notifies the condition variable. - `inst-pool-release`
7. [ ] - `p1` - **RETURN** control to the calling thread after guard is destroyed. - `inst-pool-return`

### Wrap Queries in an Atomic Transaction

- [ ] `p1` - **ID**: `cpt-orm-flow-thread-safety-transaction`

**Actor**: Developer

**Success Scenarios**:
- Developer wraps multiple `execute()` calls in a `db::transaction()` guard; calls `commit()` at the end; all statements commit atomically.
- Developer does not call `commit()`; guard destructor rolls back automatically.

**Error Scenarios**:
- A statement within the transaction fails: developer may call `rollback()` explicitly or allow the guard destructor to roll back.
- Connector does not declare `supports_transactions`: `static_assert` compile error at `db::transaction()` call site.

**Steps**:
1. [ ] - `p1` - Developer calls `auto txn = db.transaction()` to begin a transaction; connector issues `BEGIN` (or equivalent). - `inst-txn-begin`
2. [ ] - `p1` - Developer calls `db.execute(query1, args...)` and `db.execute(query2, args...)` within the transaction scope. - `inst-txn-execute`
3. [ ] - `p1` - **IF** developer calls `txn.commit()` — connector issues `COMMIT`; guard is marked committed. - `inst-txn-commit`
4. [ ] - `p1` - **IF** guard is destroyed without `commit()` having been called — destructor issues `ROLLBACK`. - `inst-txn-auto-rollback`
5. [ ] - `p1` - **RETURN** after the transaction scope exits. - `inst-txn-return`

---

## 3. Processes / Business Logic (CDSL)

### Connection Pool Acquire

- [ ] `p2` - **ID**: `cpt-orm-algo-thread-safety-pool-acquire`

**Input**: Pool of N connections (each in `Idle` or `InUse` state), thread acquire request.

**Output**: RAII `connection_guard<DB>` holding exclusive ownership of one connection.

**Steps**:
1. [ ] - `p1` - Lock the pool mutex. - `inst-acquire-lock`
2. [ ] - `p1` - Scan the pool's connection array for the first entry in `Idle` state. - `inst-acquire-scan`
3. [ ] - `p1` - **IF** found — transition connection to `InUse`, unlock mutex, construct RAII guard wrapping the connection index. - `inst-acquire-found`
4. [ ] - `p1` - **ELSE** — wait on condition variable (releases mutex) until a connection transitions to `Idle`, then retry scan. - `inst-acquire-wait`
5. [ ] - `p1` - Guard destructor: lock mutex, transition connection back to `Idle`, notify_one on condition variable, unlock. - `inst-acquire-release`
6. [ ] - `p1` - **RETURN** the `connection_guard<DB>`. - `inst-acquire-return`

### Compile-Time Capability Check

- [ ] `p2` - **ID**: `cpt-orm-algo-thread-safety-capability-check`

**Input**: `connector_trait<DB>` at compile time.

**Output**: Compile success, or `static_assert` failure if `supports_concurrent_execute` is absent.

**Steps**:
1. [ ] - `p1` - At `connection_pool<DB, N>` template instantiation, check for `connector_trait<DB>::supports_concurrent_execute` nested type. - `inst-cap-check-tag`
2. [ ] - `p1` - **IF** the nested type is absent — emit `static_assert(false, "connection_pool requires connector_trait<DB>::supports_concurrent_execute")`. - `inst-cap-check-assert`
3. [ ] - `p1` - **RETURN** compile success when the tag is present. - `inst-cap-check-return`

---

## 4. States

### Connection State Machine

- [ ] `p2` - **ID**: `cpt-orm-state-thread-safety-connection`

**States**: `Idle`, `InUse`, `Closed`

**Initial State**: `Idle`

**Transitions**:
1. [ ] - `p1` - **FROM** `Idle` **TO** `InUse` **WHEN** `pool.acquire()` assigns the connection to a RAII guard. - `inst-state-idle-to-inuse`
2. [ ] - `p1` - **FROM** `InUse` **TO** `Idle` **WHEN** the RAII guard is destroyed (connection returned to pool). - `inst-state-inuse-to-idle`
3. [ ] - `p1` - **FROM** `InUse` **TO** `Closed` **WHEN** a fatal connection error is detected during `execute()`. - `inst-state-inuse-to-closed`
4. [ ] - `p1` - **FROM** `Idle` **TO** `Closed` **WHEN** the `connection_pool` destructor runs and shuts down all connections. - `inst-state-idle-to-closed`

---

## 5. Definitions of Done

### `connection_pool<DB, N>` Template

- [ ] `p1` - **ID**: `cpt-orm-dod-thread-safety-connection-pool`

The system **MUST** provide a `connection_pool<DB, N>` template that holds N independent connection objects of type `DB`, provides `acquire()` returning a RAII `connection_guard<DB>`, and blocks calling threads when all connections are in use rather than returning a null or invalid guard.

**Implements**:
- `cpt-orm-flow-thread-safety-pool-execute`
- `cpt-orm-algo-thread-safety-pool-acquire`

**Touches**:
- Entities: `connection_pool<DB, N>`, `connection_guard<DB>`
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Thread-Local Connection Pattern

- [ ] `p1` - **ID**: `cpt-orm-dod-thread-safety-thread-local`

The system **MUST** provide a `thread_local_db<DB>` helper (or documented `thread_local` pattern) that creates one connection per thread, accessible via a `db<DB>` handle with zero synchronisation overhead.

**Implements**:
- `cpt-orm-feature-thread-safety`

**Touches**:
- Entities: `thread_local_db<DB>`
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### `db::transaction()` RAII Guard

- [ ] `p1` - **ID**: `cpt-orm-dod-thread-safety-transaction-raii`

The system **MUST** provide a `db::transaction()` RAII guard that issues `BEGIN` on construction, `COMMIT` when `commit()` is called, and `ROLLBACK` on guard destruction if `commit()` was never called. The guard **MUST** produce a `static_assert` compile error if `connector_trait<DB>` does not declare `supports_transactions`.

**Implements**:
- `cpt-orm-flow-thread-safety-transaction`

**Touches**:
- Entities: `db<DB>::transaction_guard`
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### `supports_concurrent_execute` Capability Tag

- [ ] `p1` - **ID**: `cpt-orm-dod-thread-safety-capability-tag`

The system **MUST** enforce via `static_assert` at `connection_pool<DB, N>` instantiation that `connector_trait<DB>` declares a `supports_concurrent_execute` nested type, preventing pool use with connectors that are not declared thread-safe.

**Implements**:
- `cpt-orm-algo-thread-safety-capability-check`

**Touches**:
- Entities: `connection_pool<DB, N>` concept constraint
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Test Coverage Guidance

**Unit test targets**:
- `connection_pool<DB, N>::acquire()` with N=1: verify second thread blocks until guard is destroyed.
- `connection_pool<DB, N>` instantiation with a connector that lacks `supports_concurrent_execute`: verify `static_assert` fires.
- `db::transaction()` RAII guard: verify `ROLLBACK` is issued on guard destruction without `commit()`; verify `COMMIT` is issued when `commit()` is called.
- State machine transitions: verify `Idle → InUse → Idle` cycle under normal acquire/release.

**Integration test targets**:
- Two threads concurrently calling `pool.acquire()` on a pool of size 1 under ThreadSanitizer: verify no data races.
- `thread_local_db<DB>`: verify independent connection per thread with no mutex contention.

**Mock boundaries**: The `DB` connection object is the primary mock boundary; inject a mock connector that records calls.

**Test isolation**: All unit tests use a `MockDB` connector with `supports_concurrent_execute`; no live database required.

---

## 6. Acceptance Criteria

- [ ] `connection_pool<DB, N>` compiles with no errors or warnings under C++23 with `-Wall -Wextra` when `connector_trait<DB>` declares `supports_concurrent_execute`.
- [ ] Two threads concurrently calling `pool.acquire()` on a pool of size 1 results in one thread blocking until the other releases the guard, with no data races (verified by ThreadSanitizer).
- [ ] `db::transaction()` guard issues `ROLLBACK` when destroyed without `commit()` having been called; issues `COMMIT` when `commit()` is called.
- [ ] Instantiating `connection_pool<DB, N>` for a connector without `supports_concurrent_execute` produces a `static_assert` compile error containing "supports_concurrent_execute".
- [ ] `thread_local_db<DB>` provides an independent connection per thread with no mutex contention.
