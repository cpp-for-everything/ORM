# Select Query Construction

## Where This Step Fits in the Flow

This stage turns selected properties and rule expressions into a structured SELECT query object.

It sits between expression creation and connector execution.

## Inputs

The main inputs are:

- the top-level `select<modes::..., ...>` entry point
- selected member pointers such as `&User::id`
- optional join rules, where rules, limits, grouping, and ordering
- helper types such as `orm_tuple`, `result_t`, `get_properties`, and `get_placeholders`

## What Happens

`select<modes::..., args...>` creates a `CRUD::select_query<...>` object.

That object stores six logical parts:

- selected properties
- joins
- filters
- limits
- groupings
- ordering

In `select.hpp`, the query type exposes aliases such as:

- `Response`
- `Joins`
- `Wheres`
- `Limitations`
- `Groupings`
- `OrderBy`

The builder methods defined in `lib/src/ORM/CRUD/select.cpp` do not mutate in place. Each chained call builds a new `select_query` type with updated tuple state:

- `.join<modes::..., Table>(...)`
- `.where(...)`
- `.limit(...)`
- `.group_by<...>(...)`
- `.group_by<...>()`
- `.order_by<...>()`

The query's `properties` alias is computed from the placeholders that appear in joins, filters, and limits, so the runtime parameter list is derived from the stored query structure.

## Output / Next Stage

After this stage, the ORM has a fully typed select-query object that can be:

- passed to `db << query`
- checked against runtime parameters through `params_match`
- dispatched to `MockDB::execute_select(...)`

## Relevant Files

- `lib/include/ORM/CRUD/select.hpp`
- `lib/src/ORM/CRUD/select.cpp`
- `lib/include/ORM/CRUD/utils.hpp`
- `lib/include/ORM/details/result_type.hpp`
- `lib/include/ORM/details/orm_tuple.hpp`
- `lib/include/ORM/join_rule.hpp`
- `example/users/users.hpp`

## Related Notes

- [[02_Query_Flow/03_Rules_and_Compile_Time_Expressions|Rules and Compile-Time Expressions]]
- [[02_Query_Flow/06_Query_Execution_via_operator_shift|Query Execution via operator<<]]
- [[02_Query_Flow/07_MockDB_SQL_Generation|MockDB SQL Generation]]
- [[03_Architecture/05_CRUD_Builder_Architecture|CRUD Builder Architecture]]
- [[04_Reference/select|select_query]]
