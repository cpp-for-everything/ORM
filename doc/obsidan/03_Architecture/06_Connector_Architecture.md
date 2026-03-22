# Connector Architecture

## Responsibility

The connector subsystem is the execution endpoint for query objects.

Its responsibility is to accept a typed query plus runtime values and decide what “execution” means for that backend.

In this repository, the concrete example backend is `MockDB`.

## Main Types and Functions

- `details::IDB`
- `Connector<DB_type, CPP_type>`
- `CRUD::details::operator<<(DB, Query)`
- `MockDB::MockDB`
- `MockDB::MockDB::execute(...)`
- `execute_select(...)`
- `execute_insert(...)`
- `execute_update(...)`
- `execute_delete(...)`

Important connector-side helpers in `MockDB` include:

- `injection_prevent(...)`
- `prepare_rule<...>::run()`
- `print_rule(...)`
- `execute_select_impl(...)`
- `execute_insert_impl(...)`
- `execute_update_impl(...)`
- `execute_delete_impl(...)`

## Data Flow

1. a typed query object reaches the execution boundary through `db << query`
2. the returned lambda checks parameter compatibility through `params_match`
3. the connector receives `execute(query, args...)`
4. `MockDB` dispatches by query marker base type
5. helper routines serialize rules, placeholders, ordering, grouping, and limits into SQL text

## Compile-Time Guarantees

The connector layer still benefits from compile-time structure:

- query-family dispatch is driven by query marker inheritance
- member-pointer metadata is available during serialization
- rule-tree structure is known through the query types
- runtime parameter compatibility is constrained before `execute(...)` is called

The actual SQL string construction is still runtime work, especially for streaming values into `std::stringstream`.

## Dependencies

The connector layer depends on:

- `ORM/db/connectors/interface.hpp`
- `ORM/CRUD/utils.hpp`
- `ORM/rules.hpp`
- `ORM/join_rule.hpp`
- `ORM/details/member_pointer.hpp`
- `ORM/details/orm_tuple.hpp`

## Related Notes

- [[02_Query_Flow/06_Query_Execution_via_operator_shift|Query Execution via operator<<]]
- [[02_Query_Flow/07_MockDB_SQL_Generation|MockDB SQL Generation]]
- [[03_Architecture/05_CRUD_Builder_Architecture|CRUD Builder Architecture]]
- [[04_Reference/mockdb|MockDB connector]]
- [[04_Reference/crud_utils|CRUD utils]]
