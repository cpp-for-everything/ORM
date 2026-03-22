# Defining Tables and Fields

## Purpose

Show how ordinary C++ structs become ORM table definitions in this project.

## Key Idea

A table in this ORM is not registered through a separate schema DSL. Instead, the struct itself is the schema anchor.

The current pattern is:

- add `static constexpr std::string_view table_name`
- declare members with `property<T, "column">` for columns
- optionally declare `relationship<...>` members for typed links
- rely on `Table<T>` and PFR when tuple-style reflection is needed

`property<T, lit>` stores two important pieces of information:

- the wrapped value type
- the compile-time column name returned by `_name()`

## Example in This Project

`example/users/users.hpp` defines three table-like structs:

- `Post`
  - `table_name = "Posts"`
  - `id`
  - `content`

- `User`
  - `table_name = "Users"`
  - `id`
  - `username`
  - `posts`

- `UserPost`
  - `table_name = "UserPosts"`
  - `id`
  - `author`
  - `post`

The example uses DB-oriented wrapper types such as:

- `INTEGER<>`
- `TEXT<>`
- `Nullable<INTEGER<>>`

## How to Use It

To define a new table-like type in the style used by this repository:

1. declare a struct
2. add a `table_name`
3. choose wrapper types for each field
4. declare fields with `property<...>`
5. use `Table<T>` helpers when you need tuple conversion or column lookup

`Table<T>` currently exposes:

- `to_struct(...)`
- `to_tuple(...)`
- `tuple_to_struct(...)`
- `get_index_by_column(...)`

`example/ORM.cpp` demonstrates this by converting a tuple into a `User` instance and then checking the resulting field values.

> [!tip]
> `Table<T>::get_index_by_column(...)` skips `relationship` members and only matches fields derived from `details::property`.

## Common Pitfalls

- The examples in this repository use DB wrapper types, not plain `int` or `std::string`, so keep your expectations aligned with the current source.
- `Table<T>` depends on field order because it uses PFR tuple conversion.
- `relationship` members are part of the struct layout, but they are not treated as ordinary columns by helpers like `get_index_by_column(...)`.

## Related Notes

- [[01_Getting_Started/01_What_This_ORM_Is|What This ORM Is]]
- [[01_Getting_Started/03_Relationships|Relationships]]
- [[01_Getting_Started/06_End_to_End_Example|End-to-End Example]]
- [[02_Query_Flow/01_From_Struct_to_Table|From Struct to Table]]
- [[03_Architecture/01_Field_System|Field System]]
- [[04_Reference/fields|property]]
- [[04_Reference/table|Table<T>]]
