# example ORM program

## File Role

Provides the executable example that demonstrates tuple conversion, connector creation, query execution, and printed SQL output.

## Key Types / Symbols

- `create_table_helper<T, i>()`
- `create_table<T>()`
- `main()`
- `MockDB::MockDB db`
- `my_assert(...)`

## Important Behaviors

- prints a simple table-creation view over property fields
- exercises `Table<User>::to_tuple(...)` and `tuple_to_struct(...)`
- executes the example queries through `db << query`
- prints generated SQL strings from `MockDB`

## Called By / Used By

Used directly by:

- onboarding walkthroughs in this vault
- manual inspection of generated SQL behavior

Consumes:

- entity/query declarations from `example/users/users.hpp`
- connector behavior from `MockDB`

## Source Files

- `example/ORM.cpp`

## Related Notes

- [[01_Getting_Started/06_End_to_End_Example|End-to-End Example]]
- [[01_Getting_Started/05_Executing_Queries|Executing Queries]]
- [[04_Reference/example_users|example users]]
- [[04_Reference/mockdb|MockDB connector]]
- [[04_Reference/table|table.hpp]]
