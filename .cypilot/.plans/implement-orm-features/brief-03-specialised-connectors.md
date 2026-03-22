# Compilation Brief — Phase 03: Specialised Connectors

## Phase Metadata
- **Phase**: 3 of 6
- **Title**: Cassandra & Neo4j FEATURE specs
- **Slug**: specialised-connectors
- **Kind**: delivery
- **Depends on**: Phase 2 (out/phase-02-ids.md must exist)

## Context Boundary
Disregard all previous context. This brief is self-contained.
Read ONLY the files listed below. Follow its instructions exactly.

## Files to Load at Runtime
1. `doc/v2/bg/chapters/05_future_work.tex` — lines 27-33 (Cassandra and Neo4j sections)
2. `.cypilot/config/kits/sdlc/artifacts/FEATURE/template.md` — full file
3. `.cypilot/config/kits/sdlc/artifacts/FEATURE/rules.md` — sections: Structural, Versioning, Semantic, Scope, Checkbox Management, Deliberate Omissions
4. `.cypilot/.plans/implement-orm-features/out/phase-01-ids.md`
5. `.cypilot/.plans/implement-orm-features/out/phase-02-ids.md`

## Decisions Already Made
- System: `orm`
- Output: file
- No DECOMPOSITION / DESIGN → "DESIGN pending" in frontmatter
- ID prefix: `cpt-orm-`
- Feature slugs: `cassandra-connector`, `neo4j-connector`
- Output paths: `architecture/features/FEATURE-cassandra-connector.md`, `architecture/features/FEATURE-neo4j-connector.md`

## Content to Inline

### Cassandra feature context (from future_work.tex)
- CQL resembles SQL but enforces strict partition key constraints in WHERE clauses
- `connector_trait<CassandraDB>` uses DataStax C++ Driver
- Compile-time enforcement (via `static_assert`) that every WHERE clause includes a partition key predicate
- Prevents full-table-scan queries that would be catastrophically expensive in a distributed cluster

### Neo4j feature context (from future_work.tex)
- Uses Cypher query language for graph traversal
- SELECT → `MATCH`, JOINs → relationship traversal, WHERE predicates remain structurally similar
- `connector_trait<Neo4jDB>` targets Neo4j Bolt protocol via `libneo4j-client`
- Extends query IR with graph-specific operations: `.traverse<Relationship>()`

## Context Budget
- Phase file target: ~300 lines
- Runtime context: ~950 lines total
- Status: within 2000-line budget ✓
