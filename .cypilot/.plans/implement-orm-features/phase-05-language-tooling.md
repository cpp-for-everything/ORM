---
phase: 5
title: "C++26 Reflection & Schema Migration FEATURE specs"
slug: language-tooling
status: pending
kind: delivery
depends_on: [4]
output_files:
  - architecture/features/FEATURE-cpp26-reflection.md
  - architecture/features/FEATURE-schema-migration.md
outputs:
  - .cypilot/.plans/implement-orm-features/out/phase-05-ids.md
inputs:
  - .cypilot/.plans/implement-orm-features/out/phase-01-ids.md
  - .cypilot/.plans/implement-orm-features/out/phase-02-ids.md
  - .cypilot/.plans/implement-orm-features/out/phase-03-ids.md
  - .cypilot/.plans/implement-orm-features/out/phase-04-ids.md
---

--- CONTEXT BOUNDARY ---
Disregard all previous context. This phase file is self-contained.
Read ONLY the files listed in Prior Context. Follow these instructions exactly.
---

## What

Generate two FEATURE spec artifacts:
1. `architecture/features/FEATURE-cpp26-reflection.md` — C++26 `std::meta` reflection integration
2. `architecture/features/FEATURE-schema-migration.md` — `orm::migrate<DB>` schema migration and validation tool

## Prior Context

Read these files before proceeding:

1. `doc/v2/bg/chapters/05_future_work.tex` lines 61–68 — C++26 reflection and schema migration source descriptions
2. `.cypilot/config/kits/sdlc/artifacts/FEATURE/template.md` — full template
3. `.cypilot/config/kits/sdlc/artifacts/FEATURE/rules.md` — sections: Structural, Versioning, Semantic, Scope, Checkbox Management, Deliberate Omissions
4. `.cypilot/.plans/implement-orm-features/out/phase-04-ids.md` — Phase 4 IDs (load to check for duplicates; earlier phase files are transitively covered by phase-04-ids via cumulative naming convention)

## User Decisions

- **System**: `orm`
- **ID prefix**: `cpt-orm-`
- **Feature slugs**: `cpp26-reflection`, `schema-migration`
- **No DECOMPOSITION / DESIGN** → "DESIGN pending" in frontmatter and References
- **Output destination**: file
- **Lifecycle**: archive (Phase 6)

## Rules (verbatim from rules.md — MUST enforce all)

### Structural MUST
- FEATURE follows `template.md` structure exactly
- All flows, algorithms, states, DoD items have unique IDs
- All IDs follow `cpt-{system}-{kind}-{slug}` pattern
- All IDs have priority markers (`p1`–`p9`)
- Include feature slug in `{slug}` portion of IDs
- CDSL instructions: `N. [ ] - \`pN\` - Description - \`inst-slug\``
- No placeholder content (TODO, TBD, FIXME)
- No duplicate IDs within document or across prior phases (1–4)

### Structural MUST NOT
- MUST NOT redefine system-level types — **ARCH-FDESIGN-NO-001**
- MUST NOT define new API endpoints — **ARCH-FDESIGN-NO-002**
- MUST NOT include architectural decisions — **ARCH-FDESIGN-NO-003**
- MUST NOT include product requirements — **BIZ-FDESIGN-NO-001**
- MUST NOT include sprint/task breakdowns — **BIZ-FDESIGN-NO-002**
- MUST NOT include code snippets — **MAINT-FDESIGN-NO-001**
- MUST NOT include test implementation — **TEST-FDESIGN-NO-001**
- MUST NOT include security secrets — **SEC-FDESIGN-NO-001**
- MUST NOT include infrastructure code — **OPS-FDESIGN-NO-001**

### Versioning MUST
- New artifact: version `1.0` in frontmatter; changelog with initial entry

### Semantic MUST
- Actor flows define complete user journeys
- Algorithms specify processing logic clearly
- DoD items are testable and traceable
- CDSL instructions describe "what" not "how"
- Control flow keywords: IF, RETURN, FROM/TO/WHEN, FOR EACH, TRY/CATCH

### Featstatus MUST
- `cpt-orm-featstatus-{feature-slug}` defined directly under H1, before `## Feature Context`
- Checkbox `[ ]` (unchecked)

### Checkbox Management
- All flow/algo/state/dod checkboxes `[ ]`

## Source Material

### C++26 Reflection (from future_work.tex lines 61–63)
- C++26 introduces `std::meta` reflection for compile-time inspection of struct member names
- Existing fallback: Boost.PFR (already used on pre-C++26 compilers)
- When compiled with C++26-capable compiler: eliminate mandatory string arg `"col_name"` from `property<T, "col_name">`
- Column name inferred automatically from struct member name via `std::meta::name_of`
- Explicit string arg remains available as column name override
- Both paths co-exist; selection via `#ifdef __cpp_impl_reflection`
- C++26 status: GCC 16 (merged Nov 2025), Bloomberg clang fork (experimental), MSVC (nothing)

### Schema Migration (from future_work.tex lines 65–68)
- `orm::migrate<DB>` tool compares C++ entity declarations with live database schema
- Generates required `ALTER TABLE` / `CREATE TABLE` statements
- Schema encoded in C++ types → migration tool provides compile-time guarantees:
  - No column referenced by a query has been deleted or renamed without a corresponding entity struct update

### ORM Architecture context
- `property<CppType, "col_name">` — string arg currently mandatory on PFR path
- On C++26 path: `property<int> id;` — field name inferred; string arg accepted as override
- `connector_trait<DB>`: dumb tag type; ORM core calls `connector_trait<DB>::execute(conn, query_ir, params...)`
- Query IR: fully type-level constexpr fluent builder

## Task

### Step 1 — Verify prior IDs loaded
Read `out/phase-04-ids.md`. Keep ID list in context to prevent duplicates.

### Step 2 — Generate FEATURE-cpp26-reflection.md

**Section 1: Feature Context**
- Feature ID: `cpt-orm-feature-cpp26-reflection`
- Overview: Integration of C++26 `std::meta` reflection to enable compile-time column name inference from struct member names, eliminating the mandatory `"col_name"` string argument in `property<T, "col_name">` on C++26-capable compilers
- Purpose: Reduce boilerplate in entity definitions while preserving backward compatibility via the existing Boost.PFR fallback path and optional explicit string override
- Actors: Developer (library user)
- References: DESIGN pending; no DECOMPOSITION

**Section 2: Actor Flows (CDSL)**
- Flow 1: Developer defines an entity using C++26 reflection (string-free)
  - ID: `cpt-orm-flow-cpp26-reflection-define-entity`
  - Steps: check `__cpp_impl_reflection` defined → declare `property<int> id;` without string arg → compiler invokes `std::meta::name_of` at compile time → column name `"id"` inferred → entity usable in query builder without change
- Flow 2: Developer overrides inferred column name
  - ID: `cpt-orm-flow-cpp26-reflection-override-name`
  - Steps: declare `property<int, "user_id"> id;` → explicit string takes precedence over reflection inference → column maps to `user_id` in database

**Section 3: Processes / Business Logic (CDSL)**
- Algo 1: Compile-time column name resolution — ID: `cpt-orm-algo-cpp26-reflection-resolve-name`
  - Input: `property<T>` or `property<T, "name">` declaration at compile time
  - Output: `constexpr std::string_view` column name
  - Steps: IF explicit string arg provided → use string arg → ELSE IF `__cpp_impl_reflection` defined → use `std::meta::name_of(^^member)` → ELSE (PFR path) → use `boost::pfr::get_name<N, Entity>()`
- Algo 2: PFR fallback path — ID: `cpt-orm-algo-cpp26-reflection-pfr-fallback`
  - Input: `property<T, "col_name">` on pre-C++26 compiler
  - Output: column name from explicit string arg (mandatory on this path)
  - Steps: static_assert that string arg present when `__cpp_impl_reflection` not defined

**Section 4: States** — Not applicable (reflection is a compile-time mechanism; note explicitly)

**Section 5: Definitions of Done**
- DoD 1: `cpt-orm-dod-cpp26-reflection-inferred-name` — `property<int> id;` produces column name `"id"` on C++26 compiler
- DoD 2: `cpt-orm-dod-cpp26-reflection-explicit-override` — explicit string arg overrides inferred name correctly
- DoD 3: `cpt-orm-dod-cpp26-reflection-pfr-fallback` — PFR path continues to work on pre-C++26 compilers; mandatory string arg enforced via static_assert
- DoD 4: `cpt-orm-dod-cpp26-reflection-coexistence` — both paths compile in same codebase via `#ifdef __cpp_impl_reflection`

**Section 6: Acceptance Criteria** — one testable criterion per DoD

### Step 3 — Generate FEATURE-schema-migration.md

**Section 1: Feature Context**
- Feature ID: `cpt-orm-feature-schema-migration`
- Overview: `orm::migrate<DB>` tool that compares C++ entity declarations against the live database schema and generates `ALTER TABLE` / `CREATE TABLE` DDL statements required to bring the database schema into alignment
- Purpose: Provide schema lifecycle management with compile-time safety guarantees — no column referenced by a compiled query can be silently deleted or renamed without the entity struct being updated first
- Actors: Developer (running migration), CI pipeline (automated schema drift detection)
- References: DESIGN pending; no DECOMPOSITION

**Section 2: Actor Flows (CDSL)**
- Flow 1: Developer runs schema migration against a live database
  - ID: `cpt-orm-flow-schema-migration-run-migration`
  - Steps: instantiate `orm::migrate<DB>` → connect to database → introspect live schema (INFORMATION_SCHEMA or equivalent) → diff entity C++ types vs live schema → generate DDL → present DDL for review → execute DDL on approval → report changes applied
- Flow 2: CI pipeline detects schema drift
  - ID: `cpt-orm-flow-schema-migration-detect-drift`
  - Steps: run `orm::migrate<DB>` in dry-run mode → IF diff non-empty: exit non-zero (fail CI) → ELSE: exit zero (schema in sync)

**Section 3: Processes / Business Logic (CDSL)**
- Algo 1: Entity-to-DDL diff — ID: `cpt-orm-algo-schema-migration-diff`
  - Input: compiled entity type list (from ORM registration), live schema (fetched from DB)
  - Output: ordered list of DDL operations (CREATE TABLE, ADD COLUMN, DROP COLUMN, ALTER COLUMN, etc.)
  - Steps: FOR EACH entity → compare declared columns vs live columns → IF new column: emit ADD COLUMN → IF removed column: emit DROP COLUMN → IF type changed: emit ALTER COLUMN → IF table absent: emit CREATE TABLE
- Algo 2: Compile-time column reference safety check — ID: `cpt-orm-algo-schema-migration-ref-safety`
  - Input: query IR referencing columns at compile time, entity struct declarations
  - Output: compile error if referenced column absent from entity struct
  - Steps: at compile time, for each column ref in IR → static_assert that column exists in entity → compile failure prevents stale query from reaching runtime
- Algo 3: DDL generation — ID: `cpt-orm-algo-schema-migration-generate-ddl`
  - Input: diff operation list
  - Output: DDL string (dialect-specific per `connector_trait<DB>`)
  - Steps: for each diff op → delegate to `connector_trait<DB>::ddl_for(op)` → concatenate → return DDL string

**Section 4: States — Migration State Machine**
- State machine ID: `cpt-orm-state-schema-migration-run`
- States: `Idle`, `Connected`, `Diffing`, `Reviewing`, `Executing`, `Done`, `Failed`
- Initial state: `Idle`
- Transitions:
  - FROM `Idle` TO `Connected` WHEN database connection established
  - FROM `Connected` TO `Diffing` WHEN schema introspection complete
  - FROM `Diffing` TO `Reviewing` WHEN diff computed (non-empty)
  - FROM `Diffing` TO `Done` WHEN diff empty (schema in sync)
  - FROM `Reviewing` TO `Executing` WHEN DDL approved
  - FROM `Reviewing` TO `Done` WHEN migration cancelled
  - FROM `Executing` TO `Done` WHEN all DDL statements executed successfully
  - FROM `Executing` TO `Failed` WHEN DDL execution error
  - FROM `Connected` TO `Failed` WHEN connection error during introspection

**Section 5: Definitions of Done**
- DoD 1: `cpt-orm-dod-schema-migration-diff-compute` — diff correctly identifies CREATE TABLE, ADD COLUMN, DROP COLUMN operations
- DoD 2: `cpt-orm-dod-schema-migration-ddl-generate` — generated DDL is syntactically valid for the target database dialect
- DoD 3: `cpt-orm-dod-schema-migration-dry-run` — dry-run mode outputs DDL without executing; exits non-zero if diff non-empty
- DoD 4: `cpt-orm-dod-schema-migration-compile-time-safety` — compile error raised when query references a column absent from the entity struct
- DoD 5: `cpt-orm-dod-schema-migration-connector-ddl-trait` — `connector_trait<DB>::ddl_for(op)` specialised for SQLite and MockDB

**Section 6: Acceptance Criteria** — one testable criterion per DoD

### Step 4 — Write intermediate ID registry
Write `.cypilot/.plans/implement-orm-features/out/phase-05-ids.md` listing all new IDs from this phase.

### Step 5 — Update artifacts.toml
Add both FEATURE artifact paths to `[[systems]]` entry for `orm`.

### Step 6 — Confirm before writing
Present summary of both artifacts and ask `yes/no/modify`.

## Acceptance Criteria

- [ ] Both FEATURE files written to `architecture/features/`
- [ ] Template structure followed (sections 1–6)
- [ ] `cpt-orm-featstatus-{slug}` defined under H1 in each file
- [ ] No IDs duplicated from Phases 1–4
- [ ] No duplicate IDs within this phase
- [ ] All IDs `cpt-orm-{kind}-{feature-slug}-{slug}` with priority markers
- [ ] No placeholder content; no MUST NOT violations
- [ ] Migration state machine present in schema-migration FEATURE (Section 4)
- [ ] All checkboxes `[ ]`
- [ ] `out/phase-05-ids.md` written
- [ ] `artifacts.toml` updated

## Output Format

```text
Phase 5/6: Language & Tooling — DONE

Files written:
  ✓ architecture/features/FEATURE-cpp26-reflection.md  (~N lines)
  ✓ architecture/features/FEATURE-schema-migration.md  (~N lines)
  ✓ .cypilot/.plans/implement-orm-features/out/phase-05-ids.md  (N IDs)
  ✓ .cypilot/config/artifacts.toml  (2 entries added)

IDs defined (Phase 5):
  [list all cpt-orm-* IDs]

Validation:
  Deterministic gate: SKIPPED
  Validator availability proof: cpt script not installed; no registered artifact path for validate --artifact
  Skip reason: cypilot.py scripts directory absent from this installation
  Validator-backed evidence note: none; deterministic validation was skipped
  Semantic review: template structure followed; no placeholders; no MUST NOT violations; migration state machine present; all IDs unique across phases 1–5
```

## Phase Handoff

After completion, update `plan.toml`: set `phases[4].status = "done"`.

```text
I have a Cypilot execution plan at:
  .cypilot/.plans/implement-orm-features/plan.toml

Phase 5 is complete (done).
Please read the plan manifest, confirm the next executable phase, and execute it.
The expected next phase file is: .cypilot/.plans/implement-orm-features/phase-06-archive.md
The phase file is self-contained — follow its instructions exactly.
```
