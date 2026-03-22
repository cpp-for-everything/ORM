# type wrappers

## File Role

Defines the DB-oriented wrapper types used by field declarations, including primitive wrappers, string wrappers, bit wrappers, boolean wrappers, numeric aliases, and nullability.

## Key Types / Symbols

- `Primitive<T, name, size, P>`
- `INTEGER<>`
- `FLOAT<>`
- `DOUBLE<>`
- `DECIMAL<>`
- `CHAR<>`
- `VARCHAR<>`
- `TEXT<>`
- `BOOL`
- `Nullable<T>`

## Important Behaviors

- attaches DB metadata such as `db_type`, `db_size`, and `db_percision`
- exposes `native_type` for wrapped value access
- mixes in many value-like operators through helper wrapper classes
- provides `Nullable<T>::is_null()` to represent nullability in the type layer

## Called By / Used By

Used directly by:

- entity field declarations in `example/users/users.hpp`
- `property<T, lit>` declarations that wrap DB-facing value types

Supports:

- reflection and field metadata lookup
- connector formatting and type-oriented documentation

## Source Files

- `lib/include/ORM/db/types/db_type_wrappers.hpp`
- `lib/include/ORM/db/types/primitive_type_wrapper.hpp`

## Related Notes

- [[03_Architecture/03_Type_Wrapper_System|Type Wrapper System]]
- [[03_Architecture/01_Field_System|Field System]]
- [[04_Reference/fields|fields.hpp]]
- [[01_Getting_Started/02_Defining_Tables_and_Fields|Defining Tables and Fields]]
