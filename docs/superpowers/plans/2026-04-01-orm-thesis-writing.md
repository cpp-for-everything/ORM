# ORM Thesis Writing Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Write the bilingual ORM diploma thesis in `doc/v2/bg` and `doc/v2/en`, aligned with the approved thesis structure and synchronized across both languages.

**Architecture:** The thesis will be implemented as a clean LaTeX chapter set with matching BG/EN chapter files and synchronized `Main.tex` orchestration. Existing English material will be restructured and updated where accurate, while missing Bulgarian chapters will be authored from scratch using repository code and tests as the evidence source.

**Tech Stack:** LaTeX, BibTeX, TikZ, existing `doc/v2` thesis templates, ORM source code and test suite

---

## Chunk 1: Structural alignment of the thesis documents

### Task 1: Align chapter file layout and main document includes

**Files:**
- Modify: `doc/v2/bg/Main.tex`
- Modify: `doc/v2/en/Main.tex`
- Create: `doc/v2/bg/chapters/00_abstract.tex`
- Create: `doc/v2/bg/chapters/01_introduction.tex`
- Create: `doc/v2/bg/chapters/02_existing_solutions.tex`
- Create: `doc/v2/bg/chapters/03_theoretical_background.tex`
- Create: `doc/v2/bg/chapters/04_orm_architecture.tex`
- Create: `doc/v2/bg/chapters/05_core_implementation.tex`
- Create: `doc/v2/bg/chapters/06_backend_scenarios.tex`
- Create: `doc/v2/bg/chapters/07_verification_extensibility.tex`
- Create: `doc/v2/bg/chapters/08_future_work.tex`
- Create: `doc/v2/bg/chapters/09_conclusion.tex`
- Create: `doc/v2/bg/chapters/10_glossary.tex`
- Create: `doc/v2/en/chapters/05_core_implementation.tex`
- Create: `doc/v2/en/chapters/06_backend_scenarios.tex`
- Create: `doc/v2/en/chapters/07_verification_extensibility.tex`
- Create: `doc/v2/en/chapters/08_future_work.tex`
- Create: `doc/v2/en/chapters/09_conclusion.tex`
- Create: `doc/v2/en/chapters/10_glossary.tex`

- [ ] Update `doc/v2/bg/Main.tex` to include the full approved chapter order and use the approved `Compile-time Object-Relational Mapping библиотека` phrasing on the title page.
- [ ] Update `doc/v2/en/Main.tex` to include the synchronized chapter order.
- [ ] Create the missing Bulgarian chapter files with matching filenames.
- [ ] Create the missing English chapter files required by the new approved structure.
- [ ] Ensure both `Main.tex` files point only to the synchronized chapter set.

### Task 2: Preserve repository-backed evidence references

**Files:**
- Modify: `doc/v2/bg/chapters/*.tex`
- Modify: `doc/v2/en/chapters/*.tex`

- [ ] Ensure every chapter references only code and tests that exist in the repository.
- [ ] Keep exact identifier names such as `connector_trait`, `store_as`, `property`, `table_name_trait`, and `db<DB>`.
- [ ] Ensure backend scenario sections explicitly reference their theoretical basis.

---

## Chunk 2: Author the main thesis body in Bulgarian and synchronize English mirror

### Task 3: Write front matter and foundational chapters

**Files:**
- Modify/Create: `doc/v2/bg/chapters/00_abstract.tex`
- Modify/Create: `doc/v2/bg/chapters/01_introduction.tex`
- Modify/Create: `doc/v2/bg/chapters/02_existing_solutions.tex`
- Modify/Create: `doc/v2/bg/chapters/03_theoretical_background.tex`
- Modify/Create: `doc/v2/en/chapters/00_abstract.tex`
- Modify/Create: `doc/v2/en/chapters/01_introduction.tex`
- Modify/Create: `doc/v2/en/chapters/02_existing_solutions.tex`
- Modify/Create: `doc/v2/en/chapters/03_theoretical_background.tex`

- [ ] Write the Bulgarian abstract with the approved framing and explicit multi-backend scope.
- [ ] Write the Bulgarian introduction and literature review.
- [ ] Write the Bulgarian theoretical chapter covering ORM theory and storage models.
- [ ] Update the English mirror to preserve the same structure and claims.

### Task 4: Write architecture and implementation chapters

**Files:**
- Modify/Create: `doc/v2/bg/chapters/04_orm_architecture.tex`
- Modify/Create: `doc/v2/bg/chapters/05_core_implementation.tex`
- Modify/Create: `doc/v2/en/chapters/04_orm_architecture.tex`
- Modify/Create: `doc/v2/en/chapters/05_core_implementation.tex`

- [ ] Write the architecture chapter around entity modeling, query IR, connector dispatch, capabilities, and migration flow.
- [ ] Write the core implementation chapter around property mapping, relationship strategy, placeholders, prepared queries, result hydration, migration mechanics, and thread-safety helpers.
- [ ] Synchronize the English mirror.

### Task 5: Write backend scenarios, verification/extensibility, future work, conclusion, glossary

**Files:**
- Modify/Create: `doc/v2/bg/chapters/06_backend_scenarios.tex`
- Modify/Create: `doc/v2/bg/chapters/07_verification_extensibility.tex`
- Modify/Create: `doc/v2/bg/chapters/08_future_work.tex`
- Modify/Create: `doc/v2/bg/chapters/09_conclusion.tex`
- Modify/Create: `doc/v2/bg/chapters/10_glossary.tex`
- Modify/Create: `doc/v2/en/chapters/06_backend_scenarios.tex`
- Modify/Create: `doc/v2/en/chapters/07_verification_extensibility.tex`
- Modify/Create: `doc/v2/en/chapters/08_future_work.tex`
- Modify/Create: `doc/v2/en/chapters/09_conclusion.tex`
- Modify/Create: `doc/v2/en/chapters/10_glossary.tex`

- [ ] Write scenario chapters for SQLite, PostgreSQL/MySQL, MongoDB, Redis, Neo4j, and Cassandra.
- [ ] Write the connector extensibility chapter with formal requirements and completeness criteria.
- [ ] Write future work, conclusion, and synchronized glossary.
- [ ] Ensure the English mirror remains translation-accurate and technically equivalent.

---

## Chunk 3: Validate LaTeX structure and repository consistency

### Task 6: Verify document structure and buildability

**Files:**
- Modify as needed: `doc/v2/bg/Main.tex`
- Modify as needed: `doc/v2/en/Main.tex`
- Modify as needed: chapter files under `doc/v2/bg/chapters/` and `doc/v2/en/chapters/`

- [ ] Run a full LaTeX build flow for the thesis documents.
- [ ] Fix missing inputs, broken labels, bibliography references, and obvious LaTeX syntax problems.
- [ ] Re-check that BG and EN chapter structures remain synchronized.
- [ ] Confirm that no stale chapter inputs remain in either `Main.tex`.

### Task 7: Final documentation quality pass

**Files:**
- Modify as needed: all thesis chapter files

- [ ] Normalize terminology across BG and EN versions.
- [ ] Ensure all figures/tables/listings referenced in text are either present or intentionally deferred.
- [ ] Ensure the final thesis text consistently uses repository-backed evidence.
- [ ] Prepare the documents for final academic review.

---

Plan complete and saved to `docs/superpowers/plans/2026-04-01-orm-thesis-writing.md`. Ready to execute.
