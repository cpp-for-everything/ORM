# MockDB Connector

## File Role

In-memory SQL renderer used for unit and integration testing. Serialises the compile-time query IR into a SQL string without touching any database, making SQL generation correctness fully testable without a running server.

## Key Types / Symbols

### `orm::MockDB`

```cpp
struct MockDB
{
    mutable std::string              last_sql;
    mutable std::vector<std::string> last_params;
};
```

After every `db << q` or `db.execute(q, args...)` call, `last_sql` holds the rendered SQL and `last_params` holds stringified runtime arguments (in binding order).

### `orm::connector_trait<MockDB>`

Specialisation declaring:

```cpp
using supports_joins        = void;
using supports_transactions = void;
using supports_aggregation  = void;
using supports_upsert       = void;
```

### `execute` overloads

| Query type | Overloads | Effect |
|-----------|-----------|--------|
| `select_query` | 2 (with / without runtime params) | Writes `last_sql`; params go to `last_params` |
| `insert_query` | 1 | Writes `last_sql` |
| `update_query` | 1 | Writes `last_sql` |
| `delete_query` | 1 | Writes `last_sql` |

### `mockdb` namespace render helpers

| Helper | Output example |
|--------|---------------|
| `render_columns(fields)` | `"id, name, price"` |
| `render_joins(joins)` | `" JOIN posts ON ..."` |
| `render_wheres(wheres)` | `" WHERE id == ?"` |
| `render_group_by(groups)` | `" GROUP BY id"` |
| `render_order_by(orders)` | `" ORDER BY score DESC, id ASC"` |
| `render_limits(limits)` | `" LIMIT 10 OFFSET 0"` |
| `render_set_stmts(stmts)` | `"name = ?, score = ?"` |
| `stringify_param(v)` | Converts arithmetic/string/u8string/char* to `std::string` |
| `collect_params(args...)` | Returns `std::vector<std::string>` of stringified params |

## Important Behaviours

- All rendering is **compile-time dispatch** over the query IR types; no runtime reflection.
- `render_order_by` emits actual column names and `ASC`/`DESC` direction (not placeholders).
- `render_group_by` emits actual column names from `GroupBy<MemberPtr>::member`.
- Runtime params are stringified via `stringify_param<T>`, handling arithmetic, `std::string`, `std::u8string`, and `char8_t*` literals.
- Returns empty `result<Row, FieldTuple>{}` for SELECT — no actual rows; intended only for SQL inspection.

## CMake Target

`orm::mockdb` — always built as part of the library.

## Source File

- `lib/include/ORM/db/connectors/MockDB/mock_db.hpp`

## Related Notes

- [[03_Architecture/06_Connector_Architecture|Connector Architecture]]
- [[04_Reference/sqlite_db|SQLiteDB connector]]
- [[04_Reference/fields|field and mem_ptr]]
- [[04_Reference/rules|Rule expressions]]
