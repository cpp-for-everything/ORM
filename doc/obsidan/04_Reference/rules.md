# rules.hpp and rules.cpp

## File Role

Defines the `Rule` type family and the operator overloads that build boolean query expressions.

## Key Types / Symbols

- `IRule`
- `Rule<T1, op, T2>`
- `IMPLEMENT_RULE_OPERATOR(...)`
- `IMPLEMENT_MEM_PTR_OPERATOR(...)`
- `operator!(Rule<...>)`
- `exists(...)`
- `not_exists(...)`
- `not_op<...>()`
- `not_type<...>`

## Important Behaviors

- builds typed comparison rules from member pointers and values
- builds typed boolean combinations of existing rules
- rewrites operators recursively when a rule is negated with `operator!`
- supports existence-style rules through `exists(...)` and `not_exists(...)`
- provides `operator<<` overloads for debugging/printing rule values

## Called By / Used By

Used directly by:

- select join and where clauses
- update and delete filters
- group-by `HAVING` expressions
- connector serialization code in `MockDB`

Consumes:

- member-pointer wrappers from `member_pointer.hpp`

## Source Files

- `lib/include/ORM/rules.hpp`
- `lib/src/ORM/rules.cpp`

## Related Notes

- [[03_Architecture/04_Expression_System|Expression System]]
- [[02_Query_Flow/03_Rules_and_Compile_Time_Expressions|Rules and Compile-Time Expressions]]
- [[02_Query_Flow/07_MockDB_SQL_Generation|MockDB SQL Generation]]
- [[04_Reference/member_pointer|member_pointer.hpp]]
