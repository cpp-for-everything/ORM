# Architecture

This section groups the ORM by responsibility instead of by reading order.

Use it when you already know the public API shape and want to understand which subsystem owns each part of the behavior.

> [!note]
> These notes explain responsibilities and dependencies. For step-by-step execution order, use [[02_Query_Flow/00_Query_Flow|Query Flow]].

## Start Here

- [[03_Architecture/01_Field_System|Field System]]
- [[03_Architecture/02_Table_Reflection|Table Reflection]]
- [[03_Architecture/03_Type_Wrapper_System|Type Wrapper System]]
- [[03_Architecture/04_Expression_System|Expression System]]
- [[03_Architecture/05_CRUD_Builder_Architecture|CRUD Builder Architecture]]
- [[03_Architecture/06_Connector_Architecture|Connector Architecture]]

## Subsystem Map

- **Field system**
  - schema declarations through `property` and `relationship`

- **Reflection layer**
  - tuple-based conversion and field indexing through `Table<T>` and PFR

- **Type wrapper system**
  - DB-oriented wrapper types such as `INTEGER<>`, `TEXT<>`, and `Nullable<T>`

- **Expression system**
  - `P<>`, `Statement`, `Rule`, boolean composition, and negation rewrites

- **CRUD builders**
  - query object composition for `select`, `insert`, `update`, and `deleteq`

- **Connector layer**
  - execution boundary and backend-specific SQL serialization through `MockDB`

## Recommended Contributor Reading Order

- [[01_Field_System|Field System]]
- [[02_Table_Reflection|Table Reflection]]
- [[04_Expression_System|Expression System]]
- [[05_CRUD_Builder_Architecture|CRUD Builder Architecture]]
- [[06_Connector_Architecture|Connector Architecture]]

## Related Notes

- [[00_Home|Home]]
- [[02_Query_Flow/00_Query_Flow|Query Flow]]
- [[04_Reference/00_Reference|Reference]]
