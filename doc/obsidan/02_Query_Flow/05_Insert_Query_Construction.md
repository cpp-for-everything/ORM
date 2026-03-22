# Insert Query Construction

## Where This Step Fits in the Flow

This is the INSERT-specific branch of query construction.

It starts after table fields are available as member pointers and before any connector executes the query.

## Inputs

The main inputs are:

- the top-level `insert<&Table::field, ...>` entry point
- member pointers that identify the inserted fields
- placeholder and property extraction helpers from `orm_tuple.hpp`
- unique-table discovery helpers from `CRUD/utils.hpp`

## What Happens

`insert<auto... Properties>` creates a `CRUD::insert_query<Properties...>` type.

That type is intentionally compact. In `insert.hpp`, it mainly exposes three important pieces of metadata:

- `properties`
  - derived through `get_placeholders_and_properties<...>`
  - includes deferred placeholder inputs and direct property-driven inputs used at execution time

- `tables`
  - derived through `details::unique_tables<Properties...>`
  - identifies which table or tables the insert refers to

- `signature`
  - an `orm_tuple` containing the original property sequence

The current `insert.cpp` implementation does not add more builder behavior. Most of the interesting INSERT work happens later inside `MockDB::execute_insert(...)`, which walks the query signature and the runtime argument tuple together.

## Output / Next Stage

After this stage, the ORM has a typed insert-query object that can be:

- passed through `db << query`
- checked for parameter compatibility
- serialized by `MockDB::execute_insert(...)`

## Relevant Files

- `lib/include/ORM/CRUD/insert.hpp`
- `lib/src/ORM/CRUD/insert.cpp`
- `lib/include/ORM/details/orm_tuple.hpp`
- `lib/include/ORM/CRUD/utils.hpp`
- `example/users/users.hpp`

## Related Notes

- [[02_Query_Flow/06_Query_Execution_via_operator_shift|Query Execution via operator<<]]
- [[02_Query_Flow/07_MockDB_SQL_Generation|MockDB SQL Generation]]
- [[03_Architecture/05_CRUD_Builder_Architecture|CRUD Builder Architecture]]
- [[04_Reference/insert|insert_query]]
- [[04_Reference/crud_utils|CRUD utils]]
