# Building Queries

## Purpose

Show how the current ORM exposes query construction in reader-facing code.

## Key Idea

The public query surface is built around `constexpr` query objects.

In the current codebase, you build queries by starting with one of the top-level entry points and then chaining builder calls:

- `insert<...>`
- `select<modes::..., ...>`
- `update(...)`
- `deleteq<Table>`

The chained calls do not execute anything yet. They build typed query objects that store compile-time structure and deferred parameter positions.

## Example in This Project

`example/users/users.hpp` shows all four query families:

- **Insert**
  - `insert_new_user_with_id_placeholder`
  - `insert_new_user`

- **Select**
  - `get_all_users_with_id_above`
  - `get_all_posts_with_their_assosiated_users`

- **Update**
  - `update_with_optimized_rules`
  - `update_something`
  - `update_something_2`

- **Delete**
  - `delete_all_posts`

The query builders use a few recurring pieces of syntax:

- member pointers such as `&User::id`
- `P<>` wrappers such as `P<&User::id>`
- placeholders such as `P<Placeholder<int>>`
- explicit joins and filters written as `Rule` expressions
- pagination literals such as `15_per_page & 5_page`

## How to Use It

### Insert

`insert<&User::username>` means:

- the query inserts into the table inferred from the supplied properties
- the values for those properties are provided later at execution time

### Select

`select<modes::ALL, &User::id, &User::username>` means:

- choose a select mode
- declare which properties should be returned
- optionally chain `.join(...)`, `.where(...)`, `.limit(...)`, `.group_by(...)`, and `.order_by<...>()`

### Update

`update(...)` starts from assignment-like `Statement` objects, for example:

- `P<&User::id> = P<Placeholder<int>>`
- `P<&User::username> = "Test"`

You can then chain `.where(...)`, `.order_by<...>()`, and `.limit(...)`.

### Delete

`deleteq<UserPost>` starts from a table type and then adds `.where(...)`, `.order_by<...>()`, and `.limit(...)` clauses.

> [!tip]
> `15_per_page & 5_page` and `5_page & 15_per_page` both produce a `Pagification` object in the current implementation.

## Common Pitfalls

- The README still shows older names such as `.filter(...)`; the current query builder uses `.where(...)`.
- `relationship` declarations do not remove the need for explicit join rules.
- Query construction and query execution are separate phases. Chaining builders only creates query objects.
- `insert_query::properties` tracks placeholders and property-derived inputs, so the runtime argument list is shaped by the query signature, not by handwritten SQL text.

## Related Notes

- [[01_What_This_ORM_Is|What This ORM Is]]
- [[05_Executing_Queries|Executing Queries]]
- [[06_End_to_End_Example|End-to-End Example]]
- [[02_Query_Flow/02_Member_Pointers_and_P_Syntax|Member Pointers and P Syntax]]
- [[02_Query_Flow/03_Rules_and_Compile_Time_Expressions|Rules and Compile-Time Expressions]]
- [[02_Query_Flow/04_Select_Query_Construction|Select Query Construction]]
- [[02_Query_Flow/05_Insert_Query_Construction|Insert Query Construction]]
