# ORM Obsidian Vault Design

## Overview

This document specifies the first-pass Obsidian vault to be created for the C++ compile-time ORM project in `doc/obsidan`.

The vault is intended to serve three complementary purposes:

- onboarding new readers to the library
- explaining the internal architecture and compile-time query flow
- providing a reference-oriented knowledge base tied to the authoritative source files

The vault will be English-only, per the approved design direction for this task.

## Background

The repository currently contains:

- a root `README.md` with public-facing usage information
- `doc/v2/` LaTeX documentation
- an empty `doc/obsidan/` directory
- source files implementing a compile-time ORM with query construction, expression building, and SQL generation through `MockDB`

The design of the Obsidian vault will be grounded in the actual source layout and in the codemap titled `C++ Compile-Time ORM: Query Construction & Execution Flow`.

## Goals

- create a navigable Obsidian vault in `doc/obsidan`
- support onboarding, architecture discovery, and direct technical lookup
- document how table definitions, member-pointer expressions, CRUD builders, and query execution fit together
- make the query-construction and execution flow understandable from user-facing code down to SQL serialization
- tie explanations to the real source files rather than inferred or idealized APIs
- use Obsidian-friendly navigation patterns such as wikilinks, section hubs, and related-note sections

## Non-Goals

- document every source file in the repository in the first pass
- replace or duplicate `README.md` line-by-line
- extend documentation into unrelated framework subsystems outside the ORM focus of this request
- describe future or planned APIs as if they are already implemented

## Primary Source Basis

The vault content will primarily be based on the following files:

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

Where the codemap and source differ, the source files take priority.

## Reader Personas

### New user

A reader who wants to understand:

- what this ORM is
- how to define tables and relationships
- how to build and execute queries
- how placeholders and compile-time checking work in practice

### Contributor

A reader who wants to understand:

- which files own which responsibilities
- how expressions are encoded at compile time
- how SELECT and INSERT builders accumulate state
- how query objects are executed and serialized

### Maintainer

A reader who wants to quickly look up:

- where specific abstractions live
- what a file is responsible for
- which components feed into SQL generation

## Information Architecture

The Obsidian vault will use a layered structure with Maps of Content rather than a purely linear handbook or a pure source-tree mirror.

```text
doc/obsidan/
  00_Home.md

  01_Getting_Started/
    00_Getting_Started.md
    01_What_This_ORM_Is.md
    02_Defining_Tables_and_Fields.md
    03_Relationships.md
    04_Building_Queries.md
    05_Executing_Queries.md
    06_End_to_End_Example.md

  02_Query_Flow/
    00_Query_Flow.md
    01_From_Struct_to_Table.md
    02_Member_Pointers_and_P_Syntax.md
    03_Rules_and_Compile_Time_Expressions.md
    04_Select_Query_Construction.md
    05_Insert_Query_Construction.md
    06_Query_Execution_via_operator_shift.md
    07_MockDB_SQL_Generation.md

  03_Architecture/
    00_Architecture.md
    01_Field_System.md
    02_Table_Reflection.md
    03_Type_Wrapper_System.md
    04_Expression_System.md
    05_CRUD_Builder_Architecture.md
    06_Connector_Architecture.md

  04_Reference/
    00_Reference.md
    fields.md
    table.md
    type_wrappers.md
    member_pointer.md
    rules.md
    select.md
    insert.md
    crud_utils.md
    mockdb.md
    join_rule.md
    example_users.md
    example_orm.md

  05_Glossary/
    Glossary.md
```

## Navigation Model

### Home note

`00_Home.md` will act as the top-level entry point for the vault and will link to:

- getting started notes
- query flow notes
- architecture notes
- reference notes
- glossary

### Section hubs

Each major folder will contain an index note that behaves as a local Map of Content:

- `01_Getting_Started/00_Getting_Started.md`
- `02_Query_Flow/00_Query_Flow.md`
- `03_Architecture/00_Architecture.md`
- `04_Reference/00_Reference.md`

### Cross-linking

The vault will use wikilinks for core concepts and cross-references between note families, so a reader can move from concept to implementation and back.

Representative links include:

- `[[04_Reference/fields|property]]`
- `[[04_Reference/table|Table<T>]]`
- `[[04_Reference/member_pointer|P<>]]`
- `[[04_Reference/rules|Rule]]`
- `[[04_Reference/select|select_query]]`
- `[[04_Reference/mockdb|MockDB]]`

The vault should prefer explicit links to existing notes or anchored sections rather than relying on dangling concept-note names that are not part of the planned structure.

Each note will end with a `Related Notes` section to prevent dead-end pages.

## Content Strategy by Section

### 01 Getting Started

This section will explain how to use the ORM from a reader-facing perspective.

Planned focus:

- what the project does
- how structs become tables
- how `property` and `relationship` fields are declared
- how `insert`, `select`, `update`, and `deleteq` appear in user code
- how execution happens through `db << query`
- how the example files demonstrate the end-to-end workflow

Tone:

- practical
- example-driven
- low-jargon relative to the deeper sections

### 02 Query Flow

This section will trace the codemap-backed flow from definition to execution.

Planned focus:

- field declarations and compile-time column names
- table reflection through `Table<T>` and PFR
- `P<>` wrappers and member-pointer expression syntax
- rule-tree creation and compile-time boolean rewrites
- `select_query` and `insert_query` construction
- `operator<<` as the execution boundary
- `MockDB` dispatch and SQL serialization

Tone:

- step-by-step
- flow-oriented
- implementation-aware

### 03 Architecture

This section will describe the major subsystems and their roles.

Planned focus:

- field system
- reflection and tuple conversion
- type-wrapper system
- expression system
- CRUD builder composition
- connector architecture with emphasis on `MockDB`

Tone:

- contributor-oriented
- conceptual
- responsibility-based

### 04 Reference

This section will provide concise, scan-friendly file and concept references.

Each note will explain:

- file role
- key types and symbols
- important behaviors
- who uses it or what it feeds into
- relevant source paths

This section complements the architecture notes rather than replacing them.

### 05 Glossary

This section will define recurring terms used across the vault, such as:

- property
- relationship
- placeholder
- member pointer
- rule
- statement
- select_query
- insert_query
- response tuple
- connector

## Note Templates

### Onboarding notes

```md
# Title

## Purpose

## Key Idea

## Example in This Project

## How to Use It

## Common Pitfalls

## Related Notes
```

### Query-flow notes

```md
# Title

## Where This Step Fits in the Flow

## Inputs

## What Happens

## Output / Next Stage

## Relevant Files

## Related Notes
```

### Architecture notes

```md
# Title

## Responsibility

## Main Types and Functions

## Data Flow

## Compile-Time Guarantees

## Dependencies

## Related Notes
```

### Reference notes

```md
# Title

## File Role

## Key Types / Symbols

## Important Behaviors

## Called By / Used By

## Source Files

## Related Notes
```

## Writing Rules for the Vault

- prefer actual source behavior over historical or aspirational descriptions
- keep examples consistent with `example/users/users.hpp` and `example/ORM.cpp`
- explain compile-time behavior without overstating guarantees that are not explicit in code
- distinguish reader-facing syntax from implementation internals
- use Obsidian callouts where useful:
  - `> [!info]`
  - `> [!tip]`
  - `> [!note]`
  - `> [!warning]`
- keep the reference notes concise and the flow/architecture notes more explanatory

## Planned First-Pass Coverage

The first pass will create all notes listed in the information architecture section.

The first pass will not attempt to exhaustively document every header or every helper template in the repository. Instead, it will prioritize the core flow described by the codemap and the public-to-internal path that readers are most likely to follow.

## Verification Plan

After authoring the vault:

- confirm all expected markdown files exist in the intended structure
- spot-check note content for consistency, cross-links, and terminology
- run the repository's required full build and full test workflow before considering the task complete, in line with the repository rules

## Risks and Mitigations

### Risk: README and source are not fully aligned

Mitigation:

- prefer current source behavior
- treat the README as high-level context, not as the final authority on internals

### Risk: Codemap may lag behind current files

Mitigation:

- use codemap as organizational guidance
- verify details against the current source files while writing each note

### Risk: Reference notes become too thin or architecture notes too dense

Mitigation:

- keep reference notes file-centric and concise
- keep architecture notes subsystem-centric and explanatory
- cross-link instead of duplicating large blocks of text

## Success Criteria

The design will be successful if:

- a new reader can start at `00_Home.md` and reach a working mental model of tables, queries, and execution
- a contributor can trace the path from `P<>` and `Rule` expressions to `MockDB` SQL output
- a maintainer can use the reference section to quickly locate ownership and responsibilities across the core ORM files
- the resulting vault feels like a real Obsidian knowledge base rather than a flat markdown dump
