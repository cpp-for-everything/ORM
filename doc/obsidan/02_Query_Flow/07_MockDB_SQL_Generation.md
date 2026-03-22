# MockDB SQL Generation

## Where This Step Fits in the Flow

This is the final visible stage of the example execution pipeline.

After `operator<<` forwards a query and its runtime parameters, `MockDB` inspects the query family and serializes SQL text.

## Inputs

The main inputs are:

- a query object derived from one of the CRUD query marker types
- a runtime argument pack
- helper functions for placeholder counting and rule printing
- query metadata such as selected properties, joins, filters, tables, grouping, ordering, and limits

## What Happens

`MockDB::MockDB::execute(...)` dispatches by query family:

- `select_query_t` -> `execute_select(...)`
- `insert_query_t` -> `execute_insert(...)`
- `update_query_t` -> `execute_update(...)`
- `delete_query_t` -> `execute_delete(...)`

A few internal helpers matter a lot:

- `injection_prevent(...)`
  - formats integers, strings, null, and member pointers into SQL-friendly fragments

- `prepare_rule<...>::run()`
  - counts placeholders while descending through nested `Rule` trees

- `print_rule(...)`
  - recursively prints a `Rule`, substituting runtime values when placeholders are encountered

For SELECT queries, `execute_select_impl(...)` emits:

- selected property list
- join clauses
- where clauses
- limits
- group-by clauses and optional `HAVING`
- ordering

For INSERT queries, `execute_insert_impl(...)` walks the query `signature` and runtime arguments together to form `VALUES (...)` groups.

For UPDATE and DELETE, dedicated helpers reuse the same rule-printing and ordering/limit logic.

> [!warning]
> The current SQL text is a debugging and demonstration output path. For example, limits are serialized as `LIMIT <elements> x <page>`, which reflects the current implementation rather than canonical SQL syntax.

## Output / Next Stage

For `MockDB`, the output is the end of the pipeline: a generated SQL string.

Other connectors could reuse the same query objects but implement different `execute(...)` behavior.

## Relevant Files

- `lib/include/ORM/db/connectors/MockDB/init.hpp`
- `lib/include/ORM/CRUD/utils.hpp`
- `lib/include/ORM/rules.hpp`
- `lib/src/ORM/rules.cpp`
- `lib/include/ORM/join_rule.hpp`
- `example/ORM.cpp`

## Related Notes

- [[02_Query_Flow/06_Query_Execution_via_operator_shift|Query Execution via operator<<]]
- [[03_Architecture/06_Connector_Architecture|Connector Architecture]]
- [[04_Reference/mockdb|MockDB]]
- [[04_Reference/rules|Rule]]
- [[04_Reference/join_rule|join_rule.hpp]]
