# Expression System

## Responsibility

The expression subsystem turns field identities and literal values into typed statements and boolean rules.

It is the bridge between schema-level declarations and query-level conditions.

## Main Types and Functions

- `details::i_mem_ptr<T>`
- `details::mem_ptr<P>`
- `P<Property>`
- `Statement<T1, op, T2>`
- `Rule<T1, op, T2>`
- `operator!` for `Rule<...>`
- `exists(...)`
- `not_exists(...)`

Important implementation patterns:

- member-pointer decomposition through `i_mem_ptr`
- assignment-style statement creation in `mem_ptr::operator=(...)`
- comparison-rule creation through operator macros in `rules.cpp`
- recursive negation rewriting through `not_op<...>()` and `not_type<...>`

## Data Flow

1. a field is identified by a member pointer such as `&User::id`
2. `P<>` wraps that pointer into an expression operand
3. assignment syntax creates `Statement` values for update builders
4. comparison syntax creates `Rule` values for joins, where clauses, and grouping rules
5. nested rule trees are stored inside query builders and later consumed by connector code

## Compile-Time Guarantees

The expression system encodes a large amount of structure in types:

- operand categories
- operator strings
- table and field ownership for member pointers
- recursive rule-tree shape
- negated operator rewrites for `operator!`

That said, the emitted SQL text is still produced later by runtime string-building logic inside the connector.

## Dependencies

The expression system depends on:

- `ORM/details/member_pointer.hpp`
- `ORM/rules.hpp`
- `ORM/details/string_literal.hpp`
- placeholder support from `ORM/details/orm_tuple.hpp`
- field declarations that supply member-pointer target types

## Related Notes

- [[02_Query_Flow/02_Member_Pointers_and_P_Syntax|Member Pointers and P Syntax]]
- [[02_Query_Flow/03_Rules_and_Compile_Time_Expressions|Rules and Compile-Time Expressions]]
- [[03_Architecture/05_CRUD_Builder_Architecture|CRUD Builder Architecture]]
- [[04_Reference/member_pointer|member_pointer.hpp]]
- [[04_Reference/rules|rules.hpp and rules.cpp]]
