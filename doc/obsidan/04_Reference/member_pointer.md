# member_pointer.hpp

## File Role

Defines the member-pointer traits, wrappers, and statement-building syntax used by the expression system.

## Key Types / Symbols

- `details::i_mem_ptr<T>`
- `details::mem_ptr<P>`
- `details::mem_ptr<nullptr>`
- `P<Property>`
- `IStatement`
- `Statement<T1, op, T2>`
- `Filler`

## Important Behaviors

- decomposes member pointers into owning table, field type, and pointer type
- provides the public `P<>` wrapper shorthand
- builds `Statement` objects from assignment syntax such as `P<&User::id> = ...`
- supplies a null pseudo-property through `mem_ptr<nullptr>` for expressions involving null

## Called By / Used By

Used directly by:

- rule construction in `rules.cpp`
- CRUD builders that accept `Statement` and `Rule` inputs
- query examples in `example/users/users.hpp`
- connector code that recovers table and field names from member pointers

## Source Files

- `lib/include/ORM/details/member_pointer.hpp`
- `lib/src/ORM/details/member_pointer.cpp`

## Related Notes

- [[03_Architecture/04_Expression_System|Expression System]]
- [[02_Query_Flow/02_Member_Pointers_and_P_Syntax|Member Pointers and P Syntax]]
- [[04_Reference/rules|rules.hpp and rules.cpp]]
- [[04_Reference/crud_utils|CRUD utils]]
