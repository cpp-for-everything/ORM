# fields.hpp

## File Role

Defines the schema-facing field types used in entity declarations.

This is the main entry point for `property<...>` and `relationship<...>` declarations.

## Key Types / Symbols

- `details::property`
- `details::relationship`
- `property<T, lit>`
- `RelationshipTypes`
- `relationship<RelationshipTypes, external_ref, name>`

## Important Behaviors

- exposes compile-time field names through `_name()`
- distinguishes column-like and relationship-like members through marker-base inheritance
- preserves the wrapped value type through `var_t`
- stores relationship kind and referenced source type in the relationship type

## Called By / Used By

Used directly by:

- entity declarations in `example/users/users.hpp`
- `Table<T>` reflection helpers
- member-pointer traits and query-building utilities that inspect property categories

Feeds into:

- expression construction through member pointers such as `&User::id`
- placeholder/property extraction utilities in `orm_tuple.hpp`

## Source Files

- `lib/include/ORM/fields.hpp`

## Related Notes

- [[03_Architecture/01_Field_System|Field System]]
- [[03_Architecture/03_Type_Wrapper_System|Type Wrapper System]]
- [[02_Query_Flow/01_From_Struct_to_Table|From Struct to Table]]
- [[04_Reference/table|table.hpp]]
- [[04_Reference/example_users|example users]]
