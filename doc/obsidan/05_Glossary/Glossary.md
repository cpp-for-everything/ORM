# Glossary

## property

A column-like field declaration built with `webframe::ORM::property<T, "name">`.

## relationship

A field declaration that models a typed link to another table field through `webframe::ORM::relationship<...>`.

## member pointer

A C++ member pointer such as `&User::id`, used by the ORM as compile-time field identity.

## P<>

A wrapper object that turns a member pointer into an operand that can participate in statements and rules.

## Statement

A compile-time object produced by assignment-style syntax such as `P<&User::username> = "Test"`, mainly used by update queries.

## Rule

A compile-time object that represents boolean query conditions, joins, filters, `HAVING` clauses, and existence checks.

## placeholder

A marker type supplied through `P<Placeholder<T>>` or related helper machinery so parameter values can be supplied later at execution time.

## select_query

The query object type used for SELECT pipelines.

## insert_query

The query object type used for INSERT pipelines.

## connector

A backend object that accepts a query object and parameters and turns them into a backend-specific execution result.

## MockDB

The example connector in this repository that serializes query objects into SQL strings.

## orm_tuple

The tuple-like container used internally to accumulate query parts and template metadata.

## Related Notes

- [[00_Home|Home]]
- [[01_Getting_Started/00_Getting_Started|Getting Started]]
- [[02_Query_Flow/00_Query_Flow|Query Flow]]
- [[04_Reference/00_Reference|Reference]]
