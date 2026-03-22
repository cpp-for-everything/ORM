# Compilation Brief — Phase 05: Language & Tooling

## Phase Metadata
- **Phase**: 5 of 6
- **Title**: C++26 Reflection & Schema Migration FEATURE specs
- **Slug**: language-tooling
- **Kind**: delivery
- **Depends on**: Phase 4 (out/phase-04-ids.md must exist)

## Context Boundary
Disregard all previous context. This brief is self-contained.
Read ONLY the files listed below. Follow its instructions exactly.

## Files to Load at Runtime
1. `doc/v2/bg/chapters/05_future_work.tex` — lines 61-68 (C++26 reflection and schema migration sections)
2. `.cypilot/config/kits/sdlc/artifacts/FEATURE/template.md` — full file
3. `.cypilot/config/kits/sdlc/artifacts/FEATURE/rules.md` — sections: Structural, Versioning, Semantic, Scope, Checkbox Management, Deliberate Omissions
4. `.cypilot/.plans/implement-orm-features/out/phase-04-ids.md`

## Decisions Already Made
- System: `orm`
- Output: file
- No DECOMPOSITION / DESIGN → "DESIGN pending" in frontmatter
- ID prefix: `cpt-orm-`
- Feature slugs: `cpp26-reflection`, `schema-migration`
- Output paths: `architecture/features/FEATURE-cpp26-reflection.md`, `architecture/features/FEATURE-schema-migration.md`

## Content to Inline

### C++26 Reflection feature context (from future_work.tex)
- C++26 introduces `std::meta` reflection for compile-time inspection of struct member names
- Current fallback: Boost.PFR (already used on pre-C++26 compilers)
- When compiled with C++26-capable compiler: eliminate mandatory string arg `"col_name"` from `property<T, "col_name">`
- Column name inferred automatically from struct member name via `std::meta::name_of`
- Explicit string arg remains available as column name override
- Both paths co-exist; selection via `#ifdef __cpp_impl_reflection`

### Schema Migration feature context (from future_work.tex)
- `orm::migrate<DB>` tool compares C++ entity declarations with live database schema
- Generates required `ALTER TABLE` / `CREATE TABLE` statements
- Schema encoded in C++ types → migration tool can provide compile-time guarantees:
  - No column referenced by a query has been deleted or renamed without updating the entity struct

## Context Budget
- Phase file target: ~300 lines
- Runtime context: ~900 lines total
- Status: within 2000-line budget ✓
