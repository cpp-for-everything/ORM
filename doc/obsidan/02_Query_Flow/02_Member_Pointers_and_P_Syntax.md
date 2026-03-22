# Member Pointers and P Syntax

## Where This Step Fits in the Flow

Once a struct has table-like fields, the next step is to identify those fields in query expressions.

This project does that through C++ member pointers such as `&User::id`, wrapped in `P<>` when expression syntax is needed.

## Inputs

The main inputs are:

- member pointers such as `&User::id` and `&Post::content`
- the `details::i_mem_ptr<T>` trait family
- the `details::mem_ptr<P>` wrapper
- the public `P<Property>` variable template

## What Happens

`details::i_mem_ptr<T _Table::*>` decomposes a member pointer into:

- `Table`
- `type`
- `ptr_t`

`details::mem_ptr<property>` then stores the member pointer at the type level and exposes `get()`.

The public shorthand:

`P<&User::id>`

creates a `details::mem_ptr<&User::id>` object.

That wrapper is important because it adds expression-oriented operators. In `member_pointer.hpp`, `mem_ptr<property>::operator=(...)` creates a `Statement<...>`, which is later used by update queries.

The wrapper also gives the rule system a uniform operand type when comparisons like these are written:

- `P<&User::id> > P<Placeholder<int>>`
- `P<&UserPost::post> == P<&Post::id>`
- `P<&UserPost::id> != P<nullptr>`

`P<nullptr>` is handled by a specialized `mem_ptr<nullptr>` based on the internal `Filler::Null` pseudo-property.

## Output / Next Stage

After this stage, the ORM can form typed operands for:

- update statements
- comparison rules
- placeholder-aware filters and joins
- property extraction inside query builders and connectors

The next step is combining these operands into `Rule` trees and boolean expressions.

## Relevant Files

- `lib/include/ORM/details/member_pointer.hpp`
- `lib/src/ORM/details/member_pointer.cpp`
- `example/users/users.hpp`

## Related Notes

- [[02_Query_Flow/03_Rules_and_Compile_Time_Expressions|Rules and Compile-Time Expressions]]
- [[03_Architecture/04_Expression_System|Expression System]]
- [[04_Reference/member_pointer|P<>]]
- [[04_Reference/example_users|example users]]
