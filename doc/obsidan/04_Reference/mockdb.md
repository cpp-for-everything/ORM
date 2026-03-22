# MockDB connector

## File Role

Implements the example connector backend that serializes typed query objects into SQL strings.

## Key Types / Symbols

- `MockDB::MockDB`
- `execute(QueryType, params...)`
- `execute_select(...)`
- `execute_insert(...)`
- `execute_update(...)`
- `execute_delete(...)`
- `execute_select_impl(...)`
- `execute_insert_impl(...)`
- `execute_update_impl(...)`
- `execute_delete_impl(...)`
- `prepare_rule<...>::run()`
- `print_rule(...)`
- `injection_prevent(...)`

## Important Behaviors

- dispatches by query-family marker inheritance
- recovers field and table names from member-pointer metadata
- substitutes runtime arguments into placeholder positions
- prints joins, filters, grouping, ordering, and limits into SQL-like text
- serializes INSERT values by walking the query signature and runtime argument tuple together

## Called By / Used By

Used directly by:

- the example program in `example/ORM.cpp`
- query execution through `operator<<`

Consumes:

- `Rule` trees
- CRUD query metadata
- member-pointer traits
- tuple-based parameter packs

## Source Files

- `lib/include/ORM/db/connectors/MockDB/init.hpp`

## Related Notes

- [[03_Architecture/06_Connector_Architecture|Connector Architecture]]
- [[02_Query_Flow/07_MockDB_SQL_Generation|MockDB SQL Generation]]
- [[02_Query_Flow/06_Query_Execution_via_operator_shift|Query Execution via operator<<]]
- [[04_Reference/crud_utils|CRUD utils]]
- [[04_Reference/rules|rules.hpp and rules.cpp]]
