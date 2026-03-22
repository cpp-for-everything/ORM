# Query Flow

This section traces the path from user-defined structs and member pointers to executable query objects and finally to SQL text produced by `MockDB`.

It is the best place to read if you want to understand how the public syntax is implemented internally.

> [!info]
> The flow notes focus on how the current code composes templates and tuples. They do not assume features that only appear in TODO comments or older README examples.

## Start Here

- [[01_From_Struct_to_Table|From Struct to Table]]
- [[02_Member_Pointers_and_P_Syntax|Member Pointers and P Syntax]]
- [[03_Rules_and_Compile_Time_Expressions|Rules and Compile-Time Expressions]]
- [[04_Select_Query_Construction|Select Query Construction]]
- [[05_Insert_Query_Construction|Insert Query Construction]]
- [[06_Query_Execution_via_operator_shift|Query Execution via operator<<]]
- [[07_MockDB_SQL_Generation|MockDB SQL Generation]]

## Flow Summary

- structs declare `property` and `relationship` members
- `Table<T>` and PFR expose tuple-oriented reflection helpers
- `P<>` turns member pointers into expression operands
- comparison operators build `Rule` trees
- CRUD builders accumulate joins, filters, limits, grouping, and ordering in `orm_tuple` containers
- `operator<<` turns a query object into a callable execution boundary
- `MockDB::MockDB::execute(...)` dispatches by query family and serializes SQL text

## Recommended Reading Orders

### From public syntax to SQL

- [[01_From_Struct_to_Table|From Struct to Table]]
- [[02_Member_Pointers_and_P_Syntax|Member Pointers and P Syntax]]
- [[03_Rules_and_Compile_Time_Expressions|Rules and Compile-Time Expressions]]
- [[04_Select_Query_Construction|Select Query Construction]]
- [[06_Query_Execution_via_operator_shift|Query Execution via operator<<]]
- [[07_MockDB_SQL_Generation|MockDB SQL Generation]]

### Focused on INSERT

- [[01_From_Struct_to_Table|From Struct to Table]]
- [[05_Insert_Query_Construction|Insert Query Construction]]
- [[06_Query_Execution_via_operator_shift|Query Execution via operator<<]]
- [[07_MockDB_SQL_Generation|MockDB SQL Generation]]

## Related Notes

- [[00_Home|Home]]
- [[01_Getting_Started/00_Getting_Started|Getting Started]]
- [[03_Architecture/00_Architecture|Architecture]]
- [[04_Reference/00_Reference|Reference]]
