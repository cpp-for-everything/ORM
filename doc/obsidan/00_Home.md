# ORM Vault Home

This vault documents the current compile-time ORM implementation in this repository.

It is designed for three reading styles at the same time:

- onboarding, if you want to understand how to use the library from the examples
- architecture, if you want to trace how tables, expressions, and connectors fit together
- reference, if you want fast access to the files and symbols that own the core behavior

> [!info]
> This vault prefers current source behavior over older README wording, TODO items, or aspirational API descriptions.

## Start Here

- **New to the project**
  - Start with [[01_Getting_Started/00_Getting_Started|Getting Started]]

- **Trying to understand the end-to-end pipeline**
  - Start with [[02_Query_Flow/00_Query_Flow|Query Flow]]

- **Trying to modify or review internals**
  - Start with [[03_Architecture/00_Architecture|Architecture]]

- **Trying to locate a specific file or symbol**
  - Start with [[04_Reference/00_Reference|Reference]]

- **Trying to decode recurring terms**
  - Start with [[05_Glossary/Glossary|Glossary]]

## Reading Paths

### Reader-facing path

- [[01_Getting_Started/01_What_This_ORM_Is|What This ORM Is]]
- [[01_Getting_Started/02_Defining_Tables_and_Fields|Defining Tables and Fields]]
- [[01_Getting_Started/04_Building_Queries|Building Queries]]
- [[01_Getting_Started/05_Executing_Queries|Executing Queries]]
- [[01_Getting_Started/06_End_to_End_Example|End-to-End Example]]

### Query-flow path

- [[02_Query_Flow/01_From_Struct_to_Table|From Struct to Table]]
- [[02_Query_Flow/02_Member_Pointers_and_P_Syntax|Member Pointers and P Syntax]]
- [[02_Query_Flow/03_Rules_and_Compile_Time_Expressions|Rules and Compile-Time Expressions]]
- [[02_Query_Flow/04_Select_Query_Construction|Select Query Construction]]
- [[02_Query_Flow/06_Query_Execution_via_operator_shift|Query Execution via operator<<]]
- [[02_Query_Flow/07_MockDB_SQL_Generation|MockDB SQL Generation]]

### Contributor path

- [[03_Architecture/01_Field_System|Field System]]
- [[03_Architecture/02_Table_Reflection|Table Reflection]]
- [[03_Architecture/04_Expression_System|Expression System]]
- [[03_Architecture/05_CRUD_Builder_Architecture|CRUD Builder Architecture]]
- [[03_Architecture/06_Connector_Architecture|Connector Architecture]]

## Source Anchors

The notes in this vault are mainly grounded in:

- `example/users/users.hpp`
- `example/ORM.cpp`
- `lib/include/ORM/fields.hpp`
- `lib/include/ORM/table.hpp`
- `lib/include/ORM/details/member_pointer.hpp`
- `lib/include/ORM/rules.hpp`
- `lib/src/ORM/rules.cpp`
- `lib/include/ORM/CRUD/select.hpp`
- `lib/include/ORM/CRUD/insert.hpp`
- `lib/include/ORM/CRUD/utils.hpp`
- `lib/include/ORM/join_rule.hpp`
- `lib/include/ORM/db/connectors/MockDB/init.hpp`
- `lib/include/ORM/db/types/db_type_wrappers.hpp`
- `lib/include/ORM/db/types/primitive_type_wrapper.hpp`

## What This Vault Does Not Try to Do

- restate `README.md` line by line
- document every helper template in the repository on the first pass
- describe future work as if it already exists

## Related Notes

- [[01_Getting_Started/00_Getting_Started|Getting Started]]
- [[02_Query_Flow/00_Query_Flow|Query Flow]]
- [[03_Architecture/00_Architecture|Architecture]]
- [[04_Reference/00_Reference|Reference]]
- [[05_Glossary/Glossary|Glossary]]
