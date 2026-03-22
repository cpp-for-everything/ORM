# Feature: C++26 Reflection Integration

- [ ] `p1` - **ID**: `cpt-orm-featstatus-cpp26-reflection`

- [ ] `p2` - **ID**: `cpt-orm-feature-cpp26-reflection`

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
  - [Define an Entity Using C++26 Reflection (String-Free)](#define-an-entity-using-c26-reflection-string-free)
  - [Override an Inferred Column Name](#override-an-inferred-column-name)
- [3. Processes / Business Logic (CDSL)](#3-processes--business-logic-cdsl)
  - [Compile-Time Column Name Resolution](#compile-time-column-name-resolution)
  - [PFR Fallback Path](#pfr-fallback-path)
- [4. States](#4-states)
- [5. Definitions of Done](#5-definitions-of-done)
  - [Inferred Column Name on C++26](#inferred-column-name-on-c26)
  - [Explicit String Override](#explicit-string-override)
  - [PFR Fallback Enforcement](#pfr-fallback-enforcement)
  - [Dual-Path Coexistence](#dual-path-coexistence)
  - [Test Coverage Guidance](#test-coverage-guidance)
- [6. Acceptance Criteria](#6-acceptance-criteria)

<!-- /toc -->

## 1. Feature Context

### 1.1 Overview

Integration of C++26 `std::meta` reflection to enable compile-time column name inference from struct member names in `property<T>` declarations, eliminating the mandatory `"col_name"` string argument on C++26-capable compilers while retaining the Boost.PFR fallback path and optional explicit string override on all compilers.

### 1.2 Purpose

On pre-C++26 compilers, `property<int, "id"> id;` is required — the string argument is mandatory because no compile-time member-name introspection is available. C++26 introduces `std::meta` reflection (`__cpp_impl_reflection`) that allows `std::meta::name_of(^^member)` to retrieve the member name at compile time. This feature exploits that capability so that `property<int> id;` is sufficient: the ORM infers column name `"id"` automatically, reducing entity definition boilerplate. The explicit string path remains available for column name overrides, and the PFR path remains required on pre-C++26 compilers.

**Performance**: All column name resolution occurs at compile time; there is zero runtime cost for this feature regardless of whether the reflection or PFR path is used.

**Reliability**: Not applicable — this is a pure compile-time feature with no runtime state or network interaction.

**Security**: Not applicable — this feature operates entirely at compile time and has no runtime security surface.

**Requirements**: DESIGN pending — no PRD or DESIGN artifact registered yet.

**Principles**: DESIGN pending.

### 1.3 Actors

| Actor | Role in Feature |
|-------|-----------------|
| Developer | Defines entity structs; on C++26 compilers omits the `"col_name"` string arg in `property<T>`; on pre-C++26 provides the string arg as before. |

### 1.4 References

- **PRD**: DESIGN pending — no PRD registered.
- **Design**: DESIGN pending — no DESIGN artifact registered.
- **Dependencies**: C++26 `std::meta` (`__cpp_impl_reflection`); Boost.PFR (pre-C++26 fallback, already in use).

### 1.5 Scope & Boundaries

**In scope**:
- `property<T>` column name inference via `std::meta::name_of(^^member)` on C++26-capable compilers.
- Explicit string override `property<T, "name">` taking precedence over inference on all compilers.
- `static_assert` compile error when `property<T>` is used without a string argument on pre-C++26 compilers.
- `#ifdef __cpp_impl_reflection` dual-path coexistence with no ODR violations.

**Out of scope**:
- Any runtime reflection or dynamic column name resolution.
- Reflection of types beyond `property<T>` member names (e.g., table names, relationship names).
- Support for the Bloomberg clang experimental reflection fork as a production target (experimental only).

### 1.6 Configuration

Not applicable — the reflection path is selected automatically by the compiler feature macro `__cpp_impl_reflection`; no runtime configuration options or feature flags are needed.

### Non-Applicability Declarations

- **UX**: Not applicable — this is a C++ library compile-time feature with no user-facing interface.
- **COMPL (Compliance)**: Not applicable — no regulatory or standards-compliance requirements apply to this feature. Data privacy (PII/GDPR): Not applicable at the connector layer — the connector transmits only what the caller supplies; PII handling is the application's responsibility.
- **OPS (Observability)**: Not applicable — this feature is entirely compile-time; no logging or metrics apply.
- **OPS (Rollout/Rollback)**: Not applicable — this is a C++ library feature distributed as source; rollout and rollback are governed by the consuming application's dependency management.
- **SEC (Authentication/Authorization/Injection)**: Not applicable — this feature operates entirely at compile time and has no runtime security surface.
- **PERF (Hot paths / N+1)**: Not applicable at runtime — all column name resolution is compile-time; zero runtime overhead.
- **REL (Retry / circuit breaker)**: Not applicable — this is a pure compile-time feature with no runtime state or network interaction.

---

## 2. Actor Flows (CDSL)

### Define an Entity Using C++26 Reflection (String-Free)

- [ ] `p1` - **ID**: `cpt-orm-flow-cpp26-reflection-define-entity`

**Actor**: Developer

**Success Scenarios**:
- Developer declares `property<int> id;` on a C++26-capable compiler; the ORM infers the column name `"id"` at compile time without any string argument.
- Entity is used in a compile-time query; the inferred column name is embedded in the query IR correctly.

**Error Scenarios**:
- Developer omits the string argument on a pre-C++26 compiler: `static_assert` compile error with a message directing the developer to supply the explicit string.

**Steps**:
1. [ ] - `p1` - Developer declares a struct with `property<int> id;` (no string argument). - `inst-reflect-declare-no-str`
2. [ ] - `p1` - At compile time, `property<T>`'s column-name resolver checks `__cpp_impl_reflection`. - `inst-reflect-check-macro`
3. [ ] - `p1` - **IF** `__cpp_impl_reflection` is defined — invoke `std::meta::name_of(^^member)` to obtain the member name `"id"` as a `constexpr std::string_view`. - `inst-reflect-name-of`
4. [ ] - `p1` - The resolved name `"id"` is stored as a compile-time string in the `property<T>` type. - `inst-reflect-store-name`
5. [ ] - `p1` - Developer builds a query referencing the property; the IR uses the inferred column name transparently. - `inst-reflect-use-in-query`
6. [ ] - `p1` - **RETURN** compile success; entity and query compile without the string argument. - `inst-reflect-return`

### Override an Inferred Column Name

- [ ] `p1` - **ID**: `cpt-orm-flow-cpp26-reflection-override-name`

**Actor**: Developer

**Success Scenarios**:
- Developer declares `property<int, "user_id"> id;` on a C++26 compiler; the explicit string `"user_id"` takes precedence over the inferred `"id"`.

**Steps**:
1. [ ] - `p1` - Developer declares `property<int, "user_id"> id;`. - `inst-override-declare`
2. [ ] - `p1` - `property<T, "user_id">`'s column-name resolver detects the explicit string argument is present. - `inst-override-detect-str`
3. [ ] - `p1` - The explicit string `"user_id"` is used as the column name; reflection inference is skipped. - `inst-override-use-explicit`
4. [ ] - `p1` - **RETURN** compile success with column name `"user_id"`. - `inst-override-return`

---

## 3. Processes / Business Logic (CDSL)

### Compile-Time Column Name Resolution

- [ ] `p2` - **ID**: `cpt-orm-algo-cpp26-reflection-resolve-name`

**Input**: `property<T>` or `property<T, "name">` declaration at compile time.

**Output**: `constexpr std::string_view` column name resolved at compile time.

**Steps**:
1. [ ] - `p1` - **IF** explicit string argument `"name"` is present in the `property<T, "name">` template parameter — **RETURN** `"name"` immediately; no reflection needed. - `inst-resolve-explicit`
2. [ ] - `p1` - **ELSE IF** `__cpp_impl_reflection` is defined — obtain the member name via `std::meta::name_of(^^MemberPointer)` and **RETURN** it as a `constexpr std::string_view`. - `inst-resolve-reflect`
3. [ ] - `p1` - **ELSE** (pre-C++26, no explicit string) — emit `static_assert(false, "property<T>: explicit column name required on pre-C++26 compilers")`. - `inst-resolve-assert`

### PFR Fallback Path

- [ ] `p2` - **ID**: `cpt-orm-algo-cpp26-reflection-pfr-fallback`

**Input**: `property<T, "col_name">` declaration on a pre-C++26 compiler.

**Output**: Column name from the explicit string argument; compile error if string is absent.

**Steps**:
1. [ ] - `p1` - Verify `__cpp_impl_reflection` is NOT defined (pre-C++26 path). - `inst-pfr-check-macro`
2. [ ] - `p1` - **IF** string argument is present — **RETURN** string argument as column name. - `inst-pfr-use-str`
3. [ ] - `p1` - **IF** string argument is absent — emit `static_assert(false, "property<T>: explicit column name required on pre-C++26 compilers; use property<T, \"col_name\">")`. - `inst-pfr-assert`

---

## 4. States

Not applicable — reflection is a compile-time mechanism with no runtime state machine.

---

## 5. Definitions of Done

### Inferred Column Name on C++26

- [ ] `p1` - **ID**: `cpt-orm-dod-cpp26-reflection-inferred-name`

The system **MUST** infer the column name from the struct member name when `property<T>` is used without a string argument on a compiler that defines `__cpp_impl_reflection`, producing the same column name as an explicit `property<T, "member_name">` declaration.

**Implements**:
- `cpt-orm-flow-cpp26-reflection-define-entity`
- `cpt-orm-algo-cpp26-reflection-resolve-name`

**Touches**:
- Entities: `property<T>` column name resolution
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Explicit String Override

- [ ] `p1` - **ID**: `cpt-orm-dod-cpp26-reflection-explicit-override`

The system **MUST** use the explicit string argument as the column name whenever `property<T, "name">` is used, regardless of whether `__cpp_impl_reflection` is defined, overriding any reflection-inferred name.

**Implements**:
- `cpt-orm-flow-cpp26-reflection-override-name`
- `cpt-orm-algo-cpp26-reflection-resolve-name`

**Touches**:
- Entities: `property<T, "name">` column name resolution
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### PFR Fallback Enforcement

- [ ] `p1` - **ID**: `cpt-orm-dod-cpp26-reflection-pfr-fallback`

The system **MUST** continue to compile correctly on pre-C++26 compilers when the explicit string argument is provided, and **MUST** emit a `static_assert` compile error when `property<T>` is used without a string argument on a pre-C++26 compiler.

**Implements**:
- `cpt-orm-algo-cpp26-reflection-pfr-fallback`

**Touches**:
- Entities: `property<T>` pre-C++26 path
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Dual-Path Coexistence

- [ ] `p1` - **ID**: `cpt-orm-dod-cpp26-reflection-coexistence`

The system **MUST** compile correctly on both C++26-capable and pre-C++26 compilers in the same codebase via `#ifdef __cpp_impl_reflection` guards, with no ODR violations or symbol conflicts between the two paths.

**Implements**:
- `cpt-orm-feature-cpp26-reflection`

**Touches**:
- Entities: `property<T>` dual-path implementation
- Note: Design references pending — will be updated when DESIGN artifact is registered.

### Test Coverage Guidance

**Unit test targets**:
- `property<int>` on C++26: verify `property<int>::column_name() == "id"` via `static_assert`.
- `property<int, "user_id">` on C++26: verify explicit string overrides reflection inference.
- `property<int, "user_id">` on pre-C++26: verify correct column name returned.
- `property<int>` on pre-C++26: verify `static_assert` compile error containing "explicit column name required".
- Dual-path coexistence: verify identical query IR produced by `property<int> id;` on C++26 and `property<int, "id"> id;` on pre-C++26.

**Integration test targets**:
- Entity with `property<int> id;` on a C++26 compiler: compile and execute a SELECT query; verify the rendered SQL uses column name `"id"`.
- Mixed entity (some fields with explicit strings, some inferred): verify each field gets its expected column name.

**Mock boundaries**: No mock boundaries needed — all tests are compile-time or use the existing `MockDB` connector.

**Test isolation**: C++26 path tests require a C++26-capable compiler (GCC 16+); pre-C++26 path tests must compile under GCC 13/Clang 18 without `__cpp_impl_reflection`.

---

## 6. Acceptance Criteria

- [ ] `property<int> id;` on a C++26-capable compiler produces column name `"id"`, verified by `static_assert(property<int>::column_name() == "id")`.
- [ ] `property<int, "user_id"> id;` produces column name `"user_id"` on both C++26 and pre-C++26 compilers.
- [ ] `property<int> id;` on a pre-C++26 compiler produces a `static_assert` compile error containing "explicit column name required".
- [ ] An entity using `property<int> id;` on C++26 produces identical query IR to an entity using `property<int, "id"> id;` on the same compiler.
- [ ] The codebase compiles cleanly under both GCC 16 (C++26) and GCC 13 (C++23) with no warnings.
