# Compilation Brief: Phase 12/12 — Archive

--- CONTEXT BOUNDARY ---
Disregard all previous context. This brief is self-contained.
Read ONLY the files listed below. Follow the instructions exactly.
---

## Phase Metadata
```toml
[phase]
number = 12
total = 12
type = "implement"
title = "Archive plan files"
depends_on = [2, 3, 4, 5, 6, 7, 8, 9, 10, 11]
input_files = []
output_files = []
outputs = []
inputs = []
```

## Load Instructions

1. **plan.toml**: Read `.cypilot/.plans/implement-code-orm-connectors/plan.toml` (whole file)
   - Runtime read → verify all delivery phases are `done`

**Do NOT load**: any kit files, FEATURE specs, or source files.

## Compile Phase File
Write to: `.cypilot/.plans/implement-code-orm-connectors/phase-12-archive.md`

This is a lifecycle phase. Its sole responsibility is to:
1. Verify all 11 delivery phases are `done` in `plan.toml`.
2. Move the plan directory `.cypilot/.plans/implement-code-orm-connectors/` to `.cypilot/.plans/.archive/implement-code-orm-connectors/`.
3. Update `plan.active_plan_dir` in the moved `plan.toml` to the archive path.
4. Set `plan.lifecycle_status = "done"` in the moved `plan.toml`.
5. Report completion.

No review gates. No user decisions. Fully autonomous.

## Context Budget
- Phase file target: ≤ 100 lines
- Total execution context: ~150 lines — well within budget
