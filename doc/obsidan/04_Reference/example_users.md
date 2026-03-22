# example users

## File Role

Provides the main example entity types and reusable query declarations that the rest of the vault uses as reader-facing anchors.

## Key Types / Symbols

- `Post`
- `User`
- `UserPost`
- `Utils<User>`
- `Utils<UserPost>`
- `insert_new_user`
- `get_all_users_with_id_above`
- `update_with_optimized_rules`
- `get_all_posts_with_their_assosiated_users`
- `delete_all_posts`

## Important Behaviors

- declares example schema types with `property` and `relationship`
- demonstrates insert, select, update, and delete query construction
- shows placeholder-based runtime argument positions
- exercises join, grouping, ordering, and limit syntax in one place

## Called By / Used By

Used directly by:

- `example/ORM.cpp`
- the onboarding notes in this vault
- the query-flow notes that explain how public syntax maps to internals

## Source Files

- `example/users/users.hpp`

## Related Notes

- [[01_Getting_Started/06_End_to_End_Example|End-to-End Example]]
- [[01_Getting_Started/04_Building_Queries|Building Queries]]
- [[04_Reference/example_orm|example ORM program]]
- [[04_Reference/fields|fields.hpp]]
- [[04_Reference/select|select.hpp]]
