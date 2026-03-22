# What This ORM Is

## Purpose

Explain what this library does at a high level and how to read the rest of the vault.

## Key Idea

This project is a compile-time oriented ORM toolkit for C++.

Instead of building SQL strings directly in application code, you declare table-like structs, describe queries through template-based builders, and then execute those query objects through a connector.

In the current repository state:

- schema-like fields are declared with `property<...>` and `relationship<...>`
- query objects are usually defined as `static constexpr` values
- execution happens through `db << query`
- the bundled `MockDB` connector serializes queries into SQL text instead of talking to a real database server

> [!info]
> The code strongly emphasizes compile-time structure, but not every behavior is fully validated or optimized at compile time. This vault only claims guarantees that are visible in the current source.

## Example in This Project

The main example lives in:

- `example/users/users.hpp`
- `example/ORM.cpp`

`Post`, `User`, and `UserPost` act as tables. `Utils<User>` and `Utils<UserPost>` then expose reusable query objects such as:

- `insert_new_user`
- `get_all_users_with_id_above`
- `update_with_optimized_rules`
- `get_all_posts_with_their_assosiated_users`
- `delete_all_posts`

## How to Use It

A typical usage path in this repository looks like this:

1. declare a struct with `table_name` and ORM fields
2. define one or more `static constexpr` queries near that struct or in a helper type
3. create a connector such as `MockDB::MockDB`
4. execute a query with `(db << query)(params...)`

If you are learning the project, start with the schema declarations first and then move to query building.

## Common Pitfalls

- The README shows some older calling patterns. The current example code uses `where(...)`, not `filter(...)`.
- The bundled example connector is `MockDB`, which emits SQL strings. That is useful for understanding the pipeline, but it is not the same thing as a full production database integration.
- `relationship` fields describe schema-level links, but joins in the example queries are still written explicitly with `P<>` and `Rule` expressions.

## Related Notes

- [[02_Defining_Tables_and_Fields|Defining Tables and Fields]]
- [[03_Relationships|Relationships]]
- [[04_Building_Queries|Building Queries]]
- [[05_Executing_Queries|Executing Queries]]
- [[06_End_to_End_Example|End-to-End Example]]
- [[02_Query_Flow/00_Query_Flow|Query Flow]]
