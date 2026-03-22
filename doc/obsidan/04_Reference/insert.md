# insert.hpp

## File Role

Defines the INSERT query object family and the top-level `insert<...>` entry point.

## Key Types / Symbols

- `CRUD::insert_query<Properties...>`
- `properties`
- `tables`
- `signature`
- top-level `insert<...>`

## Important Behaviors

- derives the runtime input shape from placeholders and property-driven inputs through `get_placeholders_and_properties<...>`
- infers the participating tables through `unique_tables<...>`
- stores the original property sequence in `signature`
- leaves most execution-specific INSERT formatting work to the connector layer

## Called By / Used By

Used directly by:

- insert-query declarations in `example/users/users.hpp`
- `operator<<` execution through query marker inheritance
- `MockDB::execute_insert(...)`

Depends on:

- `CRUD/utils.hpp`
- `orm_tuple.hpp`
- member-pointer-based property inputs

## Source Files

- `lib/include/ORM/CRUD/insert.hpp`
- `lib/src/ORM/CRUD/insert.cpp`

## Related Notes

- [[03_Architecture/05_CRUD_Builder_Architecture|CRUD Builder Architecture]]
- [[02_Query_Flow/05_Insert_Query_Construction|Insert Query Construction]]
- [[04_Reference/crud_utils|CRUD utils]]
- [[04_Reference/mockdb|MockDB connector]]
