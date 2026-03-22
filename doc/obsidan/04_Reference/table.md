# table.hpp

## File Role

Declares `Table<T>`, the tuple-oriented helper layer over entity structs.

## Key Types / Symbols

- `DBTable<name>`
- `Table<T>`
- `Table<T>::tuple_t`
- `to_struct(...)`
- `to_tuple(...)`
- `get_index_by_column(...)`
- `tuple_to_struct(...)`

## Important Behaviors

- derives a tuple representation from an entity type through PFR
- converts argument lists into structs and tuples
- converts tuples back into entity structs
- resolves column indices by field name while ignoring relationship members

## Called By / Used By

Used directly by:

- `example/ORM.cpp` for tuple/struct conversion
- helper specializations such as `Utils<User> : Table<User>`

Supports:

- field lookup and reflection-aware onboarding examples
- query-related explanations that depend on member-pointer ownership and field layout

## Source Files

- `lib/include/ORM/table.hpp`
- `lib/src/ORM/table.cpp`

## Related Notes

- [[03_Architecture/02_Table_Reflection|Table Reflection]]
- [[02_Query_Flow/01_From_Struct_to_Table|From Struct to Table]]
- [[04_Reference/fields|fields.hpp]]
- [[04_Reference/example_orm|example ORM program]]
