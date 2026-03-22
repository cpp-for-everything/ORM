# Rules and Compile-Time Expressions

## Where This Step Fits in the Flow

After member pointers have been wrapped with `P<>`, the ORM can build boolean conditions for joins, filters, grouping, and existence checks.

This is the stage where public comparison syntax becomes typed `Rule` objects.

## Inputs

The main inputs are:

- `details::mem_ptr<P>` operands produced by `P<>`
- literal values such as `5` or `"Test"`
- placeholder operands such as `P<Placeholder<int>>`
- `Rule<T1, op, T2>` and `Statement<T1, op, T2>`
- overloaded comparison and boolean operators from `rules.cpp`

## What Happens

Comparison operators such as `==`, `!=`, `<`, `>`, `<=`, and `>=` are generated for `mem_ptr` operands through macros in `lib/src/ORM/rules.cpp`.

That lets code such as this produce typed `Rule` objects:

- `P<&User::id> > P<Placeholder<int>>`
- `P<&UserPost::post> == P<&Post::id>`
- `P<&UserPost::id> != P<nullptr>`

Boolean combination between rules is also overloaded, so you can write:

- `rule1 && rule2`
- `rule1 || rule2`
- `rule1 ^ rule2`

The current implementation also defines `operator!` for rules. It does not simply wrap the rule in a runtime negation node. Instead, it rewrites the operator at the type level through `not_op<...>()`, for example:

- `&&` becomes `||`
- `>` becomes `<=`
- `==` becomes `!=`
- `exists` becomes `not exists`

This rewrite is applied recursively through `not_type<Rule<...>>::convert(...)`.

> [!tip]
> Assignment-style syntax such as `P<&User::username> = "Test"` creates a `Statement`, not a `Rule`. That matters because update builders consume statements while filters and joins consume rules.

## Output / Next Stage

After this stage, the ORM has typed trees that can be stored inside query builders.

Those rule trees then feed into:

- select joins and `where(...)`
- update and delete filters
- group-by `HAVING` clauses
- SQL serialization in `MockDB`

## Relevant Files

- `lib/include/ORM/rules.hpp`
- `lib/src/ORM/rules.cpp`
- `lib/include/ORM/details/member_pointer.hpp`
- `example/users/users.hpp`

## Related Notes

- [[02_Query_Flow/04_Select_Query_Construction|Select Query Construction]]
- [[02_Query_Flow/06_Query_Execution_via_operator_shift|Query Execution via operator<<]]
- [[03_Architecture/04_Expression_System|Expression System]]
- [[04_Reference/rules|Rule]]
- [[04_Reference/member_pointer|P<>]]
