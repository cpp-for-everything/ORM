# CRUD Builder Architecture

## Responsibility

The CRUD-builder subsystem packages schema metadata, expressions, placeholders, and query-specific clauses into typed query objects.

Its main responsibility is to keep each query family structurally typed before execution.

## Main Types and Functions

Core query families:

- `CRUD::select_query<...>`
- `CRUD::insert_query<...>`
- `CRUD::update_query<...>`
- `CRUD::delete_query<...>`

Shared helpers and concepts:

- `CRUD::details::IQuery`
- `select_query_t`
- `insert_query_t`
- `update_query_t`
- `delete_query_t`
- `params_match<Query, args...>`
- `unique_tables<...>`
- `unique_tables_t<...>`
- `second_properties<...>`
- `get_properties<...>`
- `get_placeholders<...>`
- `get_placeholders_and_properties<...>`
- `orm_tuple<...>`

## Data Flow

1. top-level entry points such as `select`, `insert`, `update`, and `deleteq` create query-family objects
2. builder methods return new query objects with extended tuple state rather than mutating a single runtime builder
3. helper traits derive table sets, placeholders, and property lists from the stored tuple state
4. the final query object exposes enough metadata for `params_match` and connector dispatch

Each query family emphasizes a different slice of data:

- `select_query` stores selected properties, joins, filters, limits, grouping, and ordering
- `insert_query` stores signature, inferred tables, and deferred input shape
- `update_query` stores update statements plus filters, ordering, and limits
- `delete_query` stores table, filters, ordering, and limits

## Compile-Time Guarantees

The builder layer keeps several facts in the type system:

- query family identity
- stored tuple shapes for each clause category
- placeholder/property compatibility for runtime execution
- inferred table sets from member-pointer usage

The builders do **not** execute anything themselves. They prepare typed data for the execution boundary and connector layer.

## Dependencies

The builder system depends on:

- `ORM/details/orm_tuple.hpp`
- `ORM/details/result_type.hpp`
- `ORM/details/member_pointer.hpp`
- `ORM/rules.hpp`
- `ORM/join_rule.hpp`
- `ORM/limits.hpp`
- connector-facing utilities in `CRUD/utils.hpp`

## Related Notes

- [[02_Query_Flow/04_Select_Query_Construction|Select Query Construction]]
- [[02_Query_Flow/05_Insert_Query_Construction|Insert Query Construction]]
- [[02_Query_Flow/06_Query_Execution_via_operator_shift|Query Execution via operator<<]]
- [[04_Reference/select|select.hpp]]
- [[04_Reference/insert|insert.hpp]]
- [[04_Reference/crud_utils|CRUD utils]]
- [[04_Reference/join_rule|join_rule.hpp]]
