# Compilation Brief — Phase 02: NoSQL Connectors

## Phase Metadata
- **Phase**: 2 of 6
- **Title**: MongoDB & Redis FEATURE specs
- **Slug**: nosql-connectors
- **Kind**: delivery
- **Depends on**: Phase 1 (out/phase-01-ids.md must exist)

## Context Boundary
Disregard all previous context. This brief is self-contained.
Read ONLY the files listed below. Follow its instructions exactly.

## Files to Load at Runtime
1. `doc/v2/bg/chapters/05_future_work.tex` — lines 19-26 (MongoDB and Redis sections)
2. `.cypilot/config/kits/sdlc/artifacts/FEATURE/template.md` — full file
3. `.cypilot/config/kits/sdlc/artifacts/FEATURE/rules.md` — sections: Structural, Versioning, Semantic, Scope, Checkbox Management, Deliberate Omissions
4. `.cypilot/.plans/implement-orm-features/out/phase-01-ids.md` — prior phase IDs (verify no duplicates)

## Decisions Already Made
- System: `orm`
- Output: file
- No DECOMPOSITION / DESIGN → "DESIGN pending" in frontmatter
- ID prefix: `cpt-orm-`
- Feature slugs: `mongodb-connector`, `redis-connector`
- Output paths: `architecture/features/FEATURE-mongodb-connector.md`, `architecture/features/FEATURE-redis-connector.md`

## Content to Inline

### MongoDB feature context (from future_work.tex)
- `connector_trait<MongoDB>` translates query IR to BSON filter documents via MongoDB C Driver (`libmongoc`)
- WHERE predicate tree maps to BSON operators: `$and`, `$or`, `$eq`, `$gt`
- Projected fields map to BSON projection documents
- Type system must handle BSON-specific types: `bson_oid_t` (ObjectID), `int64_t` (64-bit integers)

### Redis feature context (from future_work.tex)
- `orm::redis` connector maps single-column entities to Redis string keys, multi-column to Redis Hash fields
- IR translation is partial: WHERE predicates restricted to primary-key equality; ORDER BY / GROUP BY require client-side materialisation
- Primary use case: caching layers with compile-time key-format consistency

## Context Budget
- Phase file target: ~300 lines
- Runtime context: ~900 lines total
- Status: within 2000-line budget ✓
