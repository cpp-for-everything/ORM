# Relationships

## Purpose

Explain how the project declares typed relationships between table-like structs.

## Key Idea

A relationship is declared with:

`relationship<RelationshipTypes::<kind>, &OtherTable::field, "name">`

In the current implementation, the relationship type records:

- the relationship kind such as `one2one` or `one2many`
- the external member pointer it points at
- a compile-time display name through `_name()`
- the referenced source type through `Source`

It is a typed schema declaration, not a hidden query planner.

## Example in This Project

`example/users/users.hpp` contains two styles of relationship:

- `User::posts`
  - `relationship<RelationshipTypes::one2many, &Post::id, "post_ids">`

- `UserPost::author`
  - `relationship<RelationshipTypes::one2one, &User::id, "user_id">`

- `UserPost::post`
  - `relationship<RelationshipTypes::one2one, &Post::id, "post_id">`

These declarations make the schema link explicit at the type level.

## How to Use It

When following the current project style:

1. decide whether the field is `one2one` or `one2many`
2. point the relationship at a member pointer from the referenced table
3. give the relationship a stable name string
4. use explicit join rules in query builders when you actually need joined query behavior

For example, the relationship fields in `UserPost` are later connected to joined queries through explicit `P<>` comparisons in `Utils<UserPost>::get_all_posts_with_their_assosiated_users`.

> [!note]
> The current relationship type stores metadata and an internal `records` container, but the example code does not show automatic eager loading or implicit join generation.

## Common Pitfalls

- Do not assume that declaring a `relationship` automatically creates a SQL `JOIN`.
- Relationship declarations and query join conditions are related, but they are not the same abstraction.
- `Table<T>::get_index_by_column(...)` ignores relationships because it only checks for `details::property`.

## Related Notes

- [[01_Getting_Started/02_Defining_Tables_and_Fields|Defining Tables and Fields]]
- [[01_Getting_Started/04_Building_Queries|Building Queries]]
- [[01_Getting_Started/06_End_to_End_Example|End-to-End Example]]
- [[02_Query_Flow/01_From_Struct_to_Table|From Struct to Table]]
- [[02_Query_Flow/04_Select_Query_Construction|Select Query Construction]]
- [[03_Architecture/01_Field_System|Field System]]
- [[04_Reference/fields|property]]
