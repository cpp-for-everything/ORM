# From Struct to Table

## Where This Step Fits in the Flow

This is the start of the pipeline.

Before there are expressions, queries, or SQL strings, the ORM needs a type that acts as a table description. In this project, that description starts as an ordinary C++ struct with ORM field members.

## Inputs

The main inputs are:

- a struct with `static constexpr std::string_view table_name`
- members declared with `property<...>` or `relationship<...>`
- `Table<T>` when tuple conversion or column lookup is needed
- PFR tuple reflection for field order and field access

Representative examples:

- `Post`
- `User`
- `UserPost`

## What Happens

`property<T, lit>` contributes column-oriented metadata:

- the wrapped native value type through `var_t`
- the compile-time column name through `_name()`
- inheritance from `details::property` so the rest of the ORM can recognize it as a column-like field

`relationship<...>` contributes relationship-oriented metadata:

- the relationship kind
- the referenced source table type
- a compile-time relationship name
- inheritance from `details::relationship`

`Table<T>` then provides tuple-based helpers on top of the struct:

- `to_struct(...)`
- `to_tuple(...)`
- `tuple_to_struct(...)`
- `get_index_by_column(...)`

The implementation in `lib/src/ORM/table.cpp` uses PFR field order and field category checks to:

- assign tuple values back into struct members
- build tuples from argument lists
- look up column indices while skipping relationship members

> [!info]
> The current `Table<T>` utilities are layout- and order-driven. They depend on PFR field order rather than a separate runtime schema registry.

## Output / Next Stage

After this stage, the ORM has enough table and field metadata to support:

- member-pointer identities such as `&User::id`
- `P<>` wrappers over those member pointers
- property extraction from query rules and query builders

The next step is turning member pointers into expression operands.

## Relevant Files

- `example/users/users.hpp`
- `example/ORM.cpp`
- `lib/include/ORM/fields.hpp`
- `lib/include/ORM/table.hpp`
- `lib/src/ORM/table.cpp`

## Related Notes

- [[02_Query_Flow/02_Member_Pointers_and_P_Syntax|Member Pointers and P Syntax]]
- [[03_Architecture/01_Field_System|Field System]]
- [[03_Architecture/02_Table_Reflection|Table Reflection]]
- [[04_Reference/fields|property]]
- [[04_Reference/table|Table<T>]]
