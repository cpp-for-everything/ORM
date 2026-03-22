---
phase: 2
title: "MongoDB & Redis FEATURE specs"
slug: nosql-connectors
status: pending
kind: delivery
depends_on: [1]
output_files:
  - architecture/features/FEATURE-mongodb-connector.md
  - architecture/features/FEATURE-redis-connector.md
outputs:
  - .cypilot/.plans/implement-orm-features/out/phase-02-ids.md
inputs:
  - .cypilot/.plans/implement-orm-features/out/phase-01-ids.md
---

--- CONTEXT BOUNDARY ---
Disregard all previous context. This phase file is self-contained.
Read ONLY the files listed in Prior Context. Follow these instructions exactly.
---

## What

Generate two FEATURE spec artifacts:
1. `architecture/features/FEATURE-mongodb-connector.md` — MongoDB document-store connector
2. `architecture/features/FEATURE-redis-connector.md` — Redis key-value/caching connector

## Prior Context

Read these files before proceeding:

1. `doc/v2/bg/chapters/05_future_work.tex` lines 19–26 — MongoDB and Redis source descriptions
2. `.cypilot/config/kits/sdlc/artifacts/FEATURE/template.md` — full template
3. `.cypilot/config/kits/sdlc/artifacts/FEATURE/rules.md` — sections: Structural, Versioning, Semantic, Scope, Checkbox Management, Deliberate Omissions
4. `.cypilot/.plans/implement-orm-features/out/phase-01-ids.md` — Phase 1 IDs (verify no duplicates)

## User Decisions

- **System**: `orm`
- **ID prefix**: `cpt-orm-`
- **Feature slugs**: `mongodb-connector`, `redis-connector`
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
- No duplicate IDs within document or across prior phases

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

### MongoDB (from future_work.tex lines 19–21)
- `connector_trait<MongoDB>` translates query IR to BSON filter documents via MongoDB C Driver (`libmongoc`)
- WHERE predicate tree maps to BSON operators: `$and`, `$or`, `$eq`, `$gt`
- Projected fields map to BSON projection documents
- Type system handles BSON-specific types: `bson_oid_t` (ObjectID), `int64_t` (64-bit integers)

### Redis (from future_work.tex lines 23–25)
- `orm::redis` connector maps single-column entities to Redis string keys; multi-column to Redis Hash fields
- IR translation is partial: WHERE predicates restricted to primary-key equality
- ORDER BY / GROUP BY require client-side materialisation
- Primary use case: caching layers with compile-time key-format consistency

### ORM Architecture context
- `connector_trait<DB>`: trait holds all logic; ORM core calls `connector_trait<DB>::execute(conn, query_ir, params...)`
- Capability tags: `supports_joins`, `supports_transactions`, `supports_aggregation`
- Missing capability + usage = `static_assert` compile error
- Query IR: SQL-superset constexpr fluent builder, all structure in template params
- NoSQL connectors translate same IR to wire format

## Task

### Step 1 — Verify Phase 1 IDs loaded
Read `out/phase-01-ids.md`. Keep the ID list in context to prevent duplicates.

### Step 2 — Generate FEATURE-mongodb-connector.md

**Section 1: Feature Context**
- Feature ID: `cpt-orm-feature-mongodb-connector`
- Overview: `connector_trait<MongoDB>` specialisation that translates ORM query IR to BSON filter documents via `libmongoc`
- Purpose: Enable document-oriented storage with compile-time query structure; WHERE predicates map to BSON operators
- Actors: Developer (no PRD actor IDs — DESIGN pending)
- References: DESIGN pending, no DECOMPOSITION

**Section 2: Actor Flows (CDSL)**
- Flow: Developer executes a select query against MongoDB
- ID: `cpt-orm-flow-mongodb-connector-execute-select`
- Steps: configure connector → build compile-time query → call execute → render BSON filter → call libmongoc → iterate cursor → hydrate result

**Section 3: Processes / Business Logic (CDSL)**
- Algo 1: IR-to-BSON filter rendering — ID: `cpt-orm-algo-mongodb-connector-render-bson`
  - Input: WHERE predicate IR tree
  - Output: BSON filter document
  - Steps: walk predicate tree → map AND/OR to `$and`/`$or` → map comparisons to `$eq`/`$gt`/`$lt`/etc.
- Algo 2: BSON projection rendering — ID: `cpt-orm-algo-mongodb-connector-render-projection`
  - Input: selected column list from IR
  - Output: BSON projection document (field inclusion/exclusion)
- Algo 3: Result hydration — ID: `cpt-orm-algo-mongodb-connector-hydrate-result`
  - Input: libmongoc cursor
  - Output: sequence of C++ entity instances
  - Handles `bson_oid_t` → entity ID field mapping

**Section 4: States** — Not applicable (connector stateless; note explicitly)

**Section 5: Definitions of Done**
- DoD 1: `cpt-orm-dod-mongodb-connector-trait-specialisation`
- DoD 2: `cpt-orm-dod-mongodb-connector-bson-filter-render`
- DoD 3: `cpt-orm-dod-mongodb-connector-projection-render`
- DoD 4: `cpt-orm-dod-mongodb-connector-oid-type-mapping`
- DoD 5: `cpt-orm-dod-mongodb-connector-capability-tags`

**Section 6: Acceptance Criteria** — one testable criterion per DoD

### Step 3 — Generate FEATURE-redis-connector.md

**Section 1: Feature Context**
- Feature ID: `cpt-orm-feature-redis-connector`
- Overview: `orm::redis` connector mapping entity structs to Redis string keys (single-column) or Hash fields (multi-column)
- Purpose: Provide a caching layer connector where compile-time query structure ensures consistent key formatting
- Actors: Developer
- References: DESIGN pending, no DECOMPOSITION

**Section 2: Actor Flows (CDSL)**
- Flow: Developer stores and retrieves a cached entity via Redis connector
- ID: `cpt-orm-flow-redis-connector-cache-entity`
- Steps: build query (primary-key equality WHERE only) → execute → map entity to key/hash → SET or HSET → on read: GET/HGETALL → hydrate entity

**Section 3: Processes / Business Logic (CDSL)**
- Algo 1: Entity-to-Redis key mapping — ID: `cpt-orm-algo-redis-connector-map-key`
  - Single-column entity → string key with compile-time prefix
  - Multi-column entity → Hash field mapping
- Algo 2: Partial IR translation — ID: `cpt-orm-algo-redis-connector-translate-ir`
  - Accept only primary-key equality predicates in WHERE
  - Static-assert failure if ORDER BY / GROUP BY requested without client-side materialisation flag
- Algo 3: Client-side materialisation — ID: `cpt-orm-algo-redis-connector-materialise`
  - SCAN + filter in client when ORDER BY / GROUP BY required and flag is set

**Section 4: States** — Not applicable

**Section 5: Definitions of Done**
- DoD 1: `cpt-orm-dod-redis-connector-trait-specialisation`
- DoD 2: `cpt-orm-dod-redis-connector-key-mapping`
- DoD 3: `cpt-orm-dod-redis-connector-pk-only-where`
- DoD 4: `cpt-orm-dod-redis-connector-hash-field-mapping`
- DoD 5: `cpt-orm-dod-redis-connector-capability-tags`

**Section 6: Acceptance Criteria** — one testable criterion per DoD

### Step 4 — Write intermediate ID registry
Write `.cypilot/.plans/implement-orm-features/out/phase-02-ids.md` listing all new IDs from this phase.

### Step 5 — Update artifacts.toml
Add both FEATURE artifact paths to `[[systems]]` entry for `orm`.

### Step 6 — Confirm before writing
Present summary of both artifacts and ask `yes/no/modify`.

## Acceptance Criteria

- [ ] Both FEATURE files written to `architecture/features/`
- [ ] Template structure followed (sections 1–6)
- [ ] `cpt-orm-featstatus-{slug}` defined under H1 in each file
- [ ] No IDs duplicated from Phase 1 (verified against `out/phase-01-ids.md`)
- [ ] No duplicate IDs within this phase
- [ ] All IDs `cpt-orm-{kind}-{feature-slug}-{slug}` with priority markers
- [ ] No placeholder content; no MUST NOT violations
- [ ] All checkboxes `[ ]`
- [ ] `out/phase-02-ids.md` written
- [ ] `artifacts.toml` updated

## Output Format

```text
Phase 2/6: NoSQL Connectors — DONE

Files written:
  ✓ architecture/features/FEATURE-mongodb-connector.md  (~N lines)
  ✓ architecture/features/FEATURE-redis-connector.md  (~N lines)
  ✓ .cypilot/.plans/implement-orm-features/out/phase-02-ids.md  (N IDs)
  ✓ .cypilot/config/artifacts.toml  (2 entries added)

IDs defined (Phase 2):
  [list all cpt-orm-* IDs]

Validation:
  Deterministic gate: SKIPPED
  Validator availability proof: cpt script not installed; no registered artifact path for validate --artifact
  Skip reason: cypilot.py scripts directory absent from this installation
  Validator-backed evidence note: none; deterministic validation was skipped
  Semantic review: template structure followed; no placeholders; no MUST NOT violations; all IDs unique across phases 1–2
```

## Phase Handoff

After completion, update `plan.toml`: set `phases[1].status = "done"`.

```text
I have a Cypilot execution plan at:
  .cypilot/.plans/implement-orm-features/plan.toml

Phase 2 is complete (done).
Please read the plan manifest, confirm the next executable phase, and execute it.
The expected next phase file is: .cypilot/.plans/implement-orm-features/phase-03-specialised-connectors.md
The phase file is self-contained — follow its instructions exactly.
After completion, report results and generate the prompt for Phase 4.
```
