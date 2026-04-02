# Select Query Construction

## Where This Step Fits in the Flow

This stage turns selected properties and rule expressions into a structured SELECT query object.

It sits between expression creation and connector execution.

## Inputs

The main inputs are:

- the top-level `select(field<&T::m>, ...)` factory
- selected member pointers such as `field<&User::id>`
- optional join rules, where rules, limits, grouping, and ordering
- helper types such as `orm_tuple`, `projected_type`

## What Happens

`orm::select(field<&User::id>, ...)` creates a `select_query<Response, Joins, Wheres, Limits, Groups, Orders>` object.

That object stores six logical parts in its type parameters:

- selected properties (`Response`)
- joins (`Joins`)
- filters (`Wheres`)
- limits (`Limits`)
- groupings (`Groups`)
- ordering (`Orders`)

The builder methods do not mutate in place. Each chained call builds a new `select_query` type with updated tuple state:

- `.join<mode, Table>(...)`
- `.where(rule)`
- `.limit(pagification)`
- `.group_by<...>(...)`
- `.order_by<...>()`

All clause state is accumulated in `orm_tuple<...>` type lists. Because `orm_tuple::tuple_cat` uses index-sequence pack expansion (not `std::tuple_cat`), the entire builder chain is a constant expression and can be assigned to a `static constexpr` variable:

```cpp
using namespace orm::literals;
static constexpr auto get_active_users =
    orm::select(orm::field<&User::id>, orm::field<&User::name>)
        .where(orm::field<&User::score> > 0.0)
        .where(orm::field<&User::id> == orm::Placeholder<int>{})
        .limit(10_per_page & 1_page);
```

The `&` operator combines `_per_page` and `_page` UDL helpers in either order.

## Output / Next Stage

After this stage, the ORM has a fully typed select-query object that can be:

- declared `static constexpr` (all builder calls are constant expressions)
- passed to `db << query`
- dispatched to any `connector_trait<DB>::execute(...)` overload

## Relevant Files

- `lib/include/ORM/CRUD/select.hpp`
- `lib/src/ORM/CRUD/select.cpp`
- `lib/include/ORM/CRUD/utils.hpp`
- `lib/include/ORM/details/result_type.hpp`
- `lib/include/ORM/details/orm_tuple.hpp`
- `lib/include/ORM/join_rule.hpp`
- `example/users/users.hpp`

## Related Notes

- [[02_Query_Flow/03_Rules_and_Compile_Time_Expressions|Rules and Compile-Time Expressions]]
- [[02_Query_Flow/06_Query_Execution_via_operator_shift|Query Execution via operator<<]]
- [[02_Query_Flow/07_MockDB_SQL_Generation|MockDB SQL Generation]]
- [[03_Architecture/05_CRUD_Builder_Architecture|CRUD Builder Architecture]]
- [[04_Reference/select|select_query]]
