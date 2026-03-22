```toml
[phase]
plan = "implement-code-orm-connectors"
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

## Preamble

This is a self-contained phase file. All rules, constraints, and kit content
are included below. Project files listed in the Task section must be read
at runtime. Follow the instructions exactly, run any EXECUTE commands as
written, and report results against the acceptance criteria at the end.

## What

Lifecycle phase: verify all 11 delivery phases are `done`, then move the plan
directory `.cypilot/.plans/implement-code-orm-connectors/` to
`.cypilot/.plans/.archive/implement-code-orm-connectors/`, update `plan.toml`
`active_plan_dir` and set `lifecycle_status = "done"` in the moved manifest.

## Prior Context

- Phases 1–11 are the delivery phases. This phase runs only when all are `done`.
- Archive strategy: move directory to `.cypilot/.plans/.archive/`.
- No delivery work in this phase — archive only.

## User Decisions

None — this phase is fully autonomous.

## Rules

- MUST: Verify all 11 delivery phases have `status = "done"` in `plan.toml` before archiving
- MUST: Move plan directory to `.cypilot/.plans/.archive/implement-code-orm-connectors/`
- MUST: Update `active_plan_dir` in the moved `plan.toml` to the archive path
- MUST: Set `lifecycle_status = "done"` in the moved `plan.toml`
- MUST: Set `execution_status = "done"` in the moved `plan.toml`
- MUST NOT: Delete any delivery output files (connector headers, tests) — only move plan files
- MUST NOT: Proceed if any delivery phase is not `done`

## Input

None — no external files required.

## Task

1. **Read `plan.toml`** at `.cypilot/.plans/implement-code-orm-connectors/plan.toml`. Verify all phases 1–11 have `status = "done"`. If any phase is not `done`, STOP and report which phases are incomplete.

2. **Create archive directory** — Ensure `.cypilot/.plans/.archive/` exists.

3. **Move plan directory** — Move `.cypilot/.plans/implement-code-orm-connectors/` to `.cypilot/.plans/.archive/implement-code-orm-connectors/`.

4. **Update moved `plan.toml`** at `.cypilot/.plans/.archive/implement-code-orm-connectors/plan.toml`:
   - Set `active_plan_dir = ".cypilot/.plans/.archive/implement-code-orm-connectors"`
   - Set `lifecycle_status = "done"`
   - Set `execution_status = "done"`

5. **Self-verify** — Confirm the archive directory exists and `plan.toml` is present with correct values.

## Acceptance Criteria

- [ ] All 11 delivery phases confirmed `done` before proceeding
- [ ] Plan directory moved to `.cypilot/.plans/.archive/implement-code-orm-connectors/`
- [ ] `plan.toml` in archive has `active_plan_dir` pointing to archive path
- [ ] `plan.toml` has `lifecycle_status = "done"` and `execution_status = "done"`
- [ ] No delivery output files deleted (connector headers and tests remain intact)

## Output Format

When complete, report results in this exact format:
```text
PHASE 12/12 COMPLETE
Status: PASS | FAIL
Files created: {list}
Files modified: {list}
Acceptance criteria:
  [x] Criterion 1 — PASS
  [ ] Criterion 2 — FAIL: {reason}
  ...
Notes: {any issues or decisions made}
```

```text
ALL PHASES COMPLETE (12/12)
Plan: .cypilot/.plans/.archive/implement-code-orm-connectors/plan.toml
Lifecycle: archive
```

Then ask: `Continue in this chat? [y/n]`
