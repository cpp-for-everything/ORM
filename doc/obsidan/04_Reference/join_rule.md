# join_rule.hpp

## File Role

Defines wrapper types that extend ordinary rules with join, grouping, and ordering metadata.

## Key Types / Symbols

- `JoinRule<_mode, _Table, rule_t>`
- `GroupByRule<_Properties, rule_t>`
- `GroupByRule<_Properties, nullptr_t>`
- `OrderBy<P, way>`

## Important Behaviors

- attaches join mode and joined table identity to a rule
- attaches grouped properties and optional `HAVING` rule state to a grouping helper
- represents order-by clauses as compile-time property/sort pairs
- allows grouped rules to expose whether they actually contain a rule through `has_rule()`

## Called By / Used By

Used directly by:

- `select_query::join(...)`
- `select_query::group_by<...>(...)`
- `select_query::order_by<...>()`
- `MockDB::execute_select_impl(...)`

## Source Files

- `lib/include/ORM/join_rule.hpp`

## Related Notes

- [[03_Architecture/05_CRUD_Builder_Architecture|CRUD Builder Architecture]]
- [[02_Query_Flow/04_Select_Query_Construction|Select Query Construction]]
- [[02_Query_Flow/07_MockDB_SQL_Generation|MockDB SQL Generation]]
- [[04_Reference/select|select.hpp]]
- [[04_Reference/mockdb|MockDB connector]]
