# ORM Thesis Dual-Axis Rewrite Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rewrite the ORM thesis and Bulgarian presentation around the approved dual-axis structure, beginning with bibliography expansion and then rebuilding the Bulgarian and English documents around the compile-time modeling and asynchronous execution contributions.

**Architecture:** The rewrite keeps the existing `doc/v2` LaTeX scaffolding but replaces the old chapter flow with a new main-body narrative: storage theory, coroutine theory, compile-time ORM architecture, async execution architecture, backend execution models, empirical evaluation, and connector extensibility. Bulgarian is drafted first in bounded batches, English is mirrored immediately after each batch, and bibliography/layout infrastructure is prepared before prose drafting starts.

**Tech Stack:** LaTeX, BibTeX, TikZ, longtable/ltablex/tabularx, Beamer, existing `doc/v2` templates, ORM source code and tests, benchmark sources and recorded measurements

---

## File Map

### Core bibliography and orchestration files

- Modify: `doc/references.bib`
- Modify: `doc/v2/bg/Main.tex`
- Modify: `doc/v2/en/Main.tex`
- Modify: `doc/v2/bg_presentation/presentation.tex`

### Bulgarian thesis chapter files

- Modify: `doc/v2/bg/chapters/00_abstract.tex`
- Modify: `doc/v2/bg/chapters/01_introduction.tex`
- Modify: `doc/v2/bg/chapters/02_existing_solutions.tex`
- Modify: `doc/v2/bg/chapters/03_theoretical_background.tex`
- Create: `doc/v2/bg/chapters/04_coroutines_async_theory.tex`
- Rename/replace: `doc/v2/bg/chapters/04_orm_architecture.tex` -> `doc/v2/bg/chapters/05_compile_time_orm_architecture.tex`
- Rename/replace: `doc/v2/bg/chapters/05_core_implementation.tex` -> `doc/v2/bg/chapters/06_async_execution_architecture.tex`
- Rename/replace: `doc/v2/bg/chapters/06_backend_scenarios.tex` -> `doc/v2/bg/chapters/07_backend_execution_models.tex`
- Create: `doc/v2/bg/chapters/08_verification_evaluation.tex`
- Create: `doc/v2/bg/chapters/09_extensibility_connectors.tex`
- Rename/replace: `doc/v2/bg/chapters/08_future_work.tex` -> `doc/v2/bg/chapters/10_future_work.tex`
- Rename/replace: `doc/v2/bg/chapters/09_conclusion.tex` -> `doc/v2/bg/chapters/11_conclusion.tex`

### English thesis mirror

- Modify: `doc/v2/en/chapters/00_abstract.tex`
- Modify: `doc/v2/en/chapters/01_introduction.tex`
- Modify: `doc/v2/en/chapters/02_existing_solutions.tex`
- Modify: `doc/v2/en/chapters/03_theoretical_background.tex`
- Create: `doc/v2/en/chapters/04_coroutines_async_theory.tex`
- Rename/replace: `doc/v2/en/chapters/04_orm_architecture.tex` -> `doc/v2/en/chapters/05_compile_time_orm_architecture.tex`
- Rename/replace: `doc/v2/en/chapters/05_core_implementation.tex` -> `doc/v2/en/chapters/06_async_execution_architecture.tex`
- Rename/replace: `doc/v2/en/chapters/06_backend_scenarios.tex` -> `doc/v2/en/chapters/07_backend_execution_models.tex`
- Create: `doc/v2/en/chapters/08_verification_evaluation.tex`
- Create: `doc/v2/en/chapters/09_extensibility_connectors.tex`
- Rename/replace: `doc/v2/en/chapters/08_future_work.tex` -> `doc/v2/en/chapters/10_future_work.tex`
- Rename/replace: `doc/v2/en/chapters/09_conclusion.tex` -> `doc/v2/en/chapters/11_conclusion.tex`

### Legacy files to retire from the compiled document

Stop including these in both `Main.tex` files as soon as the new structure is wired:

- `doc/v2/bg/chapters/10_glossary.tex`
- `doc/v2/bg/chapters/11_verification_appendix.tex`
- `doc/v2/bg/chapters/12_capability_atlas.tex`
- `doc/v2/en/chapters/10_glossary.tex`
- `doc/v2/en/chapters/11_verification_appendix.tex`
- `doc/v2/en/chapters/12_capability_atlas.tex`

Do **not** delete them until their useful content has been migrated into the main chapter flow and both documents build cleanly without them.

---

## Chunk 1: Bibliography and structural scaffolding

### Task 1: Audit and expand bibliography before any thesis drafting

**Files:**
- Modify: `doc/references.bib`
- Read: `docs/superpowers/specs/2026-04-06-orm-thesis-dual-axis-rewrite-design.md`
- Read: `doc/obsidan/03_Architecture/07_Async_Architecture.md`
- Read as evidence source: `benchmarks/bench_async.cpp`

- [ ] **Step 1: Build the bibliography coverage checklist**

Document the required citation buckets from the approved spec:
- coroutine theory / C++ coroutine model;
- asynchronous execution model theory;
- native async APIs for PostgreSQL, MySQL, Redis, Cassandra;
- comparative driver behavior for MongoDB and Neo4j where discussed;
- event-loop / readiness-polling references if explicitly used in prose;
- benchmark methodology references if used in the evaluation chapter.

- [ ] **Step 2: Audit `doc/references.bib` against the checklist**

Mark which required sources already exist and which citation keys are missing.

- [ ] **Step 3: Add missing BibTeX entries**

Add stable, descriptive citation keys for every missing source required by the new theory, architecture, backend, and evaluation chapters.

- [ ] **Step 4: Run BibTeX syntax sanity check**

Run in `doc/v2/bg`:

```bash
pdflatex -interaction=nonstopmode Main.tex
bibtex Main
```

Expected:
- `pdflatex` produces `Main.aux`
- `bibtex` reports no parse/syntax errors in `../../references.bib`

- [ ] **Step 5: Commit bibliography foundation**

```bash
git add doc/references.bib docs/superpowers/specs/2026-04-06-orm-thesis-dual-axis-rewrite-design.md
git commit -m "docs: expand thesis bibliography for async rewrite"
```

### Task 2: Restructure the BG/EN LaTeX document flow around the new chapter map

**Files:**
- Modify: `doc/v2/bg/Main.tex`
- Modify: `doc/v2/en/Main.tex`
- Create/Rename as listed in the File Map above

- [ ] **Step 1: Create the new coroutine theory and split evaluation/extensibility chapter files**

Create the missing BG and EN files:
- `04_coroutines_async_theory.tex`
- `08_verification_evaluation.tex`
- `09_extensibility_connectors.tex`

Use lightweight placeholder headings initially so `Main.tex` can compile while the prose is written incrementally.

- [ ] **Step 2: Rename the old architecture/backend/future/conclusion files to the new numbering**

Perform the BG and EN file renames listed in the File Map so chapter numbering matches the approved logical flow.

- [ ] **Step 3: Update both `Main.tex` files to the new include order**

The compiled order must become:
- abstract;
- introduction;
- existing solutions;
- storage theory;
- coroutine theory;
- compile-time ORM architecture;
- async execution architecture;
- backend execution models;
- verification/evaluation;
- extensibility;
- future work;
- conclusion;
- bibliography.

Remove glossary/appendix/capability-atlas inputs from the compiled flow.

- [ ] **Step 4: Add any shared preamble support needed for multi-page tables**

If the rewritten tables need width-aware page-breaking, add the chosen shared package support (for example `ltablex`) to **both** `Main.tex` files and reuse the existing shared column types instead of inventing per-file hacks.

- [ ] **Step 5: Run missing-input sanity builds**

Run in `doc/v2/bg`:

```bash
pdflatex -interaction=nonstopmode Main.tex
```

Run in `doc/v2/en`:

```bash
pdflatex -interaction=nonstopmode Main.tex
```

Expected:
- no `File ... not found` errors;
- no stale references to glossary/appendix/capability-atlas chapter inputs.

- [ ] **Step 6: Commit structural scaffolding**

```bash
git add doc/v2/bg/Main.tex doc/v2/en/Main.tex doc/v2/bg/chapters doc/v2/en/chapters
git commit -m "docs: restructure thesis chapter flow for dual-axis rewrite"
```

---

## Chunk 2: Bulgarian rewrite — front matter and theory chapters

### Task 3: Rewrite the Bulgarian abstract, introduction, and existing-solutions chapter

**Files:**
- Modify: `doc/v2/bg/chapters/00_abstract.tex`
- Modify: `doc/v2/bg/chapters/01_introduction.tex`
- Modify: `doc/v2/bg/chapters/02_existing_solutions.tex`

- [ ] **Step 1: Rewrite the abstract around the dual-axis framing**

Ensure the abstract states:
- compile-time ORM as the core identity;
- multi-backend scope;
- coroutine-based async execution as a strengthening contribution;
- empirical evaluation as evidence.

- [ ] **Step 2: Rewrite the introduction with the new research problem and hypothesis**

The introduction must state both:
- the compile-time modeling problem;
- the practical execution problem addressed through asynchronous architecture.

- [ ] **Step 3: Rewrite the existing-solutions chapter to include async-positioning gaps**

The comparison must show not only runtime-vs-compile-time differences, but also where existing solutions fail to provide a coherent backend-honest async execution story.

- [ ] **Step 4: Enforce first-use abbreviation expansion while drafting**

Do not leave unexplained short forms such as `ORM`, `IR`, `API`, `SQL`, `CQL`, `BSON`, or `DSL` on first occurrence.

- [ ] **Step 5: Run BG thesis build after front-matter rewrite**

Run in `doc/v2/bg`:

```bash
pdflatex -interaction=nonstopmode Main.tex
bibtex Main
pdflatex -interaction=nonstopmode Main.tex
pdflatex -interaction=nonstopmode Main.tex
```

Expected:
- no LaTeX syntax errors;
- citations resolve for the rewritten front matter.

- [ ] **Step 6: Commit batch A front matter**

```bash
git add doc/v2/bg/chapters/00_abstract.tex doc/v2/bg/chapters/01_introduction.tex doc/v2/bg/chapters/02_existing_solutions.tex
git commit -m "docs: rewrite bg thesis framing for dual-axis architecture"
```

### Task 4: Rewrite the Bulgarian storage-theory and coroutine-theory chapters

**Files:**
- Modify: `doc/v2/bg/chapters/03_theoretical_background.tex`
- Modify/Create: `doc/v2/bg/chapters/04_coroutines_async_theory.tex`

- [ ] **Step 1: Refocus Chapter 3 on ORM and heterogeneous-storage theory only**

Keep this chapter centered on logical schema, physical materialization, backend families, and the boundaries of a unified abstraction.

- [ ] **Step 2: Write the dedicated coroutine theory chapter**

Cover:
- process/thread/coroutine distinctions;
- `co_await`, `co_return`, `co_yield` as needed for scientific explanation;
- compiler lowering to a state machine;
- coroutine frame and promise concepts;
- suspend/resume semantics;
- thread-pool cooperation.

- [ ] **Step 3: Add the core TikZ figures for the theory chapters**

At minimum add:
- storage model taxonomy;
- logical model vs physical materialization;
- process/thread/coroutine comparison;
- coroutine state-machine lowering;
- coroutine frame structure.

- [ ] **Step 4: Validate figure layout while drafting**

Do not allow overlapping nodes or labels. Use `calc`-based positioning and mixed-unit coordinate expressions if needed before any scaling.

- [ ] **Step 5: Run BG thesis build after theory chapters**

Run in `doc/v2/bg`:

```bash
pdflatex -interaction=nonstopmode Main.tex
bibtex Main
pdflatex -interaction=nonstopmode Main.tex
pdflatex -interaction=nonstopmode Main.tex
```

Expected:
- theory chapters compile;
- no missing figure labels;
- no broken citation keys introduced in the new coroutine chapter.

- [ ] **Step 6: Commit batch A theory chapters**

```bash
git add doc/v2/bg/chapters/03_theoretical_background.tex doc/v2/bg/chapters/04_coroutines_async_theory.tex doc/v2/bg/Main.tex
git commit -m "docs: add bg storage and coroutine theory chapters"
```

---

## Chunk 3: Bulgarian rewrite — architecture, backend execution, evaluation, extensibility

### Task 5: Rewrite the compile-time ORM architecture and async execution architecture chapters in Bulgarian

**Files:**
- Modify/Create: `doc/v2/bg/chapters/05_compile_time_orm_architecture.tex`
- Modify/Create: `doc/v2/bg/chapters/06_async_execution_architecture.tex`
- Modify as needed: `doc/v2/bg/Main.tex`

- [ ] **Step 1: Merge the useful content from the old architecture/core-implementation chapters into the new compile-time ORM architecture chapter**

Preserve the original framework core:
- entity model;
- typed query intermediate representation;
- capability gating;
- trait-based materialization.

- [ ] **Step 2: Write the async execution architecture chapter around the actual repository primitives**

Cover:
- `Task<T>`;
- `sync_wait()` as bridge/testing tool;
- `ThreadPool` and `run_on_pool()`;
- `IoContext`;
- `async_db<DB>`;
- `async_connection_pool`;
- `async_transaction_guard`;
- native vs offload async strategies.

- [ ] **Step 3: Add the main architecture TikZ figures**

At minimum add:
- compile-time ORM layered architecture;
- async subsystem overview;
- universal async path;
- native async path;
- async connection-pool lifecycle.

- [ ] **Step 4: Build BG thesis after architecture rewrite**

Run in `doc/v2/bg`:

```bash
pdflatex -interaction=nonstopmode Main.tex
bibtex Main
pdflatex -interaction=nonstopmode Main.tex
pdflatex -interaction=nonstopmode Main.tex
```

Expected:
- no missing inputs or labels;
- figures remain within page width.

- [ ] **Step 5: Commit BG architecture chapters**

```bash
git add doc/v2/bg/chapters/05_compile_time_orm_architecture.tex doc/v2/bg/chapters/06_async_execution_architecture.tex
git commit -m "docs: rewrite bg architecture chapters for async orm thesis"
```

### Task 6: Rewrite the Bulgarian backend-execution, evaluation, extensibility, future-work, and conclusion chapters

**Files:**
- Modify/Create: `doc/v2/bg/chapters/07_backend_execution_models.tex`
- Modify/Create: `doc/v2/bg/chapters/08_verification_evaluation.tex`
- Modify/Create: `doc/v2/bg/chapters/09_extensibility_connectors.tex`
- Modify/Create: `doc/v2/bg/chapters/10_future_work.tex`
- Modify/Create: `doc/v2/bg/chapters/11_conclusion.tex`
- Read/migrate from legacy material as needed: `doc/v2/bg/chapters/11_verification_appendix.tex`, `doc/v2/bg/chapters/12_capability_atlas.tex`

- [ ] **Step 1: Rewrite the backend chapter around execution models, not only data materialization**

Each backend section must explain:
- theoretical basis or reference;
- logical mapping;
- native client-library execution model;
- async integration strategy;
- verification evidence.

- [ ] **Step 2: Rewrite the evaluation chapter with benchmark methodology and results**

Include:
- why the old benchmark shape was misleading;
- workload definition;
- exact numeric tables;
- log-scale throughput graph where justified;
- prose interpretation before and after each visual.

- [ ] **Step 3: Split connector extensibility into its own chapter**

Move the formal connector-contract material out of the overloaded old chapter and make async-capable connector obligations explicit.

- [ ] **Step 4: Rewrite future work and conclusion to match the new achieved state**

Benchmarking is no longer future work in the broad sense. Narrow future-work claims to what remains genuinely unfinished.

- [ ] **Step 5: Migrate any still-useful appendix/atlas content into the main body**

If a matrix, glossary item, or explanatory note is still important, integrate it into the relevant main chapter and then stop depending on the old appendix files.

- [ ] **Step 6: Use breakable tables where long matrices would otherwise float badly**

Prefer `longtable` / `ltablex`-style solutions to avoid large blank areas and to keep evidence tables readable across pages.

- [ ] **Step 7: Run BG thesis build after the full Bulgarian rewrite**

Run in `doc/v2/bg`:

```bash
pdflatex -interaction=nonstopmode Main.tex
bibtex Main
pdflatex -interaction=nonstopmode Main.tex
pdflatex -interaction=nonstopmode Main.tex
```

Expected:
- BG thesis compiles end-to-end;
- no unresolved references remain after the final pass;
- no appendix chapters are still being compiled.

- [ ] **Step 8: Commit the completed BG thesis rewrite**

```bash
git add doc/v2/bg/chapters doc/v2/bg/Main.tex
git commit -m "docs: complete bg dual-axis thesis rewrite"
```

---

## Chunk 4: English mirror and presentation alignment

### Task 7: Mirror the approved Bulgarian thesis structure and claims into English

**Files:**
- Modify: `doc/v2/en/chapters/00_abstract.tex`
- Modify: `doc/v2/en/chapters/01_introduction.tex`
- Modify: `doc/v2/en/chapters/02_existing_solutions.tex`
- Modify: `doc/v2/en/chapters/03_theoretical_background.tex`
- Modify/Create: `doc/v2/en/chapters/04_coroutines_async_theory.tex`
- Modify/Create: `doc/v2/en/chapters/05_compile_time_orm_architecture.tex`
- Modify/Create: `doc/v2/en/chapters/06_async_execution_architecture.tex`
- Modify/Create: `doc/v2/en/chapters/07_backend_execution_models.tex`
- Modify/Create: `doc/v2/en/chapters/08_verification_evaluation.tex`
- Modify/Create: `doc/v2/en/chapters/09_extensibility_connectors.tex`
- Modify/Create: `doc/v2/en/chapters/10_future_work.tex`
- Modify/Create: `doc/v2/en/chapters/11_conclusion.tex`
- Modify: `doc/v2/en/Main.tex`

- [ ] **Step 1: Mirror batch A chapters (front matter + theory) into EN**

Preserve identical claims, citations, and chapter purpose — only translate/adapt language.

- [ ] **Step 2: Mirror batch B chapters (architecture + backend/evaluation/extensibility) into EN**

Do not let figures, tables, or evidence references diverge from BG.

- [ ] **Step 3: Remove appendix/glossary/capability-atlas inputs from the EN compiled document**

EN must follow the same no-appendix rule as BG.

- [ ] **Step 4: Run EN thesis build**

Run in `doc/v2/en`:

```bash
pdflatex -interaction=nonstopmode Main.tex
bibtex Main
pdflatex -interaction=nonstopmode Main.tex
pdflatex -interaction=nonstopmode Main.tex
```

Expected:
- EN thesis compiles;
- figures/tables/citations align with the BG structure.

- [ ] **Step 5: Commit EN mirror**

```bash
git add doc/v2/en/chapters doc/v2/en/Main.tex
git commit -m "docs: synchronize en thesis with dual-axis rewrite"
```

### Task 8: Update the Bulgarian presentation to match the rewritten thesis

**Files:**
- Modify: `doc/v2/bg_presentation/presentation.tex`

- [ ] **Step 1: Replace the old presentation storyline with the new dual-axis framing**

The slides must now show:
- compile-time ORM core;
- coroutine theory motivation in condensed form;
- async subsystem architecture;
- benchmark methodology and results;
- updated conclusion/future-work claims.

- [ ] **Step 2: Add or replace visuals so they match the thesis figures conceptually**

Use concise versions of the architecture and benchmark visuals rather than introducing a contradictory slide narrative.

- [ ] **Step 3: Remove outdated claims from the conclusion slide**

In particular, do not leave “Performance benchmarking” listed as future work once the thesis presents it as completed evaluation work.

- [ ] **Step 4: Build the presentation**

Run in `doc/v2/bg_presentation`:

```bash
lualatex -interaction=nonstopmode presentation.tex
lualatex -interaction=nonstopmode presentation.tex
```

Expected:
- `presentation.pdf` is generated;
- no missing font/package errors;
- no slide overflow that makes content unreadable.

- [ ] **Step 5: Commit presentation alignment**

```bash
git add doc/v2/bg_presentation/presentation.tex
git commit -m "docs: align bg presentation with async thesis rewrite"
```

---

## Chunk 5: Layout, editorial, and repository-wide verification

### Task 9: Perform final layout and editorial stabilization across BG/EN thesis documents

**Files:**
- Modify as needed: `doc/v2/bg/Main.tex`
- Modify as needed: `doc/v2/en/Main.tex`
- Modify as needed: chapter files under `doc/v2/bg/chapters/` and `doc/v2/en/chapters/`

- [ ] **Step 1: Inspect generated PDFs and logs for layout problems**

Check for:
- overlapping TikZ nodes/arrows/labels;
- table overflow or unreadably compressed tables;
- figures extending outside the text block;
- unresolved citations or references;
- overfull/underfull issues severe enough to hurt readability.

- [ ] **Step 2: Convert bad floats into breakable tables where needed**

When a long table causes large blank spaces or unreadable scaling, replace it with a breakable table construction instead of shrinking it aggressively.

- [ ] **Step 3: Audit first-use abbreviations in BG and EN**

Fix any remaining first-use violations such as unexplained `ORM`, `IR`, `API`, `SQL`, `CQL`, `BSON`, `DSL`, `CPU`, or `I/O` occurrences.

- [ ] **Step 4: Ensure all important appendix-era material now lives in the main body**

Only after this check passes may the legacy glossary/appendix/capability-atlas files be deleted or left permanently unreferenced.

- [ ] **Step 5: Re-run final document builds**

Run in `doc/v2/bg`:

```bash
pdflatex -interaction=nonstopmode Main.tex
bibtex Main
pdflatex -interaction=nonstopmode Main.tex
pdflatex -interaction=nonstopmode Main.tex
```

Run in `doc/v2/en`:

```bash
pdflatex -interaction=nonstopmode Main.tex
bibtex Main
pdflatex -interaction=nonstopmode Main.tex
pdflatex -interaction=nonstopmode Main.tex
```

Run in `doc/v2/bg_presentation`:

```bash
lualatex -interaction=nonstopmode presentation.tex
lualatex -interaction=nonstopmode presentation.tex
```

Expected:
- BG thesis builds;
- EN thesis builds;
- BG presentation builds;
- no major readability issues remain unresolved.

- [ ] **Step 6: Commit final documentation cleanup**

```bash
git add doc/v2/bg doc/v2/en doc/v2/bg_presentation
git commit -m "docs: finalize thesis rewrite layout and editorial cleanup"
```

### Task 10: Satisfy repository-wide verification rules after documentation changes

**Files:**
- No expected source modifications unless verification uncovers collateral issues

- [ ] **Step 1: Reconfigure or refresh the default build directory if needed**

Run in repository root:

```bash
cmake -S . -B build
```

Expected:
- CMake configure/generate succeeds.

- [ ] **Step 2: Build all targets**

Run in repository root:

```bash
cmake --build build
```

Expected:
- all configured targets compile and link successfully.

- [ ] **Step 3: Run the full test suite**

Run in repository root:

```bash
ctest --test-dir build -j1 --output-on-failure
```

Expected:
- all tests pass;
- no regressions appear from the documentation-side changes.

- [ ] **Step 4: Commit only if verification required follow-up fixes**

```bash
git add <only files changed during verification>
git commit -m "chore: fix verification issues after thesis rewrite"
```

---

Plan complete and saved to `docs/superpowers/plans/2026-04-06-orm-thesis-dual-axis-rewrite-implementation-plan.md`. Begin execution with **Chunk 1 / Task 1** using `@superpowers/subagent-driven-development` when available.
