# Connector Architecture

## Responsibility

The connector subsystem is the execution endpoint for query objects. Its responsibility is to accept a typed compile-time query IR plus runtime values and decide what "execution" means for that backend — SQL text for MockDB, a prepared `sqlite3_stmt` for SQLite, a wire message for a future NoSQL backend.

## Core Pattern — `connector_trait<DB>`

Every backend is a **dumb tag type** paired with a `connector_trait<DB>` specialisation that holds all the logic:

```cpp
// Tag
struct SQLiteDB { sqlite3* handle{nullptr}; ... };

// Trait
template <>
struct connector_trait<SQLiteDB>
{
    using supports_joins        = void;
    using supports_transactions = void;
    using supports_aggregation  = void;

    template <typename Response, ...>
    static auto execute(SQLiteDB& db, select_query<Response,...> q)
        -> result<projected_type<Response>, Response>;
};
```

The ORM core (`orm::db<DB>`) calls only `connector_trait<DB>::execute(conn, query, params...)`.

## Capability Gating

Capabilities are declared as nested void type-aliases inside the trait:

| Alias | Feature gated |
|-------|---------------|
| `using supports_joins = void` | `.join()` clauses |
| `using supports_transactions = void` | future `db.transaction()` |
| `using supports_aggregation = void` | future `COUNT(*)` etc. |

Using a feature on a connector that lacks the alias triggers a `static_assert` at the call site — a compile-time, not runtime, error.

## Available Connectors

### MockDB

- **Header**: `ORM/db/connectors/MockDB/mock_db.hpp`
- **CMake**: `orm::mockdb`
- Renders the query IR into a SQL string stored in `MockDB::last_sql`.
- Runtime parameters collected into `MockDB::last_params` (stringified).
- Full support: SELECT, INSERT, UPDATE, DELETE, WHERE, JOIN, ORDER BY (col + direction), GROUP BY, LIMIT.

### SQLiteDB

- **Header**: `ORM/db/connectors/SQLite/sqlite_db.hpp`
- **CMake**: `orm::sqlite` (requires `SQLite3` via `find_package`)
- Executes real SQL against a `sqlite3*` handle via prepared statements.
- SELECT: builds `SELECT col,... FROM table WHERE ...`, hydrates `std::tuple<>` rows via `sqlite_detail::hydrate_row`.
- INSERT: `INSERT INTO table (cols) VALUES (?,...)` with `bind_params`.
- UPDATE: `UPDATE table SET col=? WHERE ...` with `bind_params`.
- DELETE: `DELETE FROM table WHERE ...` with `bind_params`.
- Runtime values bound at slot `1..N` via `sqlite3_bind_*` — never concatenated into SQL text.

## Data Flow

```
User code:  db.execute(query, 42, "alice")
               │
               ▼
orm::db<DB>::execute  →  connector_trait<DB>::execute(conn, q, 42, "alice")
                                   │
                         [build SQL at runtime from IR]
                                   │
                         [bind 42, "alice" to stmt slots 1, 2]
                                   │
                         [step/hydrate rows into vector<Row>]
                                   │
                         return result<Row, FieldTuple>
```

## Compile-Time Guarantees

- **Query family dispatch** — driven by marker base types (`select_query_tag`, `insert_query_tag`, …).
- **Column names** — resolved at compile time from `mem_ptr<Ptr>::column_name()` inside the render helpers.
- **Row type** — `projected_type<FieldTuple>` is a `std::tuple<CppType...>` fully known at the call site.
- **Field index lookup** — `get_field<&T::m>(row)` is a `consteval` search; wrong field = `static_assert`.
- **Capability check** — missing `supports_joins` = compile error before any runtime call.

## Dependencies

- `ORM/connector/trait.hpp` — `connector_trait<DB>` primary template + `is_connector` concept
- `ORM/connector/capabilities.hpp` — `has_capability<DB, Cap>` helper
- `ORM/connector/db.hpp` — `orm::db<DB>` user-facing handle
- `ORM/result/result.hpp` — `orm::result<Row, FieldTuple>`, `orm::optional_result<T>`
- `ORM/query/field.hpp` — `mem_ptr<>`, `projected_type<>`
- `ORM/entity/table.hpp` — `table_name<Entity>()`

## Related Notes

- [[04_Reference/mockdb|MockDB connector reference]]
- [[04_Reference/sqlite_db|SQLiteDB connector reference]]
- [[03_Architecture/05_CRUD_Builder_Architecture|CRUD Builder Architecture]]
- [[02_Query_Flow/06_Query_Execution_via_operator_shift|Query Execution via operator<<]]
