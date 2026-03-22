# End-to-End Example

## Purpose

Walk through the example entities and example program as a single path from schema declaration to SQL output.

## Key Idea

The example in this repository is small, but it exercises the major ORM layers:

- table-like structs
- typed properties and relationships
- reusable query definitions
- tuple-to-struct conversion through `Table<T>`
- query execution through `MockDB`

## Example in This Project

### Step 1: Define the table-like structs

`example/users/users.hpp` declares:

- `Post`
- `User`
- `UserPost`

Each one has a `table_name`, and each uses ORM field types instead of plain primitive members.

### Step 2: Attach reusable queries

`Utils<User>` and `Utils<UserPost>` inherit from `Table<T>` and expose `static constexpr` queries.

This keeps schema-related types and the example queries close together.

### Step 3: Convert tuples into structs

`example/ORM.cpp` uses:

- `Table<User>::to_tuple(...)`
- `Table<User>::tuple_to_struct(...)`

That demonstrates the reflection and conversion layer separately from SQL execution.

### Step 4: Execute the queries through `MockDB`

The program creates `MockDB::MockDB db;` and then executes example queries such as:

- insert user
- insert user with explicit id placeholder
- select joined post/user data
- select users above an id threshold
- update user data
- delete posts through the mapping table

### Step 5: Print generated SQL

Because `MockDB` serializes rather than sending requests to a real DB, the example program prints generated SQL strings.

That makes the end-to-end flow easy to inspect.

> [!note]
> This example is best read together with the query-flow notes because it mixes public syntax with internal behaviors such as tuple reflection and rule serialization.

## How to Use It

If you are onboarding yourself to the codebase, read the example in this order:

1. `example/users/users.hpp`
2. `example/ORM.cpp`
3. [[02_Defining_Tables_and_Fields|Defining Tables and Fields]]
4. [[04_Building_Queries|Building Queries]]
5. [[05_Executing_Queries|Executing Queries]]
6. [[02_Query_Flow/00_Query_Flow|Query Flow]]

## Common Pitfalls

- The example output is SQL text from `MockDB`, not the result of a live database query.
- The example contains intentionally dense query chains to exercise many features at once.
- The type names and helper usage in the example are the most authoritative onboarding source in this repository, more so than some older README snippets.

## Related Notes

- [[01_What_This_ORM_Is|What This ORM Is]]
- [[02_Defining_Tables_and_Fields|Defining Tables and Fields]]
- [[03_Relationships|Relationships]]
- [[04_Building_Queries|Building Queries]]
- [[05_Executing_Queries|Executing Queries]]
- [[02_Query_Flow/00_Query_Flow|Query Flow]]
- [[04_Reference/example_users|example users]]
- [[04_Reference/example_orm|example ORM program]]
