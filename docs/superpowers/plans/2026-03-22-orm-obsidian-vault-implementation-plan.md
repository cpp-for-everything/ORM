# ORM Obsidian Vault Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create the first-pass English-only Obsidian vault in `doc/obsidan` that supports onboarding, architecture exploration, and reference lookup for the ORM query-construction and execution flow.

**Architecture:** Build the vault as a layered knowledge base with a top-level home note, section-level Maps of Content, and focused notes for onboarding, query flow, architecture, and file-oriented reference. Ground every note in the approved spec and the authoritative source files so the vault explains the real current behavior of the ORM instead of restating older or idealized API descriptions.

**Tech Stack:** Markdown, Obsidian wikilinks, Obsidian callouts, existing repository source files, CMake, CTest, Ninja Multi-Config on Windows

---

## Implementation Notes

This is a documentation-only implementation plan. There is no note-specific automated test suite in the repository, so validation is done through:

- source-grounded authoring
- manual note and link review
- repository-wide configure/build/test verification after the documentation changes

Do not expand scope into `README.md`, `doc/v2/`, or unrelated source files unless a concrete source/documentation mismatch is discovered that would make the new vault incorrect. If such a mismatch is found, stop and surface it before broadening the task.

## File Structure

### Create in `doc/obsidan/`

- `doc/obsidan/00_Home.md` — top-level vault entry point and navigation hub

### Create in `doc/obsidan/01_Getting_Started/`

- `doc/obsidan/01_Getting_Started/00_Getting_Started.md` — local MOC for onboarding notes
- `doc/obsidan/01_Getting_Started/01_What_This_ORM_Is.md` — high-level explanation of the project and its compile-time approach
- `doc/obsidan/01_Getting_Started/02_Defining_Tables_and_Fields.md` — how structs, `property`, and table names work
- `doc/obsidan/01_Getting_Started/03_Relationships.md` — how `relationship` fields are declared and used in the example types
- `doc/obsidan/01_Getting_Started/04_Building_Queries.md` — how `insert`, `select`, `update`, and `deleteq` appear in user-facing code
- `doc/obsidan/01_Getting_Started/05_Executing_Queries.md` — how `db << query` works from a reader-facing perspective
- `doc/obsidan/01_Getting_Started/06_End_to_End_Example.md` — guided walkthrough of the example entities and example program

### Create in `doc/obsidan/02_Query_Flow/`

- `doc/obsidan/02_Query_Flow/00_Query_Flow.md` — local MOC for the flow notes
- `doc/obsidan/02_Query_Flow/01_From_Struct_to_Table.md` — flow from struct fields to reflected table metadata
- `doc/obsidan/02_Query_Flow/02_Member_Pointers_and_P_Syntax.md` — how `P<>` wraps member pointers for expressions
- `doc/obsidan/02_Query_Flow/03_Rules_and_Compile_Time_Expressions.md` — how rule trees are formed and transformed
- `doc/obsidan/02_Query_Flow/04_Select_Query_Construction.md` — how SELECT builders accumulate joins, filters, ordering, grouping, and limits
- `doc/obsidan/02_Query_Flow/05_Insert_Query_Construction.md` — how INSERT queries store signatures and placeholders
- `doc/obsidan/02_Query_Flow/06_Query_Execution_via_operator_shift.md` — execution boundary via `operator<<`
- `doc/obsidan/02_Query_Flow/07_MockDB_SQL_Generation.md` — how `MockDB` dispatches query types and emits SQL strings

### Create in `doc/obsidan/03_Architecture/`

- `doc/obsidan/03_Architecture/00_Architecture.md` — local MOC for subsystem notes
- `doc/obsidan/03_Architecture/01_Field_System.md` — fields and relationships as schema declarations
- `doc/obsidan/03_Architecture/02_Table_Reflection.md` — `Table<T>` and PFR-based tuple conversion
- `doc/obsidan/03_Architecture/03_Type_Wrapper_System.md` — DB type wrappers and primitive wrapper behavior
- `doc/obsidan/03_Architecture/04_Expression_System.md` — member pointers, statements, rules, and compile-time operator logic
- `doc/obsidan/03_Architecture/05_CRUD_Builder_Architecture.md` — select/insert/update/delete builder responsibilities and shared utilities
- `doc/obsidan/03_Architecture/06_Connector_Architecture.md` — connector boundary with emphasis on `MockDB`

### Create in `doc/obsidan/04_Reference/`

- `doc/obsidan/04_Reference/00_Reference.md` — local MOC for reference notes
- `doc/obsidan/04_Reference/fields.md` — reference for `lib/include/ORM/fields.hpp`
- `doc/obsidan/04_Reference/table.md` — reference for `lib/include/ORM/table.hpp`
- `doc/obsidan/04_Reference/type_wrappers.md` — reference for `lib/include/ORM/db/types/db_type_wrappers.hpp` and `primitive_type_wrapper.hpp`
- `doc/obsidan/04_Reference/member_pointer.md` — reference for `lib/include/ORM/details/member_pointer.hpp`
- `doc/obsidan/04_Reference/rules.md` — reference for `lib/include/ORM/rules.hpp` and `lib/src/ORM/rules.cpp`
- `doc/obsidan/04_Reference/select.md` — reference for `lib/include/ORM/CRUD/select.hpp`
- `doc/obsidan/04_Reference/insert.md` — reference for `lib/include/ORM/CRUD/insert.hpp`
- `doc/obsidan/04_Reference/crud_utils.md` — reference for `lib/include/ORM/CRUD/utils.hpp`
- `doc/obsidan/04_Reference/mockdb.md` — reference for `lib/include/ORM/db/connectors/MockDB/init.hpp`
- `doc/obsidan/04_Reference/join_rule.md` — reference for `lib/include/ORM/join_rule.hpp`
- `doc/obsidan/04_Reference/example_users.md` — reference for `example/users/users.hpp`
- `doc/obsidan/04_Reference/example_orm.md` — reference for `example/ORM.cpp`

### Create in `doc/obsidan/05_Glossary/`

- `doc/obsidan/05_Glossary/Glossary.md` — glossary of recurring project terms

## Required Reference Material While Implementing

Keep these files open while writing:

- `docs/superpowers/specs/2026-03-22-orm-obsidian-vault-design.md`
- `README.md`
- `example/users/users.hpp`
- `example/ORM.cpp`
- `lib/include/ORM/fields.hpp`
- `lib/include/ORM/table.hpp`
- `lib/include/ORM/db/types/db_type_wrappers.hpp`
- `lib/include/ORM/db/types/primitive_type_wrapper.hpp`
- `lib/include/ORM/details/member_pointer.hpp`
- `lib/include/ORM/rules.hpp`
- `lib/src/ORM/rules.cpp`
- `lib/include/ORM/CRUD/select.hpp`
- `lib/include/ORM/CRUD/insert.hpp`
- `lib/include/ORM/CRUD/utils.hpp`
- `lib/include/ORM/join_rule.hpp`
- `lib/include/ORM/db/connectors/MockDB/init.hpp`

## Chunk 1: Vault scaffold and onboarding notes

### Task 1: Create the top-level vault navigation

**Files:**
- Create: `doc/obsidan/00_Home.md`
- Create: `doc/obsidan/01_Getting_Started/00_Getting_Started.md`
- Create: `doc/obsidan/02_Query_Flow/00_Query_Flow.md`
- Create: `doc/obsidan/03_Architecture/00_Architecture.md`
- Create: `doc/obsidan/04_Reference/00_Reference.md`
- Create: `doc/obsidan/05_Glossary/Glossary.md`
- Reference: `docs/superpowers/specs/2026-03-22-orm-obsidian-vault-design.md`

- [ ] **Step 1: Create the directory tree and hub files**

Create the `doc/obsidan/` folder tree exactly as described in the File Structure section and create the six hub/glossary files listed above.

- [ ] **Step 2: Write `doc/obsidan/00_Home.md`**

Add:

- a short project overview
- three reader entry paths: onboarding, architecture/query flow, reference
- wikilinks to each section hub
- a note explaining that the vault is grounded in the current source tree

- [ ] **Step 3: Write the section hub notes and glossary skeleton**

Populate each section hub with:

- a one-paragraph description of the section
- wikilinks to every note planned inside that section
- a short “Start here” sequence for the most useful reading order

Populate the glossary with headings for the main recurring terms and placeholder definitions to be filled later.

- [ ] **Step 4: Verify that the hub-level links match the planned file layout**

Read the home note and each section hub in the IDE and confirm that:

- every linked note is part of the approved structure
- there are no dangling concept-note names such as `[[property]]` without an actual target note
- the section hubs consistently point to `doc/obsidan/...` notes that will exist

- [ ] **Step 5: Commit**

```bash
git add doc/obsidan/00_Home.md doc/obsidan/01_Getting_Started/00_Getting_Started.md doc/obsidan/02_Query_Flow/00_Query_Flow.md doc/obsidan/03_Architecture/00_Architecture.md doc/obsidan/04_Reference/00_Reference.md doc/obsidan/05_Glossary/Glossary.md
git commit -m "docs(obsidian): scaffold vault navigation"
```

### Task 2: Write the onboarding section

**Files:**
- Create: `doc/obsidan/01_Getting_Started/01_What_This_ORM_Is.md`
- Create: `doc/obsidan/01_Getting_Started/02_Defining_Tables_and_Fields.md`
- Create: `doc/obsidan/01_Getting_Started/03_Relationships.md`
- Create: `doc/obsidan/01_Getting_Started/04_Building_Queries.md`
- Create: `doc/obsidan/01_Getting_Started/05_Executing_Queries.md`
- Create: `doc/obsidan/01_Getting_Started/06_End_to_End_Example.md`
- Modify: `doc/obsidan/01_Getting_Started/00_Getting_Started.md`
- Reference: `README.md`
- Reference: `example/users/users.hpp`
- Reference: `example/ORM.cpp`

- [ ] **Step 1: Write the project-introduction and schema-definition notes**

Author:

- `01_What_This_ORM_Is.md`
- `02_Defining_Tables_and_Fields.md`

Use the onboarding template from the spec. Explain the role of `table_name`, `property`, and compile-time field naming using the `User`, `Post`, and `UserPost` example types.

- [ ] **Step 2: Write the relationships note**

Author `03_Relationships.md` using the relationships in `example/users/users.hpp` and explain how the example tables reference each other.

- [ ] **Step 3: Write the query-building and execution notes**

Author:

- `04_Building_Queries.md`
- `05_Executing_Queries.md`

Explain user-facing construction of `insert`, `select`, `update`, and `deleteq`, then explain the high-level meaning of `db << query` before deeper internals are introduced elsewhere.

- [ ] **Step 4: Write the end-to-end example note**

Author `06_End_to_End_Example.md` as a guided walkthrough of `example/users/users.hpp` plus `example/ORM.cpp`, linking outward to query flow and reference notes.

- [ ] **Step 5: Re-read the onboarding folder and normalize terminology**

Check that the onboarding notes consistently distinguish:

- schema declaration vs query construction vs query execution
- reader-facing syntax vs implementation internals
- current source behavior vs older README wording

- [ ] **Step 6: Commit**

```bash
git add doc/obsidan/01_Getting_Started
git commit -m "docs(obsidian): add getting started notes"
```

## Chunk 2: Query flow and architecture notes

### Task 3: Write the query-flow notes

**Files:**
- Create: `doc/obsidan/02_Query_Flow/01_From_Struct_to_Table.md`
- Create: `doc/obsidan/02_Query_Flow/02_Member_Pointers_and_P_Syntax.md`
- Create: `doc/obsidan/02_Query_Flow/03_Rules_and_Compile_Time_Expressions.md`
- Create: `doc/obsidan/02_Query_Flow/04_Select_Query_Construction.md`
- Create: `doc/obsidan/02_Query_Flow/05_Insert_Query_Construction.md`
- Create: `doc/obsidan/02_Query_Flow/06_Query_Execution_via_operator_shift.md`
- Create: `doc/obsidan/02_Query_Flow/07_MockDB_SQL_Generation.md`
- Modify: `doc/obsidan/02_Query_Flow/00_Query_Flow.md`
- Reference: `example/users/users.hpp`
- Reference: `example/ORM.cpp`
- Reference: `lib/include/ORM/fields.hpp`
- Reference: `lib/include/ORM/table.hpp`
- Reference: `lib/include/ORM/details/member_pointer.hpp`
- Reference: `lib/src/ORM/rules.cpp`
- Reference: `lib/include/ORM/CRUD/select.hpp`
- Reference: `lib/include/ORM/CRUD/insert.hpp`
- Reference: `lib/include/ORM/CRUD/utils.hpp`
- Reference: `lib/include/ORM/db/connectors/MockDB/init.hpp`

- [ ] **Step 1: Write the flow overview and struct-to-table note**

Author:

- `00_Query_Flow.md`
- `01_From_Struct_to_Table.md`

Make the flow overview explain the whole sequence from entity declaration to SQL output, then make `01_From_Struct_to_Table.md` explain `property`, compile-time names, and `Table<T>`/PFR reflection.

- [ ] **Step 2: Write the member-pointer and rule-expression notes**

Author:

- `02_Member_Pointers_and_P_Syntax.md`
- `03_Rules_and_Compile_Time_Expressions.md`

Use `P<>`, `mem_ptr`, `Rule`, and `operator!` examples from the source. Explain what is compile-time metadata versus runtime data.

- [ ] **Step 3: Write the query-construction notes**

Author:

- `04_Select_Query_Construction.md`
- `05_Insert_Query_Construction.md`

Explain how the builders store selected properties, placeholders, joins, filters, and signatures. Keep these notes grounded in the current templates in `select.hpp` and `insert.hpp`.

- [ ] **Step 4: Write the execution and SQL-generation notes**

Author:

- `06_Query_Execution_via_operator_shift.md`
- `07_MockDB_SQL_Generation.md`

Explain `operator<<`, `params_match`, connector dispatch, and how `MockDB` emits SQL for query objects.

- [ ] **Step 5: Verify the flow note sequence against the approved codemap and current source**

Re-read the query-flow folder and confirm that every note moves cleanly to the next stage in the pipeline and that any codemap-derived statement still matches the current source files.

- [ ] **Step 6: Commit**

```bash
git add doc/obsidan/02_Query_Flow
git commit -m "docs(obsidian): add query flow notes"
```

### Task 4: Write the subsystem architecture notes

**Files:**
- Create: `doc/obsidan/03_Architecture/01_Field_System.md`
- Create: `doc/obsidan/03_Architecture/02_Table_Reflection.md`
- Create: `doc/obsidan/03_Architecture/03_Type_Wrapper_System.md`
- Create: `doc/obsidan/03_Architecture/04_Expression_System.md`
- Create: `doc/obsidan/03_Architecture/05_CRUD_Builder_Architecture.md`
- Create: `doc/obsidan/03_Architecture/06_Connector_Architecture.md`
- Modify: `doc/obsidan/03_Architecture/00_Architecture.md`
- Reference: `lib/include/ORM/fields.hpp`
- Reference: `lib/include/ORM/table.hpp`
- Reference: `lib/include/ORM/db/types/db_type_wrappers.hpp`
- Reference: `lib/include/ORM/db/types/primitive_type_wrapper.hpp`
- Reference: `lib/include/ORM/details/member_pointer.hpp`
- Reference: `lib/include/ORM/rules.hpp`
- Reference: `lib/src/ORM/rules.cpp`
- Reference: `lib/include/ORM/CRUD/select.hpp`
- Reference: `lib/include/ORM/CRUD/insert.hpp`
- Reference: `lib/include/ORM/CRUD/utils.hpp`
- Reference: `lib/include/ORM/db/connectors/MockDB/init.hpp`

- [ ] **Step 1: Write the architecture overview note**

Populate `00_Architecture.md` with a subsystem map and a recommended contributor reading order.

- [ ] **Step 2: Write the field, reflection, and type-wrapper notes**

Author:

- `01_Field_System.md`
- `02_Table_Reflection.md`
- `03_Type_Wrapper_System.md`

Explain responsibilities, main types, compile-time data, and dependencies.

- [ ] **Step 3: Write the expression-system note**

Author `04_Expression_System.md`, covering:

- `mem_ptr`
- `Statement`
- `Rule`
- operator generation
- compile-time boolean rewrites

- [ ] **Step 4: Write the CRUD-builder and connector notes**

Author:

- `05_CRUD_Builder_Architecture.md`
- `06_Connector_Architecture.md`

Focus on file responsibilities, query-family boundaries, shared utilities, and `MockDB` as the execution/serialization endpoint.

- [ ] **Step 5: Cross-link the architecture notes to the flow and reference notes**

Ensure each architecture note links outward to the most relevant query-flow and reference pages rather than duplicating large explanations inline.

- [ ] **Step 6: Commit**

```bash
git add doc/obsidan/03_Architecture
git commit -m "docs(obsidian): add architecture notes"
```

## Chunk 3: Reference notes, glossary completion, and verification

### Task 5: Write the file-oriented reference notes and complete the glossary

**Files:**
- Create: `doc/obsidan/04_Reference/fields.md`
- Create: `doc/obsidan/04_Reference/table.md`
- Create: `doc/obsidan/04_Reference/type_wrappers.md`
- Create: `doc/obsidan/04_Reference/member_pointer.md`
- Create: `doc/obsidan/04_Reference/rules.md`
- Create: `doc/obsidan/04_Reference/select.md`
- Create: `doc/obsidan/04_Reference/insert.md`
- Create: `doc/obsidan/04_Reference/crud_utils.md`
- Create: `doc/obsidan/04_Reference/mockdb.md`
- Create: `doc/obsidan/04_Reference/join_rule.md`
- Create: `doc/obsidan/04_Reference/example_users.md`
- Create: `doc/obsidan/04_Reference/example_orm.md`
- Modify: `doc/obsidan/04_Reference/00_Reference.md`
- Modify: `doc/obsidan/05_Glossary/Glossary.md`
- Reference: all files listed in the "Required Reference Material While Implementing" section

- [ ] **Step 1: Write the schema and low-level helper reference notes**

Author:

- `fields.md`
- `table.md`
- `type_wrappers.md`
- `member_pointer.md`

Each note must include the reference template headings from the approved spec.

- [ ] **Step 2: Write the rules and CRUD reference notes**

Author:

- `rules.md`
- `select.md`
- `insert.md`
- `crud_utils.md`

Summarize the file role, key symbols, important behaviors, and inbound/outbound relationships for each file.

- [ ] **Step 3: Write the connector and example reference notes**

Author:

- `mockdb.md`
- `join_rule.md`
- `example_users.md`
- `example_orm.md`

Use the example notes to connect the public-facing examples to the lower-level implementation notes.

- [ ] **Step 4: Finish the reference hub and glossary**

Update `00_Reference.md` so it links to every reference page and finish `Glossary.md` with stable definitions for the main recurring terms used across the vault.

- [ ] **Step 5: Re-read the reference folder for template completeness and scanability**

Confirm that every reference note contains:

- `## File Role`
- `## Key Types / Symbols`
- `## Important Behaviors`
- `## Called By / Used By`
- `## Source Files`
- `## Related Notes`

- [ ] **Step 6: Commit**

```bash
git add doc/obsidan/04_Reference doc/obsidan/05_Glossary/Glossary.md
git commit -m "docs(obsidian): add reference notes and glossary"
```

### Task 6: Validate the vault and run full repository verification

**Files:**
- Modify if needed: `doc/obsidan/**/*.md`
- Reference: `CMakePresets.json`
- Reference: `CMakeLists.txt`
- Reference: `tests/CMakeLists.txt`
- Reference: `tests/ORM/CMakeLists.txt`

- [ ] **Step 1: Perform a full note review pass**

Open and read:

- `doc/obsidan/00_Home.md`
- every section hub
- at least one representative note from each section

Fix any:

- broken wikilink targets
- inconsistent naming
- statements that overclaim compile-time behavior beyond the source
- duplicate explanations that belong in cross-links instead

- [ ] **Step 2: Review the documentation diff before building**

Run:

```bash
git diff -- doc/obsidan
```

Expected: only the new `doc/obsidan` notes and any intentional follow-up wording fixes appear.

- [ ] **Step 3: Configure and build all targets for `x64-debug`**

Run:

```bash
cmake --preset x64-debug
cmake --build out/build/x64-debug --config Debug
```

Expected: configure succeeds and the full project builds with no `--target` restriction.

- [ ] **Step 4: Run the full test suite for `x64-debug`**

Run:

```bash
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure
```

Expected: all tests pass, including `Webframe-ORM-Tests`.

- [ ] **Step 5: Configure and build all targets for `x64-release`**

Run:

```bash
cmake --preset x64-release
cmake --build out/build/x64-release --config Release
```

Expected: configure succeeds and the full project builds with no warnings or errors.

- [ ] **Step 6: Run the full test suite for `x64-release`**

Run:

```bash
ctest --test-dir out/build/x64-release -C Release --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 7: Configure and build all targets for `x86-debug`**

Run:

```bash
cmake --preset x86-debug
cmake --build out/build/x86-debug --config Debug
```

Expected: configure succeeds and the full project builds. If the local machine does not have the required x86 MSVC toolchain installed, stop and report that environmental blocker instead of silently skipping this preset.

- [ ] **Step 8: Run the full test suite for `x86-debug`**

Run:

```bash
ctest --test-dir out/build/x86-debug -C Debug --output-on-failure
```

Expected: all tests pass if configuration/build succeeded.

- [ ] **Step 9: Configure and build all targets for `x86-release`**

Run:

```bash
cmake --preset x86-release
cmake --build out/build/x86-release --config Release
```

Expected: configure succeeds and the full project builds. If the local machine does not have the required x86 MSVC toolchain installed, stop and report that environmental blocker instead of silently skipping this preset.

- [ ] **Step 10: Run the full test suite for `x86-release`**

Run:

```bash
ctest --test-dir out/build/x86-release -C Release --output-on-failure
```

Expected: all tests pass if configuration/build succeeded.

- [ ] **Step 11: Commit the finalized vault**

```bash
git add doc/obsidan
git commit -m "docs(obsidian): add ORM Obsidian vault"
```

## Implementation Reminders

- Prefer explicit note links such as `[[04_Reference/fields|property]]` over dangling concept links that do not resolve to real notes.
- Keep onboarding notes readable first and detailed second.
- Keep reference notes concise and architecture/query-flow notes explanatory.
- Use Obsidian callouts where they improve clarity, especially for caveats and navigation help.
- Reuse the exact terminology from the approved spec unless the source files force a correction.
- Do not weaken or skip the full repository verification steps at the end.
