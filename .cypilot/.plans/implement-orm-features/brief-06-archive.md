# Compilation Brief — Phase 06: Archive

## Phase Metadata
- **Phase**: 6 of 6
- **Title**: Archive plan files
- **Slug**: archive
- **Kind**: lifecycle
- **Depends on**: Phase 5 (all delivery phases done)

## Context Boundary
Disregard all previous context. This brief is self-contained.

## Files to Load at Runtime
1. `.cypilot/.plans/implement-orm-features/plan.toml` — verify all delivery phases are `done`

## Task
1. Verify `plan.execution_status` is effectively done (phases 1–5 all `done`)
2. Set `plan.lifecycle_status = "ready"` in `plan.toml`
3. Move the entire plan directory from `.cypilot/.plans/implement-orm-features/` to `.cypilot/.plans/.archive/implement-orm-features/`
4. Update `plan.active_plan_dir` in the moved `plan.toml` to `.cypilot/.plans/.archive/implement-orm-features`
5. Set `plan.lifecycle_status = "done"` in the moved `plan.toml`

## Acceptance Criteria
- All 5 delivery phases have `status = "done"` in plan.toml
- Plan directory exists at `.cypilot/.plans/.archive/implement-orm-features/`
- `plan.active_plan_dir` updated and `plan.lifecycle_status = "done"`

## Context Budget
- Phase file: ~50 lines
- Runtime: ~50 lines ✓
