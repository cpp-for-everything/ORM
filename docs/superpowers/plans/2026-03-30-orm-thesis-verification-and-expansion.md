# ORM Thesis Verification and Expansion Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Verify the current ORM project against the thesis sources, correct any inaccurate claims, and maximally expand the Bulgarian thesis while keeping the touched English thesis chapters synchronized.

**Architecture:** The work is evidence-driven rather than prose-first: establish an authoritative baseline from the C++ headers, connector sources, build system, and tests; classify every thesis claim as implemented, partial/prototype, future work, or incorrect; then rewrite and expand the thesis so every technical explanation is scientifically correct, explained in the text flow, and supported by figures, tables, cross-references, and citations. Bulgarian is the primary drafting target, while English must be updated in every touched section to preserve bilingual parity.

**Tech Stack:** CMake; C++23/C++26 feature detection; GoogleTest; LaTeX; BibTeX; repository sources under `lib/`, `tests/`, and `doc/v2/`.

**Spec:** User-approved conversational design in this session; separate written spec skipped per user request.

---

## Chunk 1: Verification baseline and discrepancy ledger

### File map

| Action | Path | Responsibility |
|--------|------|---------------|
| Inspect | `CMakeLists.txt` | Top-level build entry point |
| Inspect | `lib/CMakeLists.txt` | Connector targets, reflection detection, optional SQLite wiring |
| Inspect | `tests/CMakeLists.txt` | Test suite registration |
| Inspect | `lib/include/ORM/ORM.hpp` | Public umbrella header |
| Inspect | `lib/include/ORM/connector/*.hpp` | Connector abstraction contract |
| Inspect | `lib/include/ORM/entity/*.hpp` | Entity model contract |
| Inspect | `lib/include/ORM/query/*.hpp` | Query IR and builder API |
| Inspect | `lib/include/ORM/result/result.hpp` | Result semantics |
| Inspect | `lib/src/ORM/db/connectors/**/CMakeLists.txt` | Concrete connector maturity and target exposure |
| Inspect | `tests/unit/*.cpp` | Unit evidence for features and connector scaffolding |
| Inspect | `tests/integration/*.cpp` | Integration evidence for MockDB / SQLite |
| Inspect | `README.md` | Existing high-level public claims |
| Modify | `docs/superpowers/plans/2026-03-30-orm-thesis-verification-and-expansion.md` | Append discrepancy notes while executing |

### Task 1: Re-establish the verified project baseline

**Files:**
- Inspect: `CMakeLists.txt`
- Inspect: `lib/CMakeLists.txt`
- Inspect: `tests/CMakeLists.txt`
- Inspect: `README.md`
- Inspect: `lib/include/ORM/**/*.hpp`
- Inspect: `tests/unit/*.cpp`
- Inspect: `tests/integration/*.cpp`

- [ ] **Step 1: Confirm the current repository state and build directory contents**

Run:

```powershell
git status --short
```

Run:

```powershell
Get-ChildItem build
```

Expected: see whether a configured build directory already exists and whether the doc-only worktree state is clean enough to continue.

- [ ] **Step 2: Configure the project if the build directory is missing or stale**

Run:

```powershell
cmake -S . -B build
```

Expected: configure succeeds and reports the reflection-detection and connector-target status.

- [ ] **Step 3: Build all targets**

Run:

```powershell
cmake --build build
```

Expected: all library and test targets build successfully; record any compiler warnings or failures that contradict thesis claims.

- [ ] **Step 4: Run the full test suite**

Run:

```powershell
ctest --test-dir build --output-on-failure
```

Expected: all discovered tests pass; record total count and any skipped/conditional suites such as SQLite.

- [ ] **Step 5: Read the authoritative headers and connector/test evidence**

Inspect these files in order:

```text
lib/include/ORM/connector/trait.hpp
lib/include/ORM/connector/db.hpp
lib/include/ORM/entity/property.hpp
lib/include/ORM/entity/relationship.hpp
lib/include/ORM/query/field.hpp
lib/include/ORM/query/select.hpp
lib/include/ORM/query/insert.hpp
lib/include/ORM/query/update.hpp
lib/include/ORM/query/delete.hpp
lib/include/ORM/query/placeholders.hpp
lib/include/ORM/result/result.hpp
lib/include/ORM/details/reflection.hpp
tests/unit/test_*.cpp
tests/integration/test_*.cpp
```

Expected: enough evidence to classify all major thesis claims.

- [ ] **Step 6: Create the discrepancy ledger**

Append a section to this plan file with four sublists:

Expected: a concise claim ledger to drive corrections before expansion.

### Execution notes: discrepancy ledger

- Implemented and verified:
  - Full build succeeds with `cmake --build build`.
  - Full test suite succeeds with `ctest --test-dir build --output-on-failure` (`230/230` passing).
  - `connector_trait<DB>`, capability tags, `orm::db<DB>`, anonymous placeholders, indexed placeholders, prepared queries, and CRUD query builders are present in the public headers and exercised by tests.
  - `MockDB` is the strongest SQL-rendering evidence path for the generic query IR.
  - `SQLiteDB` is the strongest real-backend evidence path: it opens a live SQLite handle, binds parameters, executes CRUD statements, hydrates rows, supports indexed placeholders, and exercises prepared queries in integration tests.

- Implemented but narrowly evidenced:
  - MySQL and PostgreSQL connector specialisations exist and are unit-tested as driver-surface renderers with in-process mock handles rather than live integration backends.
  - MongoDB, Redis, Cassandra, and Neo4j connector specialisations exist and are unit-tested as rendered-command / rendered-filter adapters over mock handles.
  - Thread-safety helpers (`connection_pool`, `thread_local_db`, `transaction_guard`) and wire-protocol helpers (`io_uring_awaitable`, `iocp_awaitable`, `zero_copy_result`, `batch_insert`, `make_constexpr_sql`) exist and are tested, but they should be described as infrastructure experiments or extensions rather than production-hardened runtime subsystems.
  - `migrate<DB>` exists as a schema-diff / DDL-generation facility with tests through a mock migration connector, but it is not yet a live schema migration pipeline tied to real database connectors.

- Prototype / mock / scaffolded:
  - `MySQLDB`, `PostgreSQLDB`, `MongoDB`, `RedisDB`, `CassandraDB`, and `Neo4jDB` currently rely on mock driver handles and renderer logic rather than live server integrations.
  - The reflection-detection infrastructure is present (`ORM_HAS_REFLECTION`, Boost.PFR fallback, `details/reflection.hpp`), but the fully inferred `property<T>` no-string API is not active in the current public property type.
  - `orm::result` exposes a range interface and connector-specific access helpers, but the current implementation is materially backed by `std::vector<Row>` rather than a truly streaming cursor-lazy row source.

- Future work / incorrect current thesis claims:
  - Any claim that the current public API already supports `property<int> id;` without a string literal is incorrect for the present headers and tests.
  - Any claim that `orm::result` currently fetches rows lazily from a connector cursor on demand is inaccurate for the present implementation.
  - Any claim that the non-SQL connectors or SQL server connectors are already production-ready live backends is overstated; the tested implementation is currently mock-backed / renderer-oriented.
  - Any claim that the test suite still consists of `153` tests is outdated; the verified count is `230` passing tests in the current build.

---

## Chunk 2: Correct the Bulgarian thesis before expansion

### File map

| Action | Path | Responsibility |
|--------|------|---------------|
| Modify | `doc/v2/bg/chapters/00_abstract.tex` | Verified achievements and scope framing |
| Modify | `doc/v2/bg/chapters/01_introduction.tex` | Problem statement, requirements, contributions, thesis structure |
| Modify | `doc/v2/bg/chapters/02_existing_solutions.tex` | Literature/context review and comparison tables |
| Modify | `doc/v2/bg/chapters/03_theoretical_background.tex` | Compile-time requirement and C++ mechanisms |
| Modify | `doc/v2/bg/chapters/04_orm_architecture.tex` | Core architecture, invariants, execution model, connector maturity |
| Modify | `doc/v2/bg/chapters/05_future_work.tex` | Planned features moved out of present-tense claims |
| Modify | `doc/v2/bg/chapters/06_conclusion.tex` | Evidence-backed conclusions and limitations |
| Modify | `doc/v2/bg/chapters/07_glossary.tex` | Minimal acronym glossary only |
| Modify | `doc/v2/bg/Main.tex` | Chapter ordering or appendix insertion if needed |

### Task 2: Rewrite inaccurate claims into verified scientific prose

**Files:**
- Modify: `doc/v2/bg/chapters/03_theoretical_background.tex`
- Modify: `doc/v2/bg/chapters/04_orm_architecture.tex`
- Modify: `doc/v2/bg/chapters/05_future_work.tex`
- Modify: `doc/v2/bg/chapters/06_conclusion.tex`

- [ ] **Step 1: Fix high-risk claim mismatches first**

Rewrite text that currently overstates or misstates:

```text
- orm::result semantics (materialised vector-backed vs truly cursor-lazy wording)
- C++26 reflection support in current property API
- connector maturity (production vs mock/prototype/scaffold)
- example APIs that no longer match the current headers
- tested/verified scope versus planned scope
```

Expected: no technically incorrect present-tense claim remains in BG chapters.

- [ ] **Step 2: Move non-implemented features into future work**

Revise `doc/v2/bg/chapters/05_future_work.tex` so anything not proven by code/tests/build is described as planned, proposed, or future direction.

Expected: future-work chapter becomes the sink for aspirational content rather than architecture drift.

- [ ] **Step 3: Reframe the conclusion around verified evidence**

Update `doc/v2/bg/chapters/06_conclusion.tex` so contributions, limitations, and test/build claims reflect the verified baseline from Chunk 1.

Expected: the conclusion becomes academically defensible.

- [ ] **Step 4: Keep the glossary minimal**

Reduce `doc/v2/bg/chapters/07_glossary.tex` to acronyms and a few recurring specialist terms only; ensure comprehension does not depend on this chapter.

Expected: thesis remains self-contained in the narrative flow.

---

## Chunk 3: Maximal Bulgarian expansion

### File map

| Action | Path | Responsibility |
|--------|------|---------------|
| Modify | `doc/v2/bg/chapters/01_introduction.tex` | Expanded motivation, requirements, methodology, contribution framing |
| Modify | `doc/v2/bg/chapters/02_existing_solutions.tex` | Broader comparative review |
| Modify | `doc/v2/bg/chapters/03_theoretical_background.tex` | Largest theory expansion |
| Modify | `doc/v2/bg/chapters/04_orm_architecture.tex` | Largest architecture expansion |
| Modify | `doc/v2/bg/chapters/05_future_work.tex` | Near-term, medium-term, and research extensions |
| Modify | `doc/v2/bg/chapters/06_conclusion.tex` | Stronger synthesis |
| Modify | `doc/v2/bg/Main.tex` | Optional appendix inputs if created |
| Modify | `doc/references.bib` | Add supporting references |

### Task 3: Expand the introduction and literature review

**Files:**
- Modify: `doc/v2/bg/chapters/01_introduction.tex`
- Modify: `doc/v2/bg/chapters/02_existing_solutions.tex`
- Modify: `doc/references.bib`

- [ ] **Step 1: Expand the introduction**

Add structured sections for:

```text
- practical and scientific motivation
- formal problem statement
- project requirements
- thesis methodology
- contribution summary
- chapter roadmap
```

Expected: Chapter 1 becomes a strong academic framing chapter.

- [ ] **Step 2: Expand the review of existing solutions**

Add comparison coverage for runtime ORMs, query builders, compile-time DSLs, and code-generation approaches. Include at least one comparison table with evaluation dimensions.

Expected: Chapter 2 supports the need for the proposed architecture.

- [ ] **Step 3: Add supporting references**

Update `doc/references.bib` with the sources needed by the expanded introduction, theory, comparison, and database API discussions.

Expected: citations are dense but relevant.

### Task 4: Expand the theoretical background from the compile-time requirement

**Files:**
- Modify: `doc/v2/bg/chapters/03_theoretical_background.tex`
- Modify: `doc/references.bib`

- [ ] **Step 1: Reorder Chapter 3 so it starts with the compile-time requirement**

Lead with why runtime query construction is insufficient and what guarantees the thesis seeks to move to compile time.

Expected: the chapter starts exactly from the requirement specified by the user.

- [ ] **Step 2: Add the full map of C++ techniques**

Explain in flowing Bulgarian prose, with cross-references and examples:

```text
- templates and template metaprogramming
- SFINAE and why concepts are preferable in modern C++
- constexpr / consteval evaluation
- member pointers and non-type template parameters
- traits and policy-based design
- static_assert diagnostics
- Boost.PFR fallback strategy
- C++26 std::meta reflection strategy
```

Expected: Chapter 3 becomes one of the longest and most explanatory chapters.

- [ ] **Step 3: Add at least one figure/table in Chapter 3**

Introduce a figure or table comparing compile-time and runtime validation mechanisms, or comparing the relevant C++ language mechanisms.

Expected: the theory chapter gains volume and navigability.

### Task 5: Expand the architecture chapter into the core technical chapter

**Files:**
- Modify: `doc/v2/bg/chapters/04_orm_architecture.tex`
- Modify: `doc/references.bib`

- [ ] **Step 1: Expand architecture goals and invariants**

Add explicit prose for:

```text
- backend independence
- OS independence
- compile-time safety requirements
- separation of concerns between user code, query IR, connector_trait, and backend APIs
```

Expected: the reader understands the design constraints before the details.

- [ ] **Step 2: Expand the entity/type/query/result sections**

Rewrite and expand the sections for `property<>`, `relationship<>`, query builders, placeholders, `orm::db<DB>`, and `orm::result<>` so that every technical term is defined in the narrative flow.

Expected: the architecture chapter stands alone without glossary dependence.

- [ ] **Step 3: Add connector maturity classification and evidence framing**

Clearly distinguish real connectors from mock/prototype/scaffolded or renderer/test-only connectors.

Expected: architecture prose remains scientifically precise.

- [ ] **Step 4: Add more figures, tables, and listings**

Add high-value artifacts such as:

```text
- connector status matrix
- placeholder behavior matrix
- portability/build-detection table
- execution-flow figure
- capability-gating figure
```

Expected: Chapter 4 becomes the main volume center of the thesis.

---

## Chunk 4: Synchronize English chapters and references

### File map

| Action | Path | Responsibility |
|--------|------|---------------|
| Modify | `doc/v2/en/chapters/00_abstract.tex` | Synchronized abstract |
| Modify | `doc/v2/en/chapters/01_introduction.tex` | Synchronized introduction |
| Modify | `doc/v2/en/chapters/02_existing_solutions.tex` | Synchronized literature/context review |
| Modify | `doc/v2/en/chapters/03_theoretical_background.tex` | Synchronized theory chapter |
| Modify | `doc/v2/en/chapters/04_orm_architecture.tex` | Synchronized architecture chapter |
| Modify | `doc/v2/en/chapters/05_future_work.tex` | Synchronized future work |
| Modify | `doc/v2/en/chapters/06_conclusion.tex` | Synchronized conclusion |
| Modify | `doc/v2/en/chapters/07_glossary.tex` | Minimal synchronized glossary |
| Modify | `doc/v2/en/Main.tex` | Structural parity if BG main changes |
| Modify | `doc/references.bib` | Shared reference set |

### Task 6: Mirror every touched BG thesis change into EN

**Files:**
- Modify: `doc/v2/en/chapters/*.tex`
- Modify: `doc/v2/en/Main.tex`

- [ ] **Step 1: Synchronize structure**

Ensure chapter/section additions, figures, tables, labels, and references exist in EN wherever they were added in BG.

Expected: both language variants have matching structure.

- [ ] **Step 2: Synchronize meaning, not literal wording**

Translate with scientific precision and maintain exact technical meaning, especially around implemented vs planned features.

Expected: no semantic drift between BG and EN.

- [ ] **Step 3: Synchronize the minimal glossary**

Keep EN glossary limited to acronyms/support terms only, mirroring BG scope.

Expected: bilingual docs remain aligned.

---

## Chunk 5: Final verification and thesis build validation

### File map

| Action | Path | Responsibility |
|--------|------|---------------|
| Inspect | `build/` | Rebuilt code artifacts |
| Inspect | `doc/v2/bg/` | Bulgarian thesis build inputs/outputs |
| Inspect | `doc/v2/en/` | English thesis build inputs/outputs |
| Modify | `docs/superpowers/plans/2026-03-30-orm-thesis-verification-and-expansion.md` | Record final execution notes |

### Task 7: Re-run project verification after documentation changes

**Files:**
- Inspect: `build/`
- Modify: `docs/superpowers/plans/2026-03-30-orm-thesis-verification-and-expansion.md`

- [ ] **Step 1: Rebuild all targets**

Run:

```powershell
cmake --build build
```

Expected: code still builds cleanly after any documentation-only or example-sync changes.

- [ ] **Step 2: Re-run all tests**

Run:

```powershell
ctest --test-dir build --output-on-failure
```

Expected: all tests still pass with no regressions.

- [ ] **Step 3: Compile the Bulgarian thesis if LaTeX tools are available**

Run from `doc/v2/bg`:

```powershell
pdflatex -interaction=nonstopmode Main.tex
bibtex Main
pdflatex -interaction=nonstopmode Main.tex
pdflatex -interaction=nonstopmode Main.tex
```

Expected: references, citations, figures, and tables resolve without fatal errors.

- [ ] **Step 4: Compile the English thesis if LaTeX tools are available**

Run from `doc/v2/en`:

```powershell
pdflatex -interaction=nonstopmode Main.tex
bibtex Main
pdflatex -interaction=nonstopmode Main.tex
pdflatex -interaction=nonstopmode Main.tex
```

Expected: EN document also resolves cleanly.

- [ ] **Step 5: Record final execution notes**

Append to this plan file:

```markdown
### Execution notes: final status
- Build status:
- Test status:
- BG thesis compile status:
- EN thesis compile status:
- Remaining optional follow-ups:
```

Expected: final state is documented for handoff.

---

## Completion checklist

- [ ] All targets build with `cmake --build build`
- [ ] Full test suite passes with `ctest --test-dir build --output-on-failure`
- [ ] Discrepancy ledger created from source/test evidence
- [ ] Bulgarian thesis corrected for all known factual mismatches
- [ ] Bulgarian thesis substantially expanded in Chapters 1–6
- [ ] Technical terminology explained in the text flow rather than delegated to glossary
- [ ] Minimal glossary retained only as optional support
- [ ] English thesis synchronized for every touched section
- [ ] References/citations strengthened where new explanatory text was added
- [ ] If LaTeX tools are available, BG and EN documents compile without fatal errors

**Execution note:** The user explicitly requested skipping the separate written-spec approval loop and proceeding directly with execution from the approved conversational design.
