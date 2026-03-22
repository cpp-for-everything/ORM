# Compilation Brief — Phase 04: Infrastructure

## Phase Metadata
- **Phase**: 4 of 6
- **Title**: Thread Safety & Wire-Protocol Optimisations FEATURE specs
- **Slug**: infrastructure
- **Kind**: delivery
- **Depends on**: Phase 3 (out/phase-03-ids.md must exist)

## Context Boundary
Disregard all previous context. This brief is self-contained.
Read ONLY the files listed below. Follow its instructions exactly.

## Files to Load at Runtime
1. `doc/v2/bg/chapters/05_future_work.tex` — lines 35-59 (Thread Safety and Wire Protocol sections)
2. `.cypilot/config/kits/sdlc/artifacts/FEATURE/template.md` — full file
3. `.cypilot/config/kits/sdlc/artifacts/FEATURE/rules.md` — sections: Structural, Versioning, Semantic, Scope, Checkbox Management, Deliberate Omissions
4. `.cypilot/.plans/implement-orm-features/out/phase-01-ids.md`
5. `.cypilot/.plans/implement-orm-features/out/phase-02-ids.md`
6. `.cypilot/.plans/implement-orm-features/out/phase-03-ids.md`

## Decisions Already Made
- System: `orm`
- Output: file
- No DECOMPOSITION / DESIGN → "DESIGN pending" in frontmatter
- ID prefix: `cpt-orm-`
- Feature slugs: `thread-safety`, `wire-protocol`
- Output paths: `architecture/features/FEATURE-thread-safety.md`, `architecture/features/FEATURE-wire-protocol.md`

## Content to Inline

### Thread Safety feature context (from future_work.tex)
- Current `db<DB>` handle holds a mutable reference to the connection object
- Concurrent `db::execute()` calls on same instance → race condition on underlying connection handle
- Planned measures:
  - `connection_pool<DB, N>`: N independent connection objects, provided to threads via RAII guards
  - Thread-local connections: `thread_local` connection per DB type eliminates synchronisation overhead
  - `db::transaction()` RAII guard: wraps sequence of `execute()` calls in atomic unit, auto-rollback on destruction if `commit()` not called
  - Compile-time thread-safety tagging: `supports_concurrent_execute` capability tag in `connector_trait<DB>`; `connection_pool` requires this tag via `static_assert`

### Wire Protocol feature context (from future_work.tex)
- Current SQLite connector calls standard `sqlite3_*` C API
- `io_uring` (Linux) / IOCP (Windows) integration for async network I/O: `co_await db.execute(q, args...)`
- Zero-copy result parsing: expose raw column memory via `std::span` views, defer copy until app takes ownership
- Batch INSERT optimisation: accumulate rows locally, emit single multi-row `INSERT INTO ... VALUES (...), (...), ...`
- Compile-time SQL string generation: for connectors where SQL is fully determined by IR (no runtime values), compute entire SQL as `constexpr char[]` array embedded as string literal in binary — eliminates `std::format` call on hot path

## Context Budget
- Phase file target: ~350 lines
- Runtime context: ~1000 lines total
- Status: within 2000-line budget ✓
