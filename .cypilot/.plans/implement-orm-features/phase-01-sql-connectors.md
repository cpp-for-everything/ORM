---
phase: 1
title: "MySQL & PostgreSQL FEATURE specs"
slug: sql-connectors
status: pending
kind: delivery
depends_on: []
output_files:
  - architecture/features/FEATURE-mysql-connector.md
  - architecture/features/FEATURE-postgresql-connector.md
outputs:
  - .cypilot/.plans/implement-orm-features/out/phase-01-ids.md
---

--- CONTEXT BOUNDARY ---
Disregard all previous context. This phase file is self-contained.
Read ONLY the files listed in Prior Context. Follow these instructions exactly.
---

## What

Generate two FEATURE spec artifacts:
1. `architecture/features/FEATURE-mysql-connector.md` — MySQL/MariaDB connector
2. `architecture/features/FEATURE-postgresql-connector.md` — PostgreSQL connector

Both describe future implementation targets for the ORM library's `connector_trait` specialisations.

## Prior Context

Read these files before proceeding:

1. `doc/v2/bg/chapters/05_future_work.tex` lines 11–17 — MySQL/MariaDB and PostgreSQL source descriptions
2. `.cypilot/config/kits/sdlc/artifacts/FEATURE/template.md` — full template (structure to follow)
3. `.cypilot/config/kits/sdlc/artifacts/FEATURE/rules.md` — rules sections: Structural, Versioning, Semantic, Scope, Checkbox Management, Deliberate Omissions

## User Decisions

- **System**: `orm`
- **ID prefix**: `cpt-orm-`
- **Feature slugs**: `mysql-connector`, `postgresql-connector`
- **No DECOMPOSITION exists** → add `status: "DESIGN pending — no DECOMPOSITION"` in frontmatter; skip parent feature reference
- **No DESIGN exists** → skip component/type cross-references; note "DESIGN pending" in References section
- **Output destination**: file (write to disk after generation)
- **Lifecycle**: archive (handled in Phase 6)

## Rules (verbatim from rules.md — MUST enforce all)

### Structural MUST
- FEATURE follows `template.md` structure exactly
- All flows, algorithms, states, DoD items have unique IDs
- All IDs follow `cpt-{system}-{kind}-{slug}` pattern
- All IDs have priority markers (`p1`–`p9`)
- Include feature slug in `{slug}` portion of flow/algo/state/dod IDs
- CDSL instructions follow format: `N. [ ] - \`pN\` - Description - \`inst-slug\``
- No placeholder content (TODO, TBD, FIXME)
- No duplicate IDs within document

### Structural MUST NOT
- MUST NOT redefine system-level types (belongs in DESIGN) — **ARCH-FDESIGN-NO-001**
- MUST NOT define new API endpoints (belongs in DESIGN) — **ARCH-FDESIGN-NO-002**
- MUST NOT include architectural decisions (belongs in ADR) — **ARCH-FDESIGN-NO-003**
- MUST NOT include product requirements (belongs in PRD) — **BIZ-FDESIGN-NO-001**
- MUST NOT include sprint/task breakdowns (belongs in DECOMPOSITION) — **BIZ-FDESIGN-NO-002**
- MUST NOT include code snippets (belongs in implementation) — **MAINT-FDESIGN-NO-001**
- MUST NOT include test implementation (belongs in implementation) — **TEST-FDESIGN-NO-001**
- MUST NOT include security secrets — **SEC-FDESIGN-NO-001**
- MUST NOT include infrastructure code — **OPS-FDESIGN-NO-001**

### Versioning MUST
- New artifact: version `1.0` in frontmatter
- Keep changelog section with initial creation entry

### Semantic MUST
- Actor flows define complete user journeys
- Algorithms specify processing logic clearly
- DoD items are testable and traceable
- CDSL instructions describe "what" not "how"
- Control flow keywords used correctly: IF, RETURN, FROM/TO/WHEN, FOR EACH, TRY/CATCH

### Featstatus MUST
- Define `cpt-orm-featstatus-{feature-slug}` ID directly under H1, before `## Feature Context`
- Featstatus checkbox `[ ]` (unchecked — feature not yet implemented)

### Checkbox Management
- All flow/algo/state/dod checkboxes `[ ]` (unchecked — not yet implemented)

## Source Material

### MySQL/MariaDB (from future_work.tex lines 11–13)
- `connector_trait<MySQLDB>` translates query IR to MySQL C API:
  - `mysql_stmt_prepare`, `mysql_stmt_bind_param`, `mysql_stmt_execute`
- Indexed parameter syntax `?NNN` not natively supported by MySQL
- Connector must rewrite indexed placeholders by either:
  - Duplicating the argument at each occurrence in positional bind list, OR
  - Using MySQL named parameter syntax (`:name`)

### PostgreSQL (from future_work.tex lines 15–17)
- Uses libpq: `PQprepare` + `PQexecPrepared` for prepared statements
- Uses `$1`, `$2`, … positional parameters instead of `?`
- Indexed placeholder reuse natively supported — emit same `$N` token at each reuse position

### ORM Architecture context (from project memory)
- `connector_trait<DB>`: dumb tag type, trait holds all logic
- Capability tags as nested types: `supports_joins`, `supports_transactions`, etc.
- Missing capability + usage = `static_assert` compile error
- ORM core calls `connector_trait<DB>::execute(conn, query_ir, params...)`
- Query IR: SQL-superset constexpr fluent builder, fully type-level

## Task

### Step 1 — Create output directory
EXECUTE: `New-Item -ItemType Directory -Force -Path architecture/features` (PowerShell) or equivalent

### Step 2 — Generate FEATURE-mysql-connector.md
Using `template.md` structure, generate a complete FEATURE document with:

**Section 1: Feature Context**
- Feature ID: `cpt-orm-feature-mysql-connector`
- Overview: `connector_trait<MySQLDB>` specialisation that translates the ORM query IR to MySQL C API prepared-statement calls
- Purpose: Enable the ORM to target MySQL and MariaDB databases without runtime SQL string construction
- Actors: table showing developer as actor (no PRD actor IDs — DESIGN pending)
- References: DESIGN pending, no DECOMPOSITION

**Section 2: Actor Flows (CDSL)**
- Flow: Developer registers MySQLDB connector and executes a query
- ID: `cpt-orm-flow-mysql-connector-execute-query`
- Steps covering: configure connector → build compile-time query → call execute → connector translates IR → bind params → execute prepared statement → return result

**Section 3: Processes / Business Logic (CDSL)**
- Algo 1: IR-to-MySQL-SQL rendering — ID: `cpt-orm-algo-mysql-connector-render-sql`
  - Input: query IR (constexpr), runtime param values
  - Output: MySQL SQL string with `?` placeholders + ordered param bind list
  - Steps: walk IR tree → emit SQL tokens → for indexed placeholders rewrite to positional duplicates
- Algo 2: Indexed-placeholder rewrite — ID: `cpt-orm-algo-mysql-connector-rewrite-indexed-ph`
  - Input: indexed placeholder list, IR position map
  - Output: positional `?` list with duplicated args at reuse sites

**Section 4: States** — Not applicable (connector is stateless; state machine omitted with explicit note)

**Section 5: Definitions of Done**
- DoD 1: `cpt-orm-dod-mysql-connector-trait-specialisation` — `connector_trait<MySQLDB>` fully specialised and compiles
- DoD 2: `cpt-orm-dod-mysql-connector-prepared-stmt` — prepared statement lifecycle (prepare/bind/execute/finalize) exercised
- DoD 3: `cpt-orm-dod-mysql-connector-indexed-ph-rewrite` — indexed placeholder rewrite produces correct positional bind list
- DoD 4: `cpt-orm-dod-mysql-connector-capability-tags` — capability tags (`supports_joins`, `supports_transactions`) declared and enforced

**Section 6: Acceptance Criteria**
- Testable criteria for each DoD item

### Step 3 — Generate FEATURE-postgresql-connector.md
Using same template structure:

**Section 1: Feature Context**
- Feature ID: `cpt-orm-feature-postgresql-connector`
- Overview: `connector_trait<PostgreSQLDB>` specialisation using libpq `PQprepare` / `PQexecPrepared`
- Purpose: Enable ORM to target PostgreSQL with native `$N` positional parameters and indexed placeholder reuse

**Section 2: Actor Flows (CDSL)**
- Flow: Developer executes prepared query against PostgreSQL
- ID: `cpt-orm-flow-postgresql-connector-execute-prepared`
- Steps: configure connector → build query → call execute → render `$N` SQL → PQprepare → PQexecPrepared → hydrate result

**Section 3: Processes / Business Logic (CDSL)**
- Algo 1: IR-to-PostgreSQL-SQL rendering — ID: `cpt-orm-algo-postgresql-connector-render-sql`
  - Emit `$1`, `$2`, … for each parameter position; for indexed placeholder reuse emit same `$N` at each site
- Algo 2: Result hydration — ID: `cpt-orm-algo-postgresql-connector-hydrate-result`
  - Map PGresult columns to C++ entity fields via type system

**Section 4: States** — Not applicable (connector stateless)

**Section 5: Definitions of Done**
- DoD 1: `cpt-orm-dod-postgresql-connector-trait-specialisation`
- DoD 2: `cpt-orm-dod-postgresql-connector-prepared-stmt`
- DoD 3: `cpt-orm-dod-postgresql-connector-dollar-params` — `$N` parameter rendering correct
- DoD 4: `cpt-orm-dod-postgresql-connector-indexed-ph-reuse` — same `$N` token emitted at reuse sites

**Section 6: Acceptance Criteria** — testable criteria for each DoD

### Step 4 — Write intermediate ID registry
Write `.cypilot/.plans/implement-orm-features/out/phase-01-ids.md` listing all IDs defined in this phase (for duplicate-checking in later phases).

### Step 5 — Update artifacts.toml
Add both FEATURE artifact paths to `[[systems]]` entry for `orm` in `.cypilot/config/artifacts.toml`.

### Step 6 — Confirm before writing
Present a summary of both artifacts and ask for `yes/no/modify` before writing files to disk.

## Acceptance Criteria

- [ ] Both FEATURE files written to `architecture/features/`
- [ ] Each file follows `template.md` H2 structure exactly (sections 1–6)
- [ ] `cpt-orm-featstatus-{slug}` defined directly under H1 in each file
- [ ] No duplicate IDs within or across the two files
- [ ] All IDs follow `cpt-orm-{kind}-{feature-slug}-{slug}` pattern with `p1`–`p9` markers
- [ ] No placeholder content (TODO, TBD, FIXME)
- [ ] No MUST NOT violations (no code snippets, no arch decisions, no API definitions)
- [ ] All checkboxes `[ ]` (features not yet implemented)
- [ ] DESIGN pending noted in References section of each file
- [ ] `out/phase-01-ids.md` written listing all IDs
- [ ] `artifacts.toml` updated with both artifact paths

## Output Format

Report on completion:

```text
Phase 1/6: SQL Connectors — DONE

Files written:
  ✓ architecture/features/FEATURE-mysql-connector.md  (~N lines)
  ✓ architecture/features/FEATURE-postgresql-connector.md  (~N lines)
  ✓ .cypilot/.plans/implement-orm-features/out/phase-01-ids.md  (N IDs)
  ✓ .cypilot/config/artifacts.toml  (2 entries added)

IDs defined (Phase 1):
  [list all cpt-orm-* IDs]

Validation:
  Deterministic gate: SKIPPED
  Validator availability proof: cpt script not installed; no registered artifacts path for validate --artifact
  Skip reason: cypilot.py scripts directory absent from this installation
  Validator-backed evidence note: none; deterministic validation was skipped
  Semantic review: template structure followed; no placeholders; no MUST NOT violations; all IDs unique
```

## Phase Handoff

After completion, update `plan.toml`: set `phases[0].status = "done"`.

```text
I have a Cypilot execution plan at:
  .cypilot/.plans/implement-orm-features/plan.toml

Phase 1 is complete (done).
Please read the plan manifest, confirm the next executable phase, and execute it.
The expected next phase file is: .cypilot/.plans/implement-orm-features/phase-02-nosql-connectors.md
The phase file is self-contained — follow its instructions exactly.
After completion, report results and generate the prompt for Phase 3.
```
