# Table Reflection

## Responsibility

The table-reflection subsystem gives entity types tuple-oriented helpers.

Its responsibility is not full metadata registration. Instead, it provides practical conversion and lookup utilities over table-like structs using PFR field order and field categories.

## Main Types and Functions

- `webframe::ORM::Table<T>`
- `Table<T>::to_struct(...)`
- `Table<T>::to_tuple(...)`
- `Table<T>::tuple_to_struct(...)`
- `Table<T>::get_index_by_column(...)`
- `webframe::ORM::DBTable<name>`

Implementation helpers in `table.cpp` include:

- `_get_index_by_column<i>(...)`
- `_tuple_to_struct<i>(...)`
- `_to_struct<i>(...)`
- `_to_tuple<i>(...)`

## Data Flow

1. entity types declare ordered fields
2. `Table<T>` exposes `tuple_t` through `pfr::structure_to_tuple(...)`
3. tuple conversion helpers copy values between tuples and struct fields
4. column lookup walks the fields in order and matches property names while skipping relationships
5. the example program uses these helpers before any query execution happens

## Compile-Time Guarantees

The reflection layer gives compile-time structure in several ways:

- `tuple_t` is derived from the entity type at compile time
- field order is fixed by the entity layout as seen by PFR
- property-vs-relationship checks are resolved through type traits and inheritance

The actual conversion work still happens as normal runtime code over compile-time-shaped helpers.

## Dependencies

The table-reflection layer depends on:

- `ORM/fields.hpp`
- PFR tuple reflection
- standard tuple utilities
- entity field order staying compatible with PFR reflection assumptions

## Related Notes

- [[03_Architecture/01_Field_System|Field System]]
- [[02_Query_Flow/01_From_Struct_to_Table|From Struct to Table]]
- [[01_Getting_Started/02_Defining_Tables_and_Fields|Defining Tables and Fields]]
- [[04_Reference/table|table.hpp]]
- [[04_Reference/example_orm|example ORM program]]
