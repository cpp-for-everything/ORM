```toml
[phase]
plan = "implement-code-orm-connectors"
number = 10
total = 12
type = "implement"
title = "C++26 Reflection — property<T> inference + PFR fallback + dual-path + tests"
depends_on = [1]
input_files = [
  "architecture/features/FEATURE-cpp26-reflection.md",
  "lib/include/ORM/entity/property.hpp",
  "lib/include/ORM/details/reflection.hpp",
  "lib/CMakeLists.txt",
]
output_files = [
  "lib/include/ORM/entity/property.hpp",
  "tests/unit/test_cpp26_reflection.cpp",
]
outputs = ["out/phase-10-reflection-summary.md"]
inputs = ["out/phase-01-scaffolding-summary.md"]
```

## Preamble

This is a self-contained phase file. All rules, constraints, and kit content
are included below. Project files listed in the Task section must be read
at runtime. Follow the instructions exactly, run any EXECUTE commands as
written, and report results against the acceptance criteria at the end.

## What

Extend `lib/include/ORM/entity/property.hpp` to support string-free `property<T>` declarations
on C++26-capable compilers using `std::meta::name_of(^^member)` for compile-time column name
inference. On pre-C++26 compilers (or when `ORM_HAS_REFLECTION == 0`), `property<T>` without
a string argument must emit a `static_assert` compile error. The explicit string override
`property<T, "name">` must take precedence on ALL compilers. No ODR violations from the
dual-path `#ifdef ORM_HAS_REFLECTION` guard. Write unit tests in
`tests/unit/test_cpp26_reflection.cpp`.

## Prior Context

- `lib/CMakeLists.txt` already defines `ORM_HAS_REFLECTION=1` when `__cpp_impl_reflection` is detected; otherwise `ORM_HAS_REFLECTION=0`.
- Existing `property<T, StringLit>` uses the explicit string literal as column name.
- `lib/include/ORM/details/reflection.hpp` contains the existing reflection infrastructure.
- Goal: `property<int> id;` → column name `"id"` on C++26; `static_assert` error on C++23.
- `property<int, "user_id"> id;` → column name `"user_id"` on BOTH compilers.
- Traceability: DOCS-ONLY.

## User Decisions

### Already Decided (pre-resolved during planning)
- **Traceability mode**: DOCS-ONLY
- **Guard macro**: `ORM_HAS_REFLECTION` (already set by CMakeLists to 1/0)
- **C++26 API**: `std::meta::name_of(^^MemberPointer)` — returns `std::string_view` at compile time
- **Pre-C++26 assert message**: must contain "explicit column name required"

## Rules

### Structural
- MUST: When `ORM_HAS_REFLECTION == 1` AND no explicit string argument: use `std::meta::name_of(^^member)` to infer column name as `constexpr std::string_view`
- MUST: When `ORM_HAS_REFLECTION == 0` AND no explicit string argument: emit `static_assert(false, "property<T>: explicit column name required on pre-C++26 compilers")` — message MUST contain "explicit column name required"
- MUST: When explicit string argument present: use it as column name; MUST NOT invoke reflection inference even on C++26
- MUST: `#ifdef ORM_HAS_REFLECTION` guards MUST NOT introduce ODR violations; same `property<T, "name">` type on both compilers
- MUST: Existing `property<T, StringLit>` tests in `tests/unit/test_property.cpp` MUST still pass after this change
- MUST: Code implements all DoD items from FEATURE-cpp26-reflection.md §5
- MUST NOT: leave TODO/TBD/FIXME

### Engineering
- MUST: TDD — dual-path tests: explicit override works on both paths; C++26 inference produces correct name
- MUST: KISS — add `if constexpr (ORM_HAS_REFLECTION)` branch inside the existing `property` column name resolution; minimal diff
- MUST: No template specialisation duplication — extend existing `property` template, do not create a parallel type
- MUST NOT: use raw new/delete

### Quality
- MUST: Tests cover: (a) explicit string override works on current compiler; (b) on C++26 path: `property<int>::column_name()` matches member name (guarded by `#if ORM_HAS_REFLECTION`); (c) existing property tests still pass

## Input

### DoD items from FEATURE-cpp26-reflection.md §5:

**DoD: Inferred Column Name on C++26** — infer column name from struct member name when `property<T>` used without string arg on `ORM_HAS_REFLECTION=1` compiler; same as `property<T, "member_name">`.
Implements: `cpt-orm-flow-cpp26-reflection-define-entity`, `cpt-orm-algo-cpp26-reflection-resolve-name`

**DoD: Explicit String Override** — `property<T, "name">` MUST use the explicit string; overrides reflection on ALL compilers.
Implements: `cpt-orm-flow-cpp26-reflection-override-name`, `cpt-orm-algo-cpp26-reflection-resolve-name`

**DoD: PFR Fallback Enforcement** — pre-C++26 path: explicit string works; no string arg → `static_assert` with "explicit column name required".
Implements: `cpt-orm-algo-cpp26-reflection-pfr-fallback`

**DoD: Dual-Path Coexistence** — `#ifdef ORM_HAS_REFLECTION` guards; no ODR violations; both paths compile cleanly.
Implements: `cpt-orm-feature-cpp26-reflection`

### TDD acceptance criteria from FEATURE §6:
- `property<int> id;` on C++26: `property<int>::column_name() == "id"` via `static_assert`.
- `property<int, "user_id"> id;`: column name `"user_id"` on both C++26 and C++23.
- `property<int> id;` on C++23: `static_assert` error containing "explicit column name required".
- Identical query IR from `property<int> id;` on C++26 vs `property<int, "id"> id;` on same compiler.
- Codebase compiles under both GCC 16 (C++26) and GCC 13/Clang 18 (C++23).

## Task

1. **Read input files** — Read `architecture/features/FEATURE-cpp26-reflection.md` (full), `lib/include/ORM/entity/property.hpp` (full), `lib/include/ORM/details/reflection.hpp` (full), `lib/CMakeLists.txt` (full, to confirm `ORM_HAS_REFLECTION` macro).

2. **Extend `property.hpp`** — Inside the `property<T, StringLit>` template, add a specialisation or `if constexpr` branch for the no-string-arg case:
   ```cpp
   // Specialisation for property<T> without explicit string (C++26 path)
   #if ORM_HAS_REFLECTION
   template<typename T>
   struct property<T, string_literal<0>{}>  // or a sentinel default
   {
       static constexpr std::string_view column_name() noexcept {
           // Caller must pass ^^ member pointer; resolved via std::meta::name_of
           // Implementation depends on how the member pointer is threaded through
           return inferred_name_;
       }
       ...
   };
   #else
   template<typename T>
   struct property_no_string_arg {
       static_assert(false,
           "property<T>: explicit column name required on pre-C++26 compilers. "
           "Use property<T, \"col_name\">.");
   };
   #endif
   ```
   Adapt the exact implementation to match the existing `property.hpp` structure read in step 1. The key requirement is: explicit string → uses string; C++26 no-string → infers via `std::meta`; pre-C++26 no-string → `static_assert`.

3. **Write unit tests** — Create `tests/unit/test_cpp26_reflection.cpp`:
   - `TEST(Cpp26Reflection, ExplicitStringOverrideAlwaysWorks)` — `static_assert(std::string_view(property<int, "user_id">::column_name()) == "user_id")`
   - `#if ORM_HAS_REFLECTION` guarded block:
     - `TEST(Cpp26Reflection, InferredColumnNameMatchesMember)` — `static_assert(property<int>::column_name() == "id")` for a test entity member named `id`
   - `#endif`

4. **Update `tests/unit/CMakeLists.txt`** — Add `test_cpp26_reflection.cpp`.

5. **Verify existing tests still compile** — Confirm `test_property.cpp` test cases remain valid by reviewing the file (do not modify it unless necessary to fix a regression introduced by the property.hpp change).

6. **Write `out/phase-10-reflection-summary.md`**.

7. **Self-verify**.

## Acceptance Criteria

- [ ] `property.hpp` extended with dual-path column name resolution (`ORM_HAS_REFLECTION` guard)
- [ ] Explicit string `property<T, "name">` returns `"name"` on current compiler (test present)
- [ ] C++26 inference test present (guarded by `#if ORM_HAS_REFLECTION`)
- [ ] Pre-C++26 no-string-arg `static_assert` message contains "explicit column name required"
- [ ] `test_property.cpp` existing tests NOT broken (reviewed and confirmed)
- [ ] `test_cpp26_reflection.cpp` with ≥ 2 TEST() cases; CMakeLists updated
- [ ] No TODO/TBD/FIXME; no unresolved `{variable}` outside code fences
- [ ] `out/phase-10-reflection-summary.md` exists

## Output Format

```text
PHASE 10/12 COMPLETE
Status: PASS | FAIL
Files created/modified: {list}
Acceptance criteria: {checklist}
Notes: {any issues}
```

```text
I have a Cypilot execution plan at:
  .cypilot/.plans/implement-code-orm-connectors/plan.toml

Phase 10 is complete (PASS).
Please read the plan manifest, then execute Phase 11: "Schema Migration — orm::migrate<DB> diff + DDL generation + dry-run + tests".
The phase file is: .cypilot/.plans/implement-code-orm-connectors/phase-11-schema-migration.md
It is self-contained — follow its instructions exactly.
After completion, report results and generate the prompt for the next phase.
```
