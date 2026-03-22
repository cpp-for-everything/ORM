# Indexed Placeholder Design

**Date:** 2026-03-28  
**Status:** Approved

## Problem

`Placeholder<T>` is anonymous and positional. Slots are consumed left-to-right as `execute()` arguments. There is no way to reference the same runtime argument more than once in a query.

## Goal

Allow a runtime argument to be bound to multiple WHERE / SET slots by naming it with a `std::placeholders::_N` index.

## Decision: Approach B — `std::is_placeholder<>` detection

`Placeholder<T>` gains an optional second template parameter `auto N` constrained so that `N` must be one of the standard placeholder objects (`std::placeholders::_1` … `_9` etc.).

```cpp
template <typename T, auto N = std::placeholders::_1>
    requires (std::is_placeholder<decltype(N)>::value > 0)
struct Placeholder { using value_type = T; };
```

The 1-based index is recovered at compile time via `std::is_placeholder<decltype(N)>::value`.

## Shortened syntax: `orm::ph<T, N>`

A variable template provides the short form:

```cpp
template <typename T, auto N>
    requires (std::is_placeholder<decltype(N)>::value > 0)
inline constexpr Placeholder<T, N> ph{};
```

**Usage:**

```cpp
using namespace std::placeholders;

constexpr auto q = orm::select(orm::field<&User::id>)
    .where( (orm::field<&User::id>   == orm::ph<int, _1>)
         && (orm::field<&User::score> > orm::ph<double, _2>)
         && (orm::field<&User::name>  == orm::ph<std::u8string, _1>) ); // _1 reused

db.execute(q, 42, 9.5);   // _1 → 42, _2 → 9.5
```

## Mixing rule

Mixing indexed (`Placeholder<T, _N>`) and anonymous (`Placeholder<T>` — no second arg) in the same query is **forbidden**. A `static_assert` in the connector fires at the call site if the mix is detected. This avoids ambiguity in slot ordering.

## `is_placeholder_v` update

`is_placeholder_trait` is extended to match both forms:

```cpp
template <typename T>
struct is_placeholder_trait<Placeholder<T>> : std::true_type {};

template <typename T, auto N>
struct is_placeholder_trait<Placeholder<T, N>> : std::true_type {};
```

A new helper extracts the 1-based index:

```cpp
template <typename T>            // anonymous → index 0
struct placeholder_index : std::integral_constant<int, 0> {};

template <typename T, auto N>
struct placeholder_index<Placeholder<T, N>>
    : std::integral_constant<int, std::is_placeholder<decltype(N)>::value> {};
```

## Connector changes

### MockDB

`render_operand` already emits `"?"` for any placeholder — no SQL change needed. `collect_params` changes: instead of left-to-right packing, it builds a map `{index → stringified_value}` and resolves each placeholder's slot from that map.

### SQLiteDB

`bind_params` changes similarly: instead of always binding at `slot++`, it uses the placeholder index to determine the SQLite slot number (`sqlite3_bind_*` at the correct 1-based position). Re-used indices bind the same value to multiple slots.

## Backward compatibility

`Placeholder<T>{}` with no second arg remains valid. Any query using only anonymous placeholders continues to work exactly as before (left-to-right binding). The mixing restriction only fires if both forms appear together.

## Files changed

| File | Change |
|------|--------|
| `lib/include/ORM/query/placeholders.hpp` | Add `auto N` param + `ph<>` + `placeholder_index<>` |
| `lib/include/ORM/db/connectors/MockDB/mock_db.hpp` | `collect_params` respects index |
| `lib/include/ORM/db/connectors/SQLite/sqlite_db.hpp` | `bind_params` respects index |
| `tests/integration/test_mockdb.cpp` | New tests for indexed placeholder reuse |
| `tests/integration/test_sqlite.cpp` | New tests for indexed placeholder reuse |

## Out of scope

- Mixing indexed + anonymous in the same query (forbidden, `static_assert`)
- Indices beyond `_9` (users write `Placeholder<T, std::placeholders::_10>` directly)
- Named (string) placeholders
