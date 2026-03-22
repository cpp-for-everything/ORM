# CRUD utils

## File Role

Provides shared query concepts, table-inference helpers, placeholder/parameter matching glue, and the `db << query` execution boundary.

## Key Types / Symbols

- `contains<...>`
- `unique_tables<...>`
- `unique_tables_t<...>`
- `second_properties<...>`
- `IQuery`
- `is_query`
- `params_match<Query, args...>`
- `operator<<(DB, Query)`
- `select_query_t`
- `insert_query_t`
- `update_query_t`
- `delete_query_t`
- `modes::select`
- `modes::join`
- `modes::order`

## Important Behaviors

- infers participating tables from member-pointer inputs
- derives compatibility checks between query metadata and runtime arguments
- returns a lambda execution boundary through `operator<<`
- marks query families with lightweight base classes for connector dispatch
- defines query-mode enums consumed by builders and connectors

## Called By / Used By

Used directly by:

- all CRUD query families
- connectors such as `MockDB`
- public execution syntax in `example/ORM.cpp`
- join/group/order helper types in other headers

## Source Files

- `lib/include/ORM/CRUD/utils.hpp`

## Related Notes

- [[03_Architecture/05_CRUD_Builder_Architecture|CRUD Builder Architecture]]
- [[02_Query_Flow/06_Query_Execution_via_operator_shift|Query Execution via operator<<]]
- [[04_Reference/select|select.hpp]]
- [[04_Reference/insert|insert.hpp]]
- [[04_Reference/mockdb|MockDB connector]]
