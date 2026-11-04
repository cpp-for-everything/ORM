# Probes: the literal operand defect, before the fix

These two programs reproduce the defect that the negative compilation tests
`07` to `13` now prevent. They are kept as the record of what was measured.

**They are expected to fail to compile on any commit after the fix.** That is
the fix working: a literal operand reaching execution is now a compile error.
To rerun them, check out `d5795a9`, the last commit before the fix.

Measured on 4 Nov 2026 with g++ 14.2.0 on Ubuntu 24.04 (WSL), SQLite in memory,
library at `d5795a9`. Raw output is in `../evidence/probe_*_before_fix.txt`.

## What they show

`probe_literal.cpp`, a `SELECT` with a value written directly into `where()`:

| Path | Input | Rows | Correct |
|---|---|---|---|
| typed placeholder | `banana` | 1 | 1 |
| typed placeholder | `x' OR '1'='1` | 0 | 0 |
| literal in the IR | `banana` | **0** | 1 |
| literal in the IR | `x' OR '1'='1` | 0 | 0 |

`probe_update.cpp`, the same in `UPDATE ... SET` and `DELETE ... WHERE`:

| Case | Intent | Result |
|---|---|---|
| placeholder in SET | rename id 1 to `grape` | renamed |
| literal in SET | rename id 2 to `grape` | **unchanged** |
| literal in WHERE | delete id 3 | **nothing deleted** |

## Why

Every connector renders a literal operand as an unbound `?`
(`SQLite/sqlite_db.hpp:258`) and no connector has code to bind one. The driver
treats an unbound parameter as `NULL`, and `x = NULL` is never true.

In the `UPDATE`, runtime arguments are bound positionally from slot 1
(`SQLite/sqlite_db.hpp:214`). The literal's `?` comes first, so the one
argument meant for `WHERE id = ?` was bound into the `SET` slot, and the
`WHERE` slot was left `NULL`. That is parameter confusion: a caller's value
lands in the wrong position.

The literal never became SQL text, so it was **not injectable**. It failed
closed. It was a silent wrong result, which is its own kind of defect.
