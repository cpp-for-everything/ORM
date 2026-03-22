# Query Execution via operator<<

## Where This Step Fits in the Flow

This is the handoff point between query construction and connector-specific execution.

Once a query object exists, `operator<<` turns it into a callable operation bound to a specific connector instance.

## Inputs

The main inputs are:

- a connector object such as `MockDB::MockDB`
- a query object derived from `CRUD::details::IQuery`
- runtime arguments that must match the query's expected `properties` tuple

## What Happens

`operator<<` lives in `lib/include/ORM/CRUD/utils.hpp`.

Its behavior is:

1. accept a `DB` object and a query object
2. return a lambda that captures both
3. constrain the lambda with `params_match<QueryType, decltype(args)...>`
4. call `db.execute(query, args...)`
5. either return the connector result or return `void`, depending on the connector implementation

This gives the project a uniform execution shape across query families.

For example:

- `(db << Utils<User>::insert_new_user)("Name")`
- `(db << Utils<User>::get_all_users_with_id_above)(5)`
- `(db << Utils<UserPost>::get_all_posts_with_their_assosiated_users)()`

`params_match` is based on `convertible_tuples<...>` and `orm_tuple<...>`, so the runtime argument check is derived from query metadata rather than from a handwritten function signature.

> [!info]
> The returned object is a lambda, not a special query-runner class. The execution boundary stays lightweight and generic.

## Output / Next Stage

After this stage, control passes to the selected connector.

For the built-in example backend, the next stage is `MockDB::MockDB::execute(...)`, which dispatches by query family and builds SQL text.

## Relevant Files

- `lib/include/ORM/CRUD/utils.hpp`
- `lib/include/ORM/details/orm_tuple.hpp`
- `lib/include/ORM/db/connectors/interface.hpp`
- `example/ORM.cpp`

## Related Notes

- [[02_Query_Flow/04_Select_Query_Construction|Select Query Construction]]
- [[02_Query_Flow/05_Insert_Query_Construction|Insert Query Construction]]
- [[02_Query_Flow/07_MockDB_SQL_Generation|MockDB SQL Generation]]
- [[03_Architecture/06_Connector_Architecture|Connector Architecture]]
- [[04_Reference/crud_utils|CRUD utils]]
- [[04_Reference/mockdb|MockDB]]
