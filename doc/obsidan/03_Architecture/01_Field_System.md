# Field System

## Responsibility

The field system defines how table-like structs declare schema-facing members.

Its job is to give the rest of the ORM a uniform way to recognize:

- column-like fields
- relationship-like fields
- compile-time names
- wrapped native value types

## Main Types and Functions

- `webframe::ORM::property<T, lit>`
- `webframe::ORM::relationship<RelationshipTypes, external_ref, name>`
- `webframe::ORM::details::property`
- `webframe::ORM::details::relationship`
- `webframe::ORM::RelationshipTypes`

Important details in the current implementation:

- `property<T, lit>` exposes `_name()`
- wrapped value types expose `var_t`
- relationship fields expose `Source`, `type`, and `_name()`
- properties and relationships are detected through inheritance from marker base classes

## Data Flow

The field system feeds almost every later subsystem.

1. entities such as `User` and `UserPost` declare `property` and `relationship` members
2. `Table<T>` uses those members with PFR to perform tuple conversion and column lookup
3. member pointers such as `&User::id` rely on those field types to recover table and value information
4. query builders and connector helpers inspect fields through property/relationship markers and member-pointer traits

## Compile-Time Guarantees

The current code gives a few important compile-time benefits:

- field names are encoded as string literals in the type system
- field categories are available through marker inheritance
- relationship kind and referenced source type are part of the relationship type
- property wrapper types preserve the declared DB-facing wrapper type

The code does **not** by itself guarantee automatic join generation or automatic relationship loading.

## Dependencies

The field system depends on:

- `ORM/details/string_literal.hpp`
- `ORM/details/fundamental_type.hpp`
- `ORM/details/member_pointer.hpp`
- PFR headers used indirectly by table reflection
- DB wrapper types such as `INTEGER<>`, `TEXT<>`, and `Nullable<T>`

## Related Notes

- [[03_Architecture/02_Table_Reflection|Table Reflection]]
- [[03_Architecture/03_Type_Wrapper_System|Type Wrapper System]]
- [[02_Query_Flow/01_From_Struct_to_Table|From Struct to Table]]
- [[01_Getting_Started/02_Defining_Tables_and_Fields|Defining Tables and Fields]]
- [[04_Reference/fields|fields.hpp]]
