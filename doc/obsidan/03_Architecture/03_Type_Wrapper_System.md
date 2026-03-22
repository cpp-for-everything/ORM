# Type Wrapper System

## Responsibility

The type-wrapper subsystem gives fields DB-oriented wrapper types instead of exposing raw primitives everywhere.

Its role is to preserve DB-facing metadata such as logical type names, sizes, and nullability while still allowing the wrapped values to behave like C++ values in many contexts.

## Main Types and Functions

Key types include:

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

Important supporting pieces:

- operator-wrapper mixins in `primitive_type_wrapper.hpp`
- `native_type`
- `db_type`
- `db_size`
- `db_percision`
- `Nullable<T>::is_null()`

## Data Flow

1. entity fields choose DB wrapper types such as `INTEGER<>` and `TEXT<>`
2. `property<T, lit>` stores the wrapper type as part of the field definition
3. member-pointer and tuple helpers can recover the wrapped value type through field metadata
4. query builders and connector formatting logic ultimately work with the underlying values or wrapper-provided metadata

## Compile-Time Guarantees

The current wrapper layer encodes several facts in types and constants:

- DB type names such as `"INTEGER"`, `"VARCHAR"`, or `"BOOL"`
- declared size and precision metadata
- nullability through `Nullable<T>`
- wrapper/native-type relationships through `native_type`

At the same time, the wrappers also expose runtime value behavior through conversions and operator mixins.

## Dependencies

The wrapper system depends on:

- standard numeric/string/bitset types
- concepts and operator-detection machinery in `primitive_type_wrapper.hpp`
- field declarations that consume wrapper types through `property<T, lit>`

## Related Notes

- [[03_Architecture/01_Field_System|Field System]]
- [[01_Getting_Started/02_Defining_Tables_and_Fields|Defining Tables and Fields]]
- [[04_Reference/type_wrappers|type wrappers]]
- [[04_Reference/fields|fields.hpp]]
