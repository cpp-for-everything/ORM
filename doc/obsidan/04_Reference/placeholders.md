# Placeholders

## File Role

Defines the runtime parameter slot types used in query WHERE, SET, and JOIN expressions. Placeholders mark positions in a compile-time query that will be filled with runtime values at `db.execute()` time.

## Key Types / Symbols

### `orm::Placeholder<T>`

Anonymous placeholder. Slots are consumed left-to-right as `db.execute()` arguments. Each occurrence receives the next positional argument.

```cpp
constexpr auto q = orm::select(orm::field<&User::id>)
    .where(orm::field<&User::id> == orm::Placeholder<int>{});

db.execute(q, 42);  // 42 bound to the single slot
```

### `orm::IndexedPlaceholder<T, N>`

Indexed placeholder. `N` must be one of the standard placeholder objects (`std::placeholders::_1` … `_9`). The 1-based slot index is extracted at compile time via `std::is_placeholder<decltype(N)>::value`.

The **same index can appear multiple times** in a query — both occurrences receive the same runtime argument.

### `orm::ph<T, N>` — short variable template

```cpp
template <typename T, auto N>
    requires (std::is_placeholder<decltype(N)>::value > 0)
inline constexpr IndexedPlaceholder<T, N> ph{};
```

**Usage:**

```cpp
using namespace std::placeholders;

constexpr auto q = orm::select(orm::field<&User::id>, orm::field<&User::score>)
    .where((orm::field<&User::id>   == orm::ph<int, _1>)
        && (orm::field<&User::score> > orm::ph<double, _2>)
        && (orm::field<&User::id>   == orm::ph<int, _1>));  // _1 reused

db.execute(q, 42, 9.5);  // _1 → 42, _2 → 9.5
```

## Concepts and Traits

| Trait / concept | Purpose |
|-----------------|---------|
| `is_placeholder_v<T>` | `true` for both `Placeholder<T>` and `IndexedPlaceholder<T,N>` |
| `is_placeholder<T>` | Concept equivalent of `is_placeholder_v<T>` |
| `placeholder_index_v<T>` | `0` for anonymous; `1`-based index for indexed |

## SQL Rendering

| Placeholder type | MockDB / SQLite SQL text |
|-----------------|--------------------------|
| `Placeholder<T>{}` | `?` (sequential) |
| `ph<T, _1>` | `?1` |
| `ph<T, _2>` | `?2` |

SQLite's `?NNN` parameter syntax means two `?1` slots in the same SQL string both receive the value bound to SQLite parameter 1 — no extra binding calls needed.

## Mixing Rule

Mixing anonymous (`Placeholder<T>{}`) and indexed (`ph<T, _N>`) placeholders in the **same query** is forbidden. A `static_assert` fires at the call site.

## Source File

- `lib/include/ORM/query/placeholders.hpp`

## Related Notes

- [[04_Reference/rules|Rule expressions]]
- [[04_Reference/mockdb|MockDB connector]]
- [[04_Reference/sqlite_db|SQLiteDB connector]]
- [[03_Architecture/06_Connector_Architecture|Connector Architecture]]
