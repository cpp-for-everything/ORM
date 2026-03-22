# Feature: Wire-Protocol Optimisations

- [ ] `p1` - **ID**: `cpt-orm-featstatus-wire-protocol`

- [ ] `p2` - **ID**: `cpt-orm-feature-wire-protocol`

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
  - [Execute a Query Asynchronously via io\_uring / IOCP](#execute-a-query-asynchronously-via-io_uring--iocp)
  - [Bulk-Insert a Collection of Rows](#bulk-insert-a-collection-of-rows)
- [3. Processes / Business Logic (CDSL)](#3-processes--business-logic-cdsl)
  - [io\_uring Async Dispatch](#io_uring-async-dispatch)
  - [Zero-Copy Result Exposure](#zero-copy-result-exposure)
  - [Batch INSERT Accumulation and Rendering](#batch-insert-accumulation-and-rendering)
  - [Compile-Time SQL String Generation](#compile-time-sql-string-generation)
- [4. States](#4-states)
- [5. Definitions of Done](#5-definitions-of-done)
  - [Async I/O Integration](#async-io-integration)
  - [Zero-Copy Result Parsing](#zero-copy-result-parsing)
  - [Batch INSERT Query Type](#batch-insert-query-type)
  - [Compile-Time SQL Generation](#compile-time-sql-generation)
  - [Test Coverage Guidance](#test-coverage-guidance)
- [6. Acceptance Criteria](#6-acceptance-criteria)

<!-- /toc -->

## 1. Feature Context

### 1.1 Overview

Wire-protocol-level performance optimisations for the ORM: asynchronous I/O via `io_uring` (Linux) / IOCP (Windows) with a `co_await db.execute()` coroutine interface; zero-copy result parsing exposing raw column memory as `std::span` views; a `batch_insert` query type that accumulates rows and emits a single multi-row `INSERT`; and compile-time SQL string generation as `constexpr char[]` for connectors with fully static IR.

### 1.2 Purpose

The current SQLite connector calls the standard `sqlite3_*` C API synchronously, materialising result rows by copying data from the driver buffer. For network databases (PostgreSQL, MySQL, MongoDB), blocking I/O stalls the calling thread and per-row round-trips impose significant latency on bulk inserts. These four optimisations target the hot path: async I/O enables coroutine-based frameworks; zero-copy defers data ownership transfer; batch INSERT collapses N round-trips to one; compile-time SQL eliminates the `std::format` call at runtime.

**Performance**: This feature is the primary performance artifact for the ORM. Hot-path latency-sensitive operations are: (1) async `co_await db.execute()` — suspends without blocking the OS thread; (2) zero-copy result access via `result.span<col_idx>()` — no memcpy on the critical path; (3) `batch_insert` — collapses N round-trips to one statement. N+1 query prevention is a caller responsibility, not an ORM wire-level concern.

**Reliability**: Timeout and cancellation handling: `io_uring` SQE submission failure and IOCP completion errors both cause the awaitable to resume with an error result; the coroutine caller is responsible for retry decisions.

**Security**: The async dispatch path does not modify query content; SQL injection prevention is handled by the individual connector's prepared-statement layer.

**Requirements**: DESIGN pending — no PRD or DESIGN artifact registered yet.

**Principles**: DESIGN pending.

### 1.3 Actors

| Actor | Role in Feature |
|-------|-----------------|
| Developer | Uses `co_await db.execute()` for async queries, `std::span`-based result access for zero-copy reads, `batch_insert` for bulk-insert workloads, and benefits transparently from compile-time SQL generation. |

### 1.4 References

- **PRD**: DESIGN pending — no PRD registered.
- **Design**: DESIGN pending — no DESIGN artifact registered.
- **Dependencies**: `io_uring` (`liburing`, Linux 5.1+); IOCP (`windows.h`, Windows); C++20 coroutines (`<coroutine>`).

### 1.5 Scope & Boundaries

**In scope**:
- Asynchronous `co_await db.execute()` via `io_uring` (Linux) and IOCP (Windows).
- Zero-copy result exposure as `std::span<const std::byte>` views into driver buffers.
- `batch_insert` query type accumulating N rows into a single multi-row INSERT statement.
- Compile-time SQL string generation as `constexpr char[]` for fully-static query IRs.

**Out of scope**:
- Connection pooling and thread safety — see `FEATURE-thread-safety`.
- Connector-specific prepared-statement lifecycle — handled by individual connectors.
- Network protocol handshake and authentication — handled by individual connectors.
- Redis, Neo4j, or Cassandra async dispatch (requires connector-specific wire protocol integration).

### 1.6 Configuration

Not applicable — the async I/O path is selected at compile time by platform (`__linux__` / `_WIN32`); no runtime configuration options or feature flags are needed.

### Non-Applicability Declarations

- **UX**: Not applicable — this is a C++ library feature with no user-facing interface.
- **COMPL (Compliance)**: Not applicable — no regulatory or standards-compliance requirements apply to this feature. Data privacy (PII/GDPR): Not applicable at the connector layer — the connector transmits only what the caller supplies; PII handling is the application's responsibility.
- **OPS (Observability)**: Not applicable — logging and metrics are not applicable to this feature layer; instrumentation is the responsibility of the caller.
- **OPS (Rollout/Rollback)**: Not applicable — this is a C++ library feature distributed as source; rollout and rollback are governed by the consuming application's dependency management.
- **SEC (Injection prevention / auth)**: Not applicable to this feature — injection prevention is handled by individual connectors; authentication is handled by caller-supplied handles.
- **REL (Retry / circuit breaker)**: Not applicable at this layer — async I/O errors are propagated to the caller's coroutine; retry decisions are the caller's responsibility.

---

## 2. Actor Flows (CDSL)

### Execute a Query Asynchronously via io_uring / IOCP

- [ ] `p1` - **ID**: `cpt-orm-flow-wire-protocol-async-execute`

**Actor**: Developer

**Success Scenarios**:
- Developer `co_await`s `db.execute(query, args...)` inside a coroutine; the calling coroutine suspends while the I/O is in flight and resumes when the result is ready, without blocking the OS thread.

**Error Scenarios**:
- io_uring SQE submission fails: `co_await` resumes with an error result.
- IOCP completion indicates network error: `co_await` resumes with an error result.
- Network call exceeds caller-configured I/O timeout: the `io_uring` or IOCP completion arrives with an error or timeout code; `await_resume` returns an error `orm::result` to the coroutine caller without retry.

**Steps**:
1. [ ] - `p1` - Developer calls `co_await db.execute(query, runtime_args...)` inside a C++20 coroutine. - `inst-async-co-await`
2. [ ] - `p1` - Connector constructs the SQL string / BSON / CQL payload and submits it as a send SQE to the `io_uring` ring (Linux) or posts an overlapped I/O operation to IOCP (Windows). - `inst-async-submit-io`
3. [ ] - `p1` - The awaitable's `await_suspend` stores the coroutine handle and suspends the coroutine. - `inst-async-suspend`
4. [ ] - `p1` - The event loop polls the ring / IOCP completion port; on CQE / completion packet arrival, retrieves the coroutine handle and calls `resume()`. - `inst-async-resume`
5. [ ] - `p1` - **IF** the CQE / completion indicates a timeout or I/O error — `await_resume` returns an error `orm::result` to the coroutine. - `inst-async-timeout`
6. [ ] - `p1` - The coroutine resumes; `await_resume` returns the `orm::result<Row...>`. - `inst-async-await-resume`
7. [ ] - `p1` - **RETURN** `orm::result<Row...>` to the coroutine body. - `inst-async-return`

### Bulk-Insert a Collection of Rows

- [ ] `p1` - **ID**: `cpt-orm-flow-wire-protocol-batch-insert`

**Actor**: Developer

**Success Scenarios**:
- Developer accumulates a collection of entity values into a `batch_insert` query and calls `execute()`; the connector emits a single multi-row `INSERT INTO ... VALUES (...), (...), ...` statement.

**Error Scenarios**:
- Partial insert failure on network databases: connector reports the first error row; behaviour is connector-specific.
- Empty batch: `execute()` is a no-op and returns zero affected rows.

**Steps**:
1. [ ] - `p1` - Developer builds `batch_insert(entity1, entity2, …)` compile-time query. - `inst-batch-build`
2. [ ] - `p1` - Developer calls `db.execute(batch_query)`. - `inst-batch-execute`
3. [ ] - `p1` - Connector accumulates all row values into a local buffer. - `inst-batch-accumulate`
4. [ ] - `p1` - **IF** buffer is empty — **RETURN** zero-rows-affected result immediately. - `inst-batch-empty`
5. [ ] - `p1` - Connector renders a single multi-row SQL: `INSERT INTO table (cols) VALUES (row1), (row2), …` or uses the native bulk-load API if available. - `inst-batch-render`
6. [ ] - `p1` - Connector sends the single statement and awaits the result. - `inst-batch-send`
7. [ ] - `p1` - **RETURN** affected-rows count result to caller. - `inst-batch-return`

---

## 3. Processes / Business Logic (CDSL)

### io_uring Async Dispatch

- [ ] `p2` - **ID**: `cpt-orm-algo-wire-protocol-io-uring-dispatch`

**Input**: Prepared query payload (SQL string or wire-format bytes); runtime parameters; `io_uring` ring or IOCP handle; coroutine handle.

**Output**: Coroutine awaitable that resolves to `orm::result<Row...>`.

**Steps**:
1. [ ] - `p1` - Construct an awaitable object that captures the query payload, parameters, and the ring/IOCP handle. - `inst-uring-awaitable-ctor`
2. [ ] - `p1` - In `await_ready`: **RETURN** `false` (always suspend — I/O is always async). - `inst-uring-await-ready`
3. [ ] - `p1` - In `await_suspend`: submit a send SQE (`io_uring_prep_send`) or post an overlapped write to IOCP; store the coroutine handle in the SQE user data / OVERLAPPED structure. - `inst-uring-await-suspend`
4. [ ] - `p1` - In the event loop's completion handler: retrieve the coroutine handle from the CQE user data / OVERLAPPED, call `handle.resume()`. - `inst-uring-complete`
5. [ ] - `p1` - In `await_resume`: read the result bytes, hydrate into `orm::result<Row...>`. - `inst-uring-await-resume`
6. [ ] - `p1` - **RETURN** `orm::result<Row...>`. - `inst-uring-return`

### Zero-Copy Result Exposure

- [ ] `p2` - **ID**: `cpt-orm-algo-wire-protocol-zero-copy-result`

**Input**: Driver result buffer pointer; column count and byte-offset array for the current row.

**Output**: Per-column `std::span<const std::byte>` views without copying data from the driver buffer.

**Steps**:
1. [ ] - `p1` - Retain a non-owning pointer to the driver's internal result buffer (valid until the result object is destroyed or `next_row()` is called). - `inst-zcopy-retain-ptr`
2. [ ] - `p1` - **FOR EACH** column in the row — compute `std::span<const std::byte>(buffer + offset[col], length[col])`. - `inst-zcopy-span`
3. [ ] - `p1` - Expose each span via `result.span<col_idx>()` accessor; no copy occurs at this call site. - `inst-zcopy-expose`
4. [ ] - `p1` - Copy occurs only when the caller calls `.value<T>()` on the span or materialises via `.to_vector()`. - `inst-zcopy-lazy-copy`
5. [ ] - `p1` - **RETURN** the span view to the caller. - `inst-zcopy-return`

### Batch INSERT Accumulation and Rendering

- [ ] `p2` - **ID**: `cpt-orm-algo-wire-protocol-batch-insert-render`

**Input**: Compile-time `batch_insert` IR containing the target table and column list; collection of runtime row values.

**Output**: Single multi-row SQL INSERT string with all rows inlined, or a native bulk-load call.

**Steps**:
1. [ ] - `p1` - Render the INSERT preamble: `INSERT INTO table (col1, col2, …) VALUES `. - `inst-batch-preamble`
2. [ ] - `p1` - **FOR EACH** row in the row collection — render `($1, $2, …)` (or `?, ?` for MySQL-style) and append a `,` separator between rows. - `inst-batch-render-row`
3. [ ] - `p1` - Remove the trailing `,` after the last row. - `inst-batch-strip-comma`
4. [ ] - `p1` - **IF** the connector provides a native bulk-load API — delegate to it instead of the multi-row VALUES rendering. - `inst-batch-native-bulk`
5. [ ] - `p1` - **RETURN** the final SQL string (or signal that native bulk-load was used). - `inst-batch-render-return`

### Compile-Time SQL String Generation

- [ ] `p2` - **ID**: `cpt-orm-algo-wire-protocol-constexpr-sql`

**Input**: Query IR that is fully static (no runtime-variant nodes); connector's `supports_constexpr_sql` capability tag present.

**Output**: `constexpr char[]` SQL string computed at compile time, embedded in the binary as a string literal.

**Steps**:
1. [ ] - `p1` - At compile time, walk the query IR and verify that no node contains a runtime-variant sub-expression (i.e. no `Placeholder<T>` or `orm::ph` nodes). - `inst-csql-check-static`
2. [ ] - `p1` - **IF** runtime-variant nodes are present — fall back to the standard runtime `std::format` rendering path; do not generate `constexpr` SQL. - `inst-csql-fallback`
3. [ ] - `p1` - **IF** fully static — invoke the connector's `constexpr` SQL renderer at compile time, producing a `constexpr char[N]` array. - `inst-csql-generate`
4. [ ] - `p1` - Embed the `constexpr char[N]` array as a string literal in the binary; the hot path passes this pointer directly to the connector without any formatting call. - `inst-csql-embed`
5. [ ] - `p1` - **RETURN** the embedded string pointer at runtime. - `inst-csql-return`

---

## 4. States

Not applicable — all four optimisations are stateless transforms on query execution paths. Async I/O state (suspend/resume) is managed by the coroutine machinery, not by the connector.

---

## 5. Definitions of Done

### Async I/O Integration

- [ ] `p1` - **ID**: `cpt-orm-dod-wire-protocol-async-io`

The system **MUST** provide a `co_await db.execute(query, args...)` overload for connectors targeting network databases, using `io_uring` on Linux and IOCP on Windows, such that the calling coroutine suspends without blocking the OS thread and resumes upon I/O completion.

**Implements**:
- `cpt-orm-flow-wire-protocol-async-execute`
- `cpt-orm-algo-wire-protocol-io-uring-dispatch`

**Touches**:
- Entities: `db<DB>` execute async overload, `io_uring_awaitable<DB>`, `iocp_awaitable<DB>`
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Zero-Copy Result Parsing

- [ ] `p1` - **ID**: `cpt-orm-dod-wire-protocol-zero-copy`

The system **MUST** expose per-column result data as `std::span<const std::byte>` views directly into the driver's buffer, deferring any copy to the point where the caller materialises the value, with no copy performed by the ORM itself during iteration.

**Implements**:
- `cpt-orm-algo-wire-protocol-zero-copy-result`

**Touches**:
- Entities: `orm::result` span accessor path
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Batch INSERT Query Type

- [ ] `p1` - **ID**: `cpt-orm-dod-wire-protocol-batch-insert`

The system **MUST** provide a `batch_insert(rows...)` query type that accumulates N entity values and emits a single multi-row `INSERT INTO ... VALUES (...), (...), ...` SQL statement (or uses the connector's native bulk-load API), reducing N round-trips to one.

**Implements**:
- `cpt-orm-flow-wire-protocol-batch-insert`
- `cpt-orm-algo-wire-protocol-batch-insert-render`

**Touches**:
- Entities: `batch_insert<DB, Entity>` query type
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Compile-Time SQL Generation

- [ ] `p1` - **ID**: `cpt-orm-dod-wire-protocol-constexpr-sql`

The system **MUST** compute the SQL string as a `constexpr char[]` array at compile time for query IRs that contain no runtime-variant nodes and whose connector declares `supports_constexpr_sql`, embedding the result as a string literal in the binary to eliminate the `std::format` call on the hot path.

**Implements**:
- `cpt-orm-algo-wire-protocol-constexpr-sql`

**Touches**:
- Entities: `connector_trait<DB>` constexpr SQL render path
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Test Coverage Guidance

**Unit test targets**:
- `io_uring_awaitable<DB>`: verify `await_ready` returns false; verify coroutine suspends on `await_suspend`; verify `await_resume` returns correct `orm::result` on success and on I/O error.
- IOCP awaitable: same as above for Windows code path.
- Zero-copy span: verify `result.span<col_idx>().data()` equals driver buffer base + column offset; verify no copy occurs during iteration.
- `batch_insert`: verify that accumulating N rows emits exactly one SQL statement; verify empty batch returns zero-rows-affected without sending any statement.
- Compile-time SQL: verify `static_assert(sql[0] != '\0')` passes for a fully-static query IR targeting a `supports_constexpr_sql` connector.

**Integration test targets**:
- `co_await db.execute()` on Linux with a real `io_uring` ring: end-to-end coroutine suspend/resume with a MockDB connector.
- `batch_insert` of 1000 rows against a mock connector: verify single `execute()` call.

**Mock boundaries**: `io_uring` ring and IOCP completion port are the primary mock boundaries; use MockDB connector for unit tests.

**Test isolation**: Platform-specific paths (`io_uring` vs IOCP) must be tested independently; each path requires its own test executable or `#ifdef`-guarded test suite.

---

## 6. Acceptance Criteria

- [ ] `co_await db.execute(query, args...)` compiles and correctly suspends/resumes in a C++20 coroutine test on Linux (`io_uring`) and Windows (IOCP).
- [ ] Zero-copy result access via `result.span<0>()` returns a valid `std::span<const std::byte>` pointing into the driver buffer, verified by comparing the span's `data()` pointer with the driver buffer's base address.
- [ ] `batch_insert` with 1000 rows emits exactly one SQL statement to the connector, verified by a mock connector that counts `execute()` calls.
- [ ] A fully-static query IR targeting a `supports_constexpr_sql` connector produces a `constexpr char[]` SQL string verified at compile time via `static_assert(sql[0] != '\0')`.
- [ ] `co_await db.execute()` under ThreadSanitizer shows no data races when two coroutines execute concurrently on the same io_uring ring with separate connection handles.
