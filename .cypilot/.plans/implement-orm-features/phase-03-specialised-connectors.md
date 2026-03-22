---
phase: 3
title: "Cassandra & Neo4j FEATURE specs"
slug: specialised-connectors
status: pending
kind: delivery
depends_on: [2]
output_files:
  - architecture/features/FEATURE-cassandra-connector.md
  - architecture/features/FEATURE-neo4j-connector.md
outputs:
  - .cypilot/.plans/implement-orm-features/out/phase-03-ids.md
inputs:
  - .cypilot/.plans/implement-orm-features/out/phase-01-ids.md
  - .cypilot/.plans/implement-orm-features/out/phase-02-ids.md
---

--- CONTEXT BOUNDARY ---
Disregard all previous context. This phase file is self-contained.
Read ONLY the files listed in Prior Context. Follow these instructions exactly.
---

## What

Generate two FEATURE spec artifacts:
1. `architecture/features/FEATURE-cassandra-connector.md` — Apache Cassandra CQL connector
2. `architecture/features/FEATURE-neo4j-connector.md` — Neo4j graph database connector

## Prior Context

Read these files before proceeding:

1. `doc/v2/bg/chapters/05_future_work.tex` lines 27–33 — Cassandra and Neo4j source descriptions
2. `.cypilot/config/kits/sdlc/artifacts/FEATURE/template.md` — full template
3. `.cypilot/config/kits/sdlc/artifacts/FEATURE/rules.md` — sections: Structural, Versioning, Semantic, Scope, Checkbox Management, Deliberate Omissions
4. `.cypilot/.plans/implement-orm-features/out/phase-01-ids.md` — Phase 1 IDs
5. `.cypilot/.plans/implement-orm-features/out/phase-02-ids.md` — Phase 2 IDs

## User Decisions

- **System**: `orm`
- **ID prefix**: `cpt-orm-`
- **Feature slugs**: `cassandra-connector`, `neo4j-connector`
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
- No duplicate IDs within document or across prior phases (phases 1–2)

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

### Apache Cassandra (from future_work.tex lines 27–29)
- CQL resembles SQL but enforces strict partition key constraints in WHERE clauses
- `connector_trait<CassandraDB>` uses DataStax C++ Driver
- Compile-time enforcement via `static_assert`: every WHERE clause must include a partition key predicate
- Prevents full-table-scan queries (catastrophically expensive in distributed clusters)

### Neo4j (from future_work.tex lines 31–33)
- Uses Cypher query language for graph traversal
- Requires fundamentally different rendering pass from relational IR:
  - SELECT → `MATCH`
  - JOINs → relationship traversal
  - WHERE predicates remain structurally similar
- `connector_trait<Neo4jDB>` targets Neo4j Bolt protocol via `libneo4j-client`
- Extends query IR with graph-specific operations: `.traverse<Relationship>()`

### ORM Architecture context
- `connector_trait<DB>`: dumb tag type, trait holds all logic
- Capability tags as nested types: `supports_joins`, `supports_transactions`, `supports_aggregation`
- Missing capability + usage = `static_assert` compile error
- ORM core calls `connector_trait<DB>::execute(conn, query_ir, params...)`

## Task

### Step 1 — Verify prior IDs loaded
Read `out/phase-01-ids.md` and `out/phase-02-ids.md`. Keep the combined ID list in context to prevent duplicates.

### Step 2 — Generate FEATURE-cassandra-connector.md

**Section 1: Feature Context**
- Feature ID: `cpt-orm-feature-cassandra-connector`
- Overview: `connector_trait<CassandraDB>` specialisation using DataStax C++ Driver; enforces partition key constraints at compile time
- Purpose: Enable ORM to target Apache Cassandra while statically preventing full-table-scan queries

**Section 2: Actor Flows (CDSL)**
- Flow: Developer executes a filtered query against Cassandra
- ID: `cpt-orm-flow-cassandra-connector-execute-query`
- Steps: configure connector → build query with WHERE partition key predicate → compile-time static_assert validates partition key present → execute via DataStax driver → hydrate result

**Section 3: Processes / Business Logic (CDSL)**
- Algo 1: Compile-time partition key validation — ID: `cpt-orm-algo-cassandra-connector-validate-pk`
  - Input: WHERE predicate IR at compile time
  - Output: compile success or static_assert failure
  - Steps: inspect predicate tree for partition key column equality → static_assert if absent
- Algo 2: IR-to-CQL rendering — ID: `cpt-orm-algo-cassandra-connector-render-cql`
  - Input: query IR
  - Output: CQL string with `?` positional parameters (DataStax driver style)
- Algo 3: Result hydration — ID: `cpt-orm-algo-cassandra-connector-hydrate-result`
  - Map CassandraResult rows to C++ entity fields

**Section 4: States** — Not applicable (connector stateless; note explicitly)

**Section 5: Definitions of Done**
- DoD 1: `cpt-orm-dod-cassandra-connector-trait-specialisation`
- DoD 2: `cpt-orm-dod-cassandra-connector-pk-static-assert` — static_assert fires when partition key absent from WHERE
- DoD 3: `cpt-orm-dod-cassandra-connector-cql-render`
- DoD 4: `cpt-orm-dod-cassandra-connector-capability-tags`

**Section 6: Acceptance Criteria** — one testable criterion per DoD

### Step 3 — Generate FEATURE-neo4j-connector.md

**Section 1: Feature Context**
- Feature ID: `cpt-orm-feature-neo4j-connector`
- Overview: `connector_trait<Neo4jDB>` specialisation translating relational query IR to Cypher via Neo4j Bolt protocol (`libneo4j-client`)
- Purpose: Enable graph database storage with compile-time query structure; extends IR with `.traverse<Relationship>()` for graph traversal

**Section 2: Actor Flows (CDSL)**
- Flow: Developer executes a graph traversal query against Neo4j
- ID: `cpt-orm-flow-neo4j-connector-execute-traversal`
- Steps: configure connector → build query with `.traverse<Relationship>()` → render Cypher MATCH statement → send via Bolt protocol → iterate result stream → hydrate entities

**Section 3: Processes / Business Logic (CDSL)**
- Algo 1: IR-to-Cypher rendering — ID: `cpt-orm-algo-neo4j-connector-render-cypher`
  - SELECT → `MATCH`, JOINs → relationship pattern, WHERE → Cypher WHERE
  - `.traverse<R>()` → relationship traversal pattern in MATCH clause
- Algo 2: Bolt protocol dispatch — ID: `cpt-orm-algo-neo4j-connector-bolt-dispatch`
  - Encode Cypher string + params as Bolt request
  - Send via `libneo4j-client`; receive response stream
- Algo 3: Result hydration — ID: `cpt-orm-algo-neo4j-connector-hydrate-result`
  - Map Bolt record fields to C++ entity fields

**Section 4: States** — Not applicable

**Section 5: Definitions of Done**
- DoD 1: `cpt-orm-dod-neo4j-connector-trait-specialisation`
- DoD 2: `cpt-orm-dod-neo4j-connector-cypher-render` — SELECT/JOIN/WHERE correctly rendered as Cypher MATCH
- DoD 3: `cpt-orm-dod-neo4j-connector-traverse-op` — `.traverse<Relationship>()` extension compiles and renders correctly
- DoD 4: `cpt-orm-dod-neo4j-connector-bolt-dispatch`
- DoD 5: `cpt-orm-dod-neo4j-connector-capability-tags`

**Section 6: Acceptance Criteria** — one testable criterion per DoD

### Step 4 — Write intermediate ID registry
Write `.cypilot/.plans/implement-orm-features/out/phase-03-ids.md` with all new IDs from this phase.

### Step 5 — Update artifacts.toml
Add both FEATURE artifact paths to `[[systems]]` entry for `orm`.

### Step 6 — Confirm before writing
Present summary and ask `yes/no/modify`.

## Acceptance Criteria

- [ ] Both FEATURE files written to `architecture/features/`
- [ ] Template structure followed (sections 1–6)
- [ ] `cpt-orm-featstatus-{slug}` defined under H1 in each file
- [ ] No IDs duplicated from Phases 1–2 (verified against both out files)
- [ ] No duplicate IDs within this phase
- [ ] All IDs `cpt-orm-{kind}-{feature-slug}-{slug}` with priority markers
- [ ] No placeholder content; no MUST NOT violations
- [ ] All checkboxes `[ ]`
- [ ] `out/phase-03-ids.md` written
- [ ] `artifacts.toml` updated

## Output Format

```text
Phase 3/6: Specialised Connectors — DONE

Files written:
  ✓ architecture/features/FEATURE-cassandra-connector.md  (~N lines)
  ✓ architecture/features/FEATURE-neo4j-connector.md  (~N lines)
  ✓ .cypilot/.plans/implement-orm-features/out/phase-03-ids.md  (N IDs)
  ✓ .cypilot/config/artifacts.toml  (2 entries added)

IDs defined (Phase 3):
  [list all cpt-orm-* IDs]

Validation:
  Deterministic gate: SKIPPED
  Validator availability proof: cpt script not installed; no registered artifact path for validate --artifact
  Skip reason: cypilot.py scripts directory absent from this installation
  Validator-backed evidence note: none; deterministic validation was skipped
  Semantic review: template structure followed; no placeholders; no MUST NOT violations; all IDs unique across phases 1–3
```

## Phase Handoff

After completion, update `plan.toml`: set `phases[2].status = "done"`.

```text
I have a Cypilot execution plan at:
  .cypilot/.plans/implement-orm-features/plan.toml

Phase 3 is complete (done).
Please read the plan manifest, confirm the next executable phase, and execute it.
The expected next phase file is: .cypilot/.plans/implement-orm-features/phase-04-infrastructure.md
The phase file is self-contained — follow its instructions exactly.
After completion, report results and generate the prompt for Phase 5.
```
