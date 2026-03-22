# Executing Queries

## Purpose

Explain how a built query object is turned into an executable operation.

## Key Idea

In the current implementation, execution starts at `operator<<` defined in `lib/include/ORM/CRUD/utils.hpp`.

`db << query` does not immediately run the query. Instead, it returns a callable object that:

- captures the connector object and the query object
- accepts runtime parameters later
- requires those parameters to match the query's expected placeholder/property tuple shape
- forwards the call to `db.execute(query, args...)`

That is why the example code looks like this:

- `(db << Utils<User>::insert_new_user)("Name")`
- `(db << Utils<User>::get_all_users_with_id_above)(5)`

## Example in This Project

`example/ORM.cpp` constructs `MockDB::MockDB db;` and then prints the result of multiple executed queries.

The execution shape is consistent across query families:

- build or reuse a query object
- bind it to a connector with `db << query`
- supply the deferred arguments
- receive the connector-specific result

For `MockDB`, that result is a SQL string.

## How to Use It

1. create a connector instance
2. choose a query object
3. call `db << query`
4. invoke the returned callable with the runtime values that fill placeholders or deferred inputs

If the query has no deferred arguments, the returned callable is invoked with no parameters.

> [!info]
> Parameter checking is not string-based. The current code uses the `params_match<Query, args...>` concept in `CRUD/utils.hpp` to require a compatible tuple of runtime arguments.

## Common Pitfalls

- `db << query` returns a callable. It is not the final result by itself.
- The runtime parameter list must match the query's expected placeholder/property shape.
- The meaning of “execution” depends on the connector. In `MockDB`, execution means SQL serialization, not network I/O.

## Related Notes

- [[04_Building_Queries|Building Queries]]
- [[06_End_to_End_Example|End-to-End Example]]
- [[02_Query_Flow/06_Query_Execution_via_operator_shift|Query Execution via operator<<]]
- [[02_Query_Flow/07_MockDB_SQL_Generation|MockDB SQL Generation]]
- [[04_Reference/crud_utils|CRUD utils]]
- [[04_Reference/mockdb|MockDB]]
