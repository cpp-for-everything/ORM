# Compilation Brief: Phase 10/12 — C++26 Reflection

--- CONTEXT BOUNDARY ---
Disregard all previous context. This brief is self-contained.
Read ONLY the files listed below. Follow the instructions exactly.
---

## Phase Metadata
```toml
[phase]
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

## Load Instructions

1. **Codebase rules**: Read `.cypilot/config/kits/sdlc/codebase/rules.md` (lines 60–260, ~200 lines)
   - Inline → Rules section; skip Traceability (DOCS-ONLY), Checkbox Cascade

2. **FEATURE spec**: Read `architecture/features/FEATURE-cpp26-reflection.md` (whole file, ~253 lines)
   - Runtime read → Task step 1 (extract two flows, two algos, DoD items)

3. **property.hpp**: Read `lib/include/ORM/entity/property.hpp` (whole file)
   - Runtime read → Task step 1 (understand existing property<T, "name"> struct to extend)

4. **reflection.hpp**: Read `lib/include/ORM/details/reflection.hpp` (whole file)
   - Runtime read → Task step 1 (understand existing reflection infrastructure)

5. **lib/CMakeLists.txt**: Read `lib/CMakeLists.txt` (whole file, ~78 lines)
   - Runtime read → Task step 1 (understand ORM_HAS_REFLECTION=1/0 compile definition)

**Do NOT load**: other connector headers, other FEATURE specs, checklist.md.

## Compile Phase File
Write to: `.cypilot/.plans/implement-code-orm-connectors/phase-10-cpp26-reflection.md`

Key deliverables:
- Extend `property<T>` (or `property<T, StringLiteral>`) to support `property<T>` (no string arg) on C++26.
- `#ifdef ORM_HAS_REFLECTION` (already set by CMakeLists to 1 when `__cpp_impl_reflection` is available):
  - C++26 path: use `std::meta::name_of(^^MemberPointer)` to infer column name at compile time.
  - Pre-C++26 path: `static_assert(false, "property<T>: explicit column name required on pre-C++26 compilers")`.
- Explicit string `property<T, "name">` MUST override inference on ALL compilers.
- `#ifdef` guards must not introduce ODR violations.
- Unit tests:
  - `static_assert(property<int, "id">::column_name() == "id")` — explicit override works.
  - On C++26 path: `static_assert(property<int>::column_name() == "id")` for member `id` (compiler-specific test, guarded by `#ifdef ORM_HAS_REFLECTION`).
  - Dual-path coexistence: same query IR produced by both paths.

## Context Budget
- Phase file target: ≤ 600 lines
- Total execution context: phase (~600) + FEATURE (~253) + property.hpp (~TBD) + reflection.hpp (~TBD) + CMakeLists (~78) = ~1,100–1,400 lines — within budget
