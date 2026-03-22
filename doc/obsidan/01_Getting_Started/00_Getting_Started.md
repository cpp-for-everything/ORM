# Getting Started

This section explains how to read and use the ORM from the outside in.

It stays close to the examples in `example/users/users.hpp` and `example/ORM.cpp`, so the onboarding path reflects the code that currently exists in the repository.

> [!tip]
> If you want to understand how a user-facing query eventually becomes SQL, read this section first and then continue with [[02_Query_Flow/00_Query_Flow|Query Flow]].

## Start Here

- [[01_Getting_Started/01_What_This_ORM_Is|What This ORM Is]]
- [[01_Getting_Started/02_Defining_Tables_and_Fields|Defining Tables and Fields]]
- [[01_Getting_Started/03_Relationships|Relationships]]
- [[01_Getting_Started/04_Building_Queries|Building Queries]]
- [[01_Getting_Started/05_Executing_Queries|Executing Queries]]
- [[01_Getting_Started/06_End_to_End_Example|End-to-End Example]]

## What You Will Learn

- how structs become tables through `table_name`, `property`, and `relationship`
- how the example code defines `insert`, `select`, `update`, and `deleteq` queries
- how parameters are supplied at execution time through `db << query`
- which pieces are user-facing syntax and which belong to the internal implementation

## Recommended Reading Orders

### First-time user

- [[01_Getting_Started/01_What_This_ORM_Is|What This ORM Is]]
- [[01_Getting_Started/02_Defining_Tables_and_Fields|Defining Tables and Fields]]
- [[01_Getting_Started/04_Building_Queries|Building Queries]]
- [[01_Getting_Started/05_Executing_Queries|Executing Queries]]

### Reader coming from the example program

- [[01_Getting_Started/06_End_to_End_Example|End-to-End Example]]
- [[01_Getting_Started/04_Building_Queries|Building Queries]]
- [[01_Getting_Started/05_Executing_Queries|Executing Queries]]

## Notes in This Section

- [[01_Getting_Started/01_What_This_ORM_Is|What This ORM Is]]
- [[01_Getting_Started/02_Defining_Tables_and_Fields|Defining Tables and Fields]]
- [[01_Getting_Started/03_Relationships|Relationships]]
- [[01_Getting_Started/04_Building_Queries|Building Queries]]
- [[01_Getting_Started/05_Executing_Queries|Executing Queries]]
- [[01_Getting_Started/06_End_to_End_Example|End-to-End Example]]

## Related Notes

- [[00_Home|Home]]
- [[02_Query_Flow/00_Query_Flow|Query Flow]]
- [[03_Architecture/00_Architecture|Architecture]]
- [[04_Reference/00_Reference|Reference]]
