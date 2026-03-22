---
phase: 6
title: "Archive plan files"
slug: archive
status: pending
kind: lifecycle
depends_on: [5]
output_files: []
outputs: []
inputs: []
---

--- CONTEXT BOUNDARY ---
Disregard all previous context. This phase file is self-contained.
Read ONLY the files listed in Prior Context. Follow these instructions exactly.
---

## What

Archive the completed plan directory by moving it from `.cypilot/.plans/implement-orm-features/` to `.cypilot/.plans/.archive/implement-orm-features/` and updating the manifest accordingly.

## Prior Context

Read this file before proceeding:

1. `.cypilot/.plans/implement-orm-features/plan.toml` — verify all delivery phases are `done` before archiving

## Task

### Step 1 — Verify all delivery phases are done

Read `plan.toml`. Confirm that phases 1–5 all have `status = "done"`. If any phase is not `done`, STOP and report which phases are incomplete — do not proceed with archive.

### Step 2 — Set lifecycle_status to ready

In `plan.toml`, update:
```toml
lifecycle_status = "ready"
```

### Step 3 — Create archive directory

EXECUTE: `New-Item -ItemType Directory -Force -Path .cypilot/.plans/.archive` (PowerShell)

### Step 4 — Move plan directory

EXECUTE: `Move-Item -Path .cypilot/.plans/implement-orm-features -Destination .cypilot/.plans/.archive/implement-orm-features` (PowerShell)

### Step 5 — Update manifest in new location

In `.cypilot/.plans/.archive/implement-orm-features/plan.toml`, update:
```toml
active_plan_dir = ".cypilot/.plans/.archive/implement-orm-features"
execution_status = "done"
lifecycle_status = "done"
```

## Acceptance Criteria

- [ ] All 5 delivery phases have `status = "done"` in plan.toml
- [ ] Plan directory exists at `.cypilot/.plans/.archive/implement-orm-features/`
- [ ] `plan.active_plan_dir = ".cypilot/.plans/.archive/implement-orm-features"` in moved manifest
- [ ] `plan.lifecycle_status = "done"` in moved manifest
- [ ] `plan.execution_status = "done"` in moved manifest
- [ ] Original `.cypilot/.plans/implement-orm-features/` directory no longer exists

## Output Format

```text
Phase 6/6: Archive — DONE

═══════════════════════════════════════════════
ALL PHASES COMPLETE (6/6)
───────────────────────────────────────────────
Plan: .cypilot/.plans/.archive/implement-orm-features/plan.toml
Target: FEATURE (10 artifacts)
Phases completed: 6
Execution status: done
Lifecycle strategy: archive
Lifecycle status: done
═══════════════════════════════════════════════

FEATURE artifacts generated:
  architecture/features/FEATURE-mysql-connector.md
  architecture/features/FEATURE-postgresql-connector.md
  architecture/features/FEATURE-mongodb-connector.md
  architecture/features/FEATURE-redis-connector.md
  architecture/features/FEATURE-cassandra-connector.md
  architecture/features/FEATURE-neo4j-connector.md
  architecture/features/FEATURE-thread-safety.md
  architecture/features/FEATURE-wire-protocol.md
  architecture/features/FEATURE-cpp26-reflection.md
  architecture/features/FEATURE-schema-migration.md
```
