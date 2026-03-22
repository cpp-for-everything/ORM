# SQLiteDB Connector

## File Role

Implements the SQLite3 connector backend. Executes real SQL against a `sqlite3*` handle via prepared statements, binding runtime values safely without string concatenation.

## Key Types / Symbols

### `orm::SQLiteDB`

```cpp
struct SQLiteDB
{
    sqlite3* handle{nullptr};

    [[nodiscard]] static SQLiteDB open(const char* path);
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;
};
```

Move-only RAII handle. `open(":memory:")` creates an in-memory database.

### `orm::connector_trait<SQLiteDB>`

Specialisation of the primary `connector_trait<>` template. Declares:

```cpp
using supports_joins        = void;
using supports_transactions = void;
using supports_aggregation  = void;
```

### `execute` overloads

| Query type | Overloads | Runtime params |
|-----------|-----------|----------------|
| `select_query` | 2 (with / without params) | Bound to WHERE `?` slots |
| `insert_query` | 1 | One value per column |
| `update_query` | 1 | SET values then WHERE values |
| `delete_query` | 1 | WHERE values |

### `sqlite_detail` namespace helpers

| Helper | Purpose |
|--------|---------|
| `bind_value(stmt, col, v)` | Binds one C++ value to a prepared statement slot |
| `read_column<T>(stmt, col)` | Reads one column from the current result row |
| `hydrate_row<Tuple>(stmt)` | Builds a `std::tuple<>` from all columns of a result row |
| `render_columns<OrmTuple>()` | Produces `"col1, col2, ..."` string from field tags |
| `entity_of_t<OrmTuple>` | Extracts the entity (table) type from the first `mem_ptr<>` |
| `bind_params(stmt, args...)` | Binds a variadic pack of runtime values (1-indexed) |
| `render_wheres_sqlite(w)` | Produces ` WHERE col = ?, ...` from a Wheres `orm_tuple` |
| `sql_op(op)` | Translates `==` → `=`, `!=` → `<>`, `&&` → `AND`, `||` → `OR` |

## Wire Type Mapping

| C++ type | SQLite API used |
|----------|----------------|
| `int` | `sqlite3_bind_int` / `sqlite3_column_int` |
| `std::int64_t` | `sqlite3_bind_int64` / `sqlite3_column_int64` |
| `double` | `sqlite3_bind_double` / `sqlite3_column_double` |
| `std::string` | `sqlite3_bind_text` (SQLITE_TRANSIENT) / `sqlite3_column_text` |
| `std::u8string` | same as `std::string` with `char8_t` cast |
| `std::nullptr_t` | `sqlite3_bind_null` |

## Important Behaviours

- Runtime values are **always** bound via `sqlite3_bind_*` — never interpolated into SQL strings.
- WHERE clause `==` is translated to SQL `=` at render time via `sql_op()`.
- Row hydration uses `std::index_sequence` to unpack all tuple elements in one expression.
- The entity (table) type is extracted from the first `mem_ptr<>` in the `Response` `orm_tuple` via `entity_of_t<>`.
- `table_name<Entity>()` provides the SQL table name (requires `table_name_trait<Entity>` specialisation).

## CMake Target

`orm::sqlite` — interface library, created only when `find_package(SQLite3)` succeeds. Links `SQLite3::SQLite3` and `orm::orm`.

## Source File

- `lib/include/ORM/db/connectors/SQLite/sqlite_db.hpp`

## Related Notes

- [[03_Architecture/06_Connector_Architecture|Connector Architecture]]
- [[04_Reference/mockdb|MockDB connector]]
- [[04_Reference/fields|field and mem_ptr]]
- [[04_Reference/table|table_name_trait]]
