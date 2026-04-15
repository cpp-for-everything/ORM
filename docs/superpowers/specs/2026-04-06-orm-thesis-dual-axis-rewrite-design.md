# ORM Thesis Dual-Axis Rewrite Design

**Date:** 2026-04-06  
**Status:** Approved by user — ready for bibliography audit and implementation planning  
**Author:** Design session with Alex Tsvetanov

---

## 1. Purpose and supersession

This document defines the approved full-rewrite design for the ORM thesis and presentation after the introduction of the coroutine-based asynchronous execution architecture and the new benchmark suite.

This design **supersedes** the narrower structure-only design in:

- `docs/superpowers/specs/2026-04-01-orm-thesis-structure-design.md`

The earlier design remains historically useful, but it is no longer sufficient for the current phase because the thesis must now treat the asynchronous execution model as a first-class scientific and engineering contribution rather than as a localized chapter update.

---

## 2. Goals

The rewrite must:

1. Preserve the core message of the thesis as a **`Compile-time Object-Relational Mapping библиотека`** for C++.
2. Rebuild the thesis around two explicit scientific axes:
   - the **compile-time modeling axis**;
   - the **asynchronous execution axis**.
3. Introduce a **dedicated theory chapter on coroutines and asynchronous execution in C++**.
4. Explain not only what the framework does, but also why its coroutine-based design is scientifically and practically justified.
5. Integrate the new benchmark work as **architectural evidence**, not as an isolated afterthought.
6. Keep all essential material inside the main thesis flow, with **no appendix used as a spillover area for important content**.
7. Produce a **Bulgarian main thesis** in `doc/v2/bg` and a **synchronized English mirror** in `doc/v2/en` with the same logic, evidence, figures, tables, and claims.
8. Update the Bulgarian presentation in `doc/v2/bg_presentation` so it reflects the new architecture and evaluation narrative.
9. Ensure the bibliography is expanded **before drafting thesis prose**, so claims are cited from the start rather than retrofitted later.
10. Keep all claims grounded in the actual repository code, tests, benchmark sources, and credible external references.

---

## 3. Non-goals

This rewrite does **not**:

- change the ORM library architecture itself;
- invent async features, connector capabilities, or benchmark conclusions not supported by code and measurements;
- demote the original compile-time ORM contribution in favor of a generic concurrency thesis;
- move substantial technical content into appendices;
- allow BG and EN versions to drift semantically.

---

## 4. Primary constraints and governing decisions

### 4.1 Canonical project framing

The thesis must consistently preserve the established project framing:

- **`Compile-time Object-Relational Mapping библиотека`**

The async layer extends the framework's practical viability, but does not replace the ORM-centered identity of the work.

### 4.2 Structural decision approved by the user

The thesis must follow a **dual-axis full rewrite** rather than chapter-pass patching.

The original thesis message remains intact, but the coroutine-based asynchronous architecture must be treated as a major new scientific and practical strengthening of the framework.

### 4.3 Dedicated coroutine theory chapter

The new coroutine material must begin with a **standalone theory chapter** explaining:

- what coroutines are;
- how they differ from threads;
- coroutine syntax in C++;
- compiler lowering to state machines;
- coroutine frames and promise objects;
- suspension and resumption semantics;
- thread-pool interaction;
- safety and lifecycle concerns.

### 4.4 No appendix rule

The final thesis should not rely on an appendix for important architectural or evaluative material. If a concept, result, or diagram is important for understanding the scientific argument, it belongs in the main chapter flow.

### 4.5 Abbreviation rule

Every abbreviation must first appear in expanded form with the abbreviation in parentheses before the abbreviation is subsequently used in short form.

Examples of the rule's intent:

- first mention: `Object-Relational Mapping (ORM)`
- later use: `ORM`

### 4.6 Bibliography-first workflow

Before drafting the Bulgarian thesis text, the work must first:

1. audit all new theoretical and implementation claims;
2. identify all external sources that must be cited;
3. update `doc/references.bib` accordingly.

Only after that may the BG drafting begin.

### 4.7 Bilingual synchronization

All core thesis changes must be mirrored between:

- `doc/v2/bg/`
- `doc/v2/en/`

The Bulgarian version is the primary drafting stream, but the English version must remain a structure-synchronized and claim-synchronized mirror.

---

## 5. Source material and evidence base

### 5.1 Existing thesis and writing guidance

Primary document constraints come from:

- `doc/v2/bg/thesis_writing_guidelines_bg.md`
- `doc/v2/bg/Main.tex`
- `doc/v2/en/Main.tex`
- `doc/references.bib`

### 5.2 Async architecture evidence

Primary async architecture evidence comes from:

- `doc/obsidan/03_Architecture/07_Async_Architecture.md`
- `lib/include/ORM/async/task.hpp`
- `lib/include/ORM/async/thread_pool.hpp`
- `lib/include/ORM/async/io_context.hpp`
- `lib/include/ORM/connector/async_db.hpp`
- `lib/include/ORM/connector/capabilities.hpp`
- `lib/include/ORM/db/connectors/ThreadSafety/async_thread_safety.hpp`

### 5.3 Benchmark evidence

Primary benchmark evidence comes from:

- `benchmarks/bench_async.cpp`
- `docs/superpowers/specs/2026-04-06-benchmark-redesign-design.md`
- recorded benchmark outputs and measurement notes from the current development phase

### 5.4 Existing compile-time ORM evidence

Primary compile-time ORM evidence comes from:

- `lib/include/ORM/entity/property.hpp`
- `lib/include/ORM/entity/table.hpp`
- `lib/include/ORM/entity/relationship.hpp`
- `lib/include/ORM/query/`
- `lib/include/ORM/connector/trait.hpp`
- `lib/include/ORM/connector/capabilities.hpp`
- `lib/include/ORM/connector/db.hpp`
- connectors under `lib/include/ORM/db/connectors/`
- migration utilities under `lib/include/ORM/db/migration/`

### 5.5 Verification evidence

Primary verification evidence comes from:

- unit tests under `tests/unit/`
- integration and live tests under `tests/integration/`
- especially:
  - `tests/unit/test_thread_safety.cpp`
  - `tests/unit/test_async_connector.cpp`
  - `tests/unit/test_task.cpp`
  - `tests/unit/test_thread_pool.cpp`
  - backend-specific connector tests

---

## 6. Central thesis framing

The rewritten thesis must argue that the framework is best understood as a coherent system built around **three mutually supporting layers of evidence**:

1. **Compile-time modeling**
   - typed entity model;
   - typed query intermediate representation;
   - trait-based backend materialization;
   - capability-gated admissibility.

2. **Asynchronous execution architecture**
   - coroutine-based task abstraction;
   - thread-pool and event-driven execution support;
   - connector-specific async strategies;
   - resource-safe async database access.

3. **Verification and empirical evaluation**
   - compile-time constraints;
   - unit and integration tests;
   - benchmark evidence showing when async execution matters and when it does not.

The thesis must not present the async subsystem as a tangential convenience layer. It must be integrated as a second major architectural dimension that strengthens the scientific viability of the framework.

---

## 7. Approved logical chapter map

The thesis must follow this logical chapter sequence in both BG and EN:

1. Abstract
2. Introduction
3. Existing solutions and positioning
4. Theoretical foundations of Object-Relational Mapping and heterogeneous storage
5. Theory of coroutines and asynchronous execution in C++
6. Compile-time ORM architecture
7. Async execution architecture of the framework
8. Backend realization and native client-library execution models
9. Verification and empirical evaluation
10. Extensibility through new connectors
11. Limitations and future work
12. Conclusion
13. Bibliography

There must be **no appendix chapter** carrying essential material.

---

## 8. Detailed chapter responsibilities

### 8.1 Chapter 1 — Introduction

Must establish:

- the problem of runtime query validation;
- the mismatch between strongly typed application code and heterogeneous storage models;
- the research objective of a compile-time ORM framework;
- the additional practical challenge of efficient asynchronous execution;
- the thesis hypothesis that compile-time modeling and coroutine-based async execution can be combined into one coherent framework.

### 8.2 Chapter 2 — Existing solutions and positioning

Must position the project against:

- classical runtime ORMs;
- driver-centric data access libraries;
- compile-time or DSL-based query systems;
- solutions that only support relational models;
- solutions lacking a coherent async execution story across backend types.

### 8.3 Chapter 3 — Theoretical foundations of Object-Relational Mapping and heterogeneous storage

Must explain:

- Object-Relational Mapping as an abstraction problem;
- logical schema vs physical materialization;
- relational, document, key-value, graph, and wide-column models;
- relationship strategies such as embedding and reference-based linkage;
- the theoretical limits of a unified abstraction.

### 8.4 Chapter 4 — Theory of coroutines and asynchronous execution in C++

Must explain in scientific prose:

- process, thread, task, and coroutine distinctions;
- coroutine syntax and control-flow constructs in C++;
- lowering to a compiler-generated state machine;
- coroutine frames, local state, promise objects, and continuation behavior;
- suspension and resumption semantics;
- how coroutines differ from kernel-scheduled threads;
- how thread pools cooperate with coroutines without becoming the same abstraction;
- lifecycle and safety concerns such as ownership, exception propagation, and cancellation boundaries.

This chapter must be theory-first, with the framework introduced only as a motivating application of the concepts.

### 8.5 Chapter 5 — Compile-time ORM architecture

Must present the original architectural core of the project:

- entity descriptions;
- typed query intermediate representation;
- `connector_trait<DB>`;
- capability gating;
- the invariant that application logic manipulates C++ types rather than backend-native strings.

### 8.6 Chapter 6 — Async execution architecture of the framework

Must explain the framework-specific realization of the asynchronous model:

- `Task<T>`;
- `sync_wait()` as bridge/testing tool rather than dominant user-facing style;
- `ThreadPool` and `run_on_pool()`;
- `IoContext`;
- `async_db<DB>`;
- `async_connection_pool`;
- `async_transaction_guard`;
- universal async-by-offload path;
- native non-blocking path for connectors that support it.

### 8.7 Chapter 7 — Backend realization and native client-library execution models

Must analyze not only logical materialization, but also execution semantics per backend family.

It must discuss, where applicable:

- PostgreSQL via libpq asynchronous state-machine style APIs;
- MySQL via `_start` / `_cont` non-blocking pairs;
- Redis via hiredis async integration;
- Cassandra via future/callback-based completion;
- MongoDB and Neo4j as honest sync-driver scenarios where thread-pool offload remains the correct architectural choice.

Each backend section must either include its theoretical basis directly or explicitly reference the relevant theory chapter subsection.

### 8.8 Chapter 8 — Verification and empirical evaluation

Must unify:

- compile-time verification;
- unit and integration testing;
- benchmark methodology;
- benchmark results;
- interpretation of sync vs async performance under different I/O latencies and concurrency levels.

The benchmark section must explain why earlier `sync_wait()`-oriented micro-structures are misleading and why throughput-oriented coroutine workloads are the scientifically relevant measurement target.

### 8.9 Chapter 9 — Extensibility through new connectors

Must retain and deepen the formal connector-contract contribution.

It must explicitly cover:

- connector requirements;
- signatures;
- typing;
- naming conventions;
- absence of inheritance-based `IConnector` abstraction;
- capability declarations;
- async-capable connector obligations where relevant;
- completeness and maturity criteria.

### 8.10 Chapter 10 — Limitations and future work

Must discuss what remains outside the current scope after the new benchmark and async work has been integrated, for example:

- broader live-backend async benchmarking;
- richer workload models;
- distributed connection-pool strategies;
- future reflection-assisted entity description;
- lower-level runtime enhancements.

### 8.11 Chapter 11 — Conclusion

Must synthesize the thesis as proving that:

- compile-time ORM is viable in standard C++;
- the same framework can maintain backend honesty across heterogeneous storage families;
- coroutine-based asynchronous execution increases the framework's practical relevance;
- empirical evaluation supports the architectural claims rather than merely decorating them.

---

## 9. Figure, table, and listing inventory

### 9.1 Required figure density

The rewritten thesis must include a significantly richer set of real figures than the current version. Figures must be explanatory, not decorative.

### 9.2 Core figure inventory

At minimum, the rewrite should include:

- storage-model taxonomy figure;
- logical-model vs physical-materialization figure;
- relationship-strategy figure (`reference` vs `embed`);
- process/thread/coroutine comparison figure;
- coroutine state-machine lowering figure;
- coroutine-frame structure figure;
- coroutine and thread-pool cooperation figure;
- compile-time ORM layered architecture figure;
- async subsystem architecture figure;
- universal async path figure;
- native async path figure;
- async connection-pool lifecycle figure;
- backend async-strategy atlas figure;
- benchmark harness figure;
- throughput and/or speedup plots;
- final synthesis figure combining compile-time, async, and evidence axes.

### 9.3 Table policy

Tables must be readable and may be long when necessary, but must avoid awkward page layout.

For long tables:

- prefer breakable table mechanisms such as `longtable`, `ltablex`, or equivalent;
- avoid large empty spaces caused by over-constrained floating tables;
- avoid shrinking the text so much that the table becomes unreadable.

### 9.4 Listing policy

Listings must be chosen to support the scientific argument and should be tied to repository evidence. Captions and surrounding prose must explain why each listing matters.

---

## 10. LaTeX layout and formatting rules

### 10.1 No overlap / no overflow rule

The final rewritten thesis must be laid out so that:

- TikZ diagrams do not contain overlapping nodes, arrows, or labels;
- tables, figures, and text do not produce visible overflow problems;
- oversized layouts are resolved structurally rather than hidden through brute-force scaling.

### 10.2 TikZ layout guidance

When necessary, TikZ positioning should use explicit distances, `calc`-based expressions, and mixed-unit coordinate expressions to achieve stable non-overlapping layouts.

The preferred order of operations is:

1. fix the structure;
2. fix spacing and alignment;
3. only then scale if still necessary.

### 10.3 Existing preamble-aware table usage

When wide evidence or capability tables contain slash-heavy content, prefer the existing slash-aware column types already defined in the shared BG and EN preambles, rather than ad hoc paragraph-column hacks.

Where left-aligned width-controlled columns are needed, prefer the existing project column types over raw `p{...}` usage.

### 10.4 Inline technical text formatting

Inline code and path formatting must follow the existing thesis conventions:

- use `\code{...}` for code identifiers;
- use `\path{...}` for file paths;
- avoid wrapping Bulgarian prose in raw monospaced code formatting;
- preserve the shared BG/EN preamble behavior for breakable inline technical text.

---

## 11. Citation strategy and bibliography-first workflow

### 11.1 Mandatory sequence

The rewrite must follow this order:

1. bibliography audit;
2. bibliography update in `doc/references.bib`;
3. Bulgarian thesis drafting;
4. synchronized English mirroring;
5. presentation rewrite.

### 11.2 Reference categories that must be audited before drafting

The bibliography audit must cover at least:

- the C++ language standard and authoritative coroutine references;
- theoretical or standards-level discussion of asynchronous execution models where appropriate;
- native client-library references for:
  - libpq asynchronous APIs;
  - MySQL non-blocking APIs;
  - hiredis async APIs;
  - Cassandra driver future/callback APIs;
  - MongoDB and Neo4j driver behavior if discussed comparatively;
- event-loop/runtime references if the thesis explicitly discusses readiness polling or OS event mechanisms;
- benchmarking methodology references where needed to support evaluation methodology claims.

### 11.3 Drafting rule

No new theoretical subsection should be drafted until its likely citations are already represented or planned concretely in `doc/references.bib`.

---

## 12. Cross-chapter writing rules

The final thesis prose must obey the following:

1. Every backend-specific architectural discussion must include either:
   - its own theoretical basis subsection; or
   - an explicit cross-reference to the relevant theory subsection.
2. Every visual element must be introduced and interpreted in prose as though the reader must understand the argument even without seeing the figure or table.
3. The async contribution must be explained both theoretically and architecturally, not only empirically.
4. The benchmark chapter must state where async helps, where it does not, and why.
5. The text must distinguish clearly between:
   - logical model;
   - compile-time query intermediate representation;
   - runtime execution strategy;
   - backend-specific physical materialization.
6. All abbreviations must be defined on first use.
7. Important content must remain in the chapter flow rather than being deferred to appendices.
8. BG and EN versions must preserve matching claims, structure, figures, and evidence references.

---

## 13. Rewrite workflow

### 13.1 Phase order

The approved implementation sequence is:

1. Audit existing bibliography coverage.
2. Add missing bibliography entries to `doc/references.bib`.
3. Rewrite the Bulgarian abstract and introduction so they establish the new dual-axis framing.
4. Rewrite the Bulgarian theory and architecture chapters.
5. Rewrite the Bulgarian backend, evaluation, extensibility, future-work, and conclusion chapters.
6. Mirror the approved Bulgarian content into synchronized English chapter rewrites.
7. Update the Bulgarian presentation to match the rewritten thesis.
8. Perform document builds and repository-level verification.

### 13.2 BG/EN synchronization strategy

The Bulgarian thesis remains the primary drafting stream, but EN synchronization must happen in bounded batches rather than being postponed until the very end.

Recommended batching:

- batch A: abstract + introduction + theory chapters;
- batch B: architecture chapters;
- batch C: backend/evaluation/extensibility chapters;
- batch D: future work + conclusion + presentation alignment.

---

## 14. Deliverables

The rewrite must ultimately produce:

1. an updated `doc/references.bib` with the required new sources;
2. a rewritten Bulgarian thesis under `doc/v2/bg`;
3. a synchronized English mirror under `doc/v2/en`;
4. an updated Bulgarian presentation under `doc/v2/bg_presentation`;
5. updated supporting documentation if the rewritten architecture narrative requires synchronization elsewhere in the repository.

---

## 15. Verification gates before completion

Before the implementation phase is considered complete, the following must be verified:

### 15.1 Documentation verification

- Bulgarian thesis compiles successfully;
- English thesis compiles successfully;
- Bulgarian presentation compiles successfully;
- no major visual overlap or overflow remains unresolved;
- long tables break acceptably across pages where needed.

### 15.2 Editorial verification

- no appendix carries essential content;
- abbreviations are defined on first use;
- figures and tables are integrated into the prose naturally;
- BG and EN versions remain synchronized in claims and structure.

### 15.3 Repository-level verification

Because project rules require broader verification after changes, final completion should also include:

- full build of all targets;
- full test-suite execution.

---

## 16. Readiness for the next phase

This design is approved and ready for the next step.

The next phase must produce:

- a bibliography audit and reference-expansion pass;
- a concrete implementation plan for the rewrite sequence;
- then the actual thesis and presentation rewriting work.
