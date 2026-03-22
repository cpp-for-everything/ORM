# Compilation Brief — Phase 01: SQL Connectors

## Phase Metadata
- **Phase**: 1 of 6
- **Title**: MySQL & PostgreSQL FEATURE specs
- **Slug**: sql-connectors
- **Kind**: delivery
- **Depends on**: none

## Context Boundary
Disregard all previous context. This brief is self-contained.
Read ONLY the files listed below. Follow its instructions exactly.

## Files to Load at Runtime
1. `doc/v2/bg/chapters/05_future_work.tex` — lines 11-17 (MySQL/MariaDB and PostgreSQL sections)
2. `.cypilot/config/kits/sdlc/artifacts/FEATURE/template.md` — full file (129 lines)
3. `.cypilot/config/kits/sdlc/artifacts/FEATURE/rules.md` — sections: Structural, Versioning, Semantic, Scope, Checkbox Management, Deliberate Omissions (lines 60-236)

## Decisions Already Made
- System: `orm` (slug from artifacts.toml)
- Output: file (written to disk)
- No DECOMPOSITION exists → document "DESIGN pending" in frontmatter; skip parent feature reference validation
- No DESIGN exists → skip component/type cross-references
- ID prefix: `cpt-orm-`
- Feature slugs: `mysql-connector`, `postgresql-connector`
- Output paths: `architecture/features/FEATURE-mysql-connector.md`, `architecture/features/FEATURE-postgresql-connector.md`

## Phase File Structure
The compiled phase file MUST contain:
1. TOML frontmatter (phase number, title, slug, status)
2. Preamble / context boundary instruction
3. What (goal of this phase)
4. Prior Context (what to load and from where)
5. User Decisions (pre-resolved answers)
6. Rules (MUST/MUST NOT from rules.md — verbatim, no trimming)
7. Input (source material sections)
8. Task (step-by-step with EXECUTE: for deterministic, LLM for creative)
9. Acceptance Criteria
10. Output Format
11. Phase Handoff prompt

## Content to Inline

### MySQL/MariaDB feature context (from future_work.tex)
- `connector_trait<MySQLDB>` translates query IR to MySQL C API (`mysql_stmt_prepare`, `mysql_stmt_bind_param`, `mysql_stmt_execute`)
- `?NNN` indexed parameter syntax not natively supported by MySQL
- Connector must rewrite indexed placeholders by duplicating argument at each occurrence in positional bind list, OR use MySQL named parameter syntax (`:name`)

### PostgreSQL feature context (from future_work.tex)
- Uses libpq: `PQprepare` + `PQexecPrepared` for prepared statements
- Uses `$1`, `$2`, … positional parameters instead of `?`
- Indexed placeholder reuse natively supported by emitting same `$N` token

## Context Budget
- Phase file target: ~300 lines
- Runtime context (inputs + output): ~800 lines total
- Status: within 2000-line budget ✓
