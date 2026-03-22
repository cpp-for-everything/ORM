# select.hpp

## File Role

Defines the SELECT query object family and the fluent builder interface for joins, filters, limits, grouping, and ordering.

## Key Types / Symbols

- `CRUD::select_query<...>`
- `mode`
- `properties`
- `Response`
- `Joins`
- `Wheres`
- `Limitations`
- `Groupings`
- `OrderBy`
- `join(...)`
- `where(...)`
- `limit(...)`
- `group_by<...>(...)`
- `order_by<...>()`
- top-level `select<modes::..., ...>`

## Important Behaviors

- stores selected properties and all clause categories as typed tuple state
- returns new query objects on each chained builder call
- derives the runtime parameter shape from placeholders found in joins, filters, and limits
- uses `result_t<...>` to build the selected-response tuple shape

## Called By / Used By

Used directly by:

- query declarations in `example/users/users.hpp`
- connector dispatch through `operator<<` and `MockDB::execute_select(...)`

Depends on:

- `CRUD/utils.hpp`
- `join_rule.hpp`
- `result_type.hpp`
- `orm_tuple.hpp`

## Source Files

- `lib/include/ORM/CRUD/select.hpp`
- `lib/src/ORM/CRUD/select.cpp`

## Related Notes

- [[03_Architecture/05_CRUD_Builder_Architecture|CRUD Builder Architecture]]
- [[02_Query_Flow/04_Select_Query_Construction|Select Query Construction]]
- [[04_Reference/crud_utils|CRUD utils]]
- [[04_Reference/join_rule|join_rule.hpp]]
- [[04_Reference/mockdb|MockDB connector]]
