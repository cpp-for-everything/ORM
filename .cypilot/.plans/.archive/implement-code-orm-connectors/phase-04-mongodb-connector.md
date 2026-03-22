```toml
[phase]
plan = "implement-code-orm-connectors"
number = 4
total = 12
type = "implement"
title = "MongoDB Connector — connector_trait<MongoDB> + tests"
depends_on = [1]
input_files = [
  "architecture/features/FEATURE-mongodb-connector.md",
  "lib/include/ORM/connector/capabilities.hpp",
  "lib/include/ORM/connector/trait.hpp",
  "lib/include/ORM/db/connectors/MongoDB/mongodb_db.hpp",
]
output_files = [
  "lib/include/ORM/db/connectors/MongoDB/mongodb_db.hpp",
  "tests/unit/test_mongodb_connector.cpp",
]
outputs = ["out/phase-04-mongodb-summary.md"]
inputs = ["out/phase-01-scaffolding-summary.md"]
```

## Preamble

This is a self-contained phase file. All rules, constraints, and kit content
are included below. Project files listed in the Task section must be read
at runtime. Follow the instructions exactly, run any EXECUTE commands as
written, and report results against the acceptance criteria at the end.

## What

Implement `connector_trait<MongoDB>` as a header-only C++23 specialisation. No SQL strings —
the connector translates the ORM WHERE predicate IR to a BSON-like filter representation
(simulated in-memory as a `std::string` JSON-like document for testability). Projection
rendering, result hydration, and ObjectID type mapping are also implemented. A `MockCollection`
struct stubs the libmongoc surface. `supports_joins` MUST NOT be declared.

## Prior Context

- Phase 1 created the stub header.
- MongoDB uses BSON documents, not SQL. IR → filter doc with `$and`/`$or`/`$eq`/`$gt`/`$lt` operators.
- BSON projection doc: `{ "col1": 1, "col2": 1, "_id": 0 }` when `_id` not in select list.
- ObjectID type: `bson_oid_t` — mapped to a 12-byte hex string in the mock.
- libmongoc API: `mongoc_collection_find_with_opts` → `mongoc_cursor_next` → `mongoc_cursor_destroy`.
- Capability tags applicable: NONE of `supports_joins`, `supports_transactions`, `supports_aggregation` (MongoDB does not support cross-collection joins or SQL transactions at this connector layer).
- Traceability: DOCS-ONLY.

## User Decisions

### Already Decided (pre-resolved during planning)
- **Traceability mode**: DOCS-ONLY
- **Mock strategy**: inline `MockCollection` and `MockCursor` structs; BSON rendered as `std::string` JSON-like doc
- **ObjectID mock**: use `std::string` with 24-char hex as the mock `bson_oid_t` equivalent

## Rules

### Structural
- MUST: `connector_trait<MongoDB>` MUST satisfy `is_connector<MongoDB>` (wire_type + cursor_type)
- MUST NOT: declare `using supports_joins`, `using supports_transactions`, or `using supports_aggregation`
- MUST: Implement BSON filter rendering: AND → `{"$and":[...]}`, OR → `{"$or":[...]}`, EQ → `{"field":{"$eq":val}}`, GT → `{"field":{"$gt":val}}`, LT → `{"field":{"$lt":val}}`
- MUST: Implement projection rendering: `{"col1":1,"col2":1}` + `"_id":0` suppression when `_id` not in select list
- MUST: Implement result hydration: `mongoc_cursor_next` iteration, field lookup, type mapping to C++ entity
- MUST: ObjectID type mapping: `bson_oid_t` ↔ entity ObjectID field (mock: `std::string` 24-char hex)
- MUST: `mongoc_cursor_destroy` called exactly once per cursor (RAII)
- MUST: Code implements all DoD items from FEATURE-mongodb-connector.md §5
- MUST NOT: leave TODO/TBD/FIXME

### Engineering
- MUST: TDD — tests written alongside implementation
- MUST: RAII for `MockCursor` — destructor increments `cursor_destroy_count`
- MUST: Error handling — BSON construction failure returns `orm::result` error state
- MUST NOT: use raw new/delete

### Quality
- MUST: Tests cover: (a) `is_connector<MongoDB>`; (b) AND/OR/EQ/GT/LT filter rendering; (c) projection with `_id` suppression; (d) ObjectID round-trip; (e) `cursor_destroy_count == 1`

## Input

### DoD items from FEATURE-mongodb-connector.md §5:

**DoD: connector_trait<MongoDB> Specialisation** — C++23, satisfies ConnectorTrait.
Implements: `cpt-orm-flow-mongodb-connector-execute-select`, `cpt-orm-algo-mongodb-connector-render-bson`

**DoD: BSON Filter Rendering** — AND/OR/EQ/GT/LT predicate nodes → BSON operator equivalents; runtime values bound at leaf nodes.

**DoD: BSON Projection Rendering** — emit projection doc with exactly the columns in select list; exclude `_id` when not mapped.

**DoD: ObjectID Type Mapping** — `bson_oid_t` ↔ entity ObjectID field during hydration and filter construction.

**DoD: Capability Tags** — tags not applicable to MongoDB MUST NOT be declared.

### TDD acceptance criteria from FEATURE §6:
- Compiles C++23 `-Wall -Wextra`.
- Single equality WHERE → `{ "field": { "$eq": value } }` (verified via `bson_as_canonical_extended_json` equivalent — `last_filter` string).
- AND of two predicates → `$and` array with two child docs.
- Two-column SELECT → projection `{ "col1": 1, "col2": 1, "_id": 0 }`.
- `bson_oid_t` field correctly hydrated into entity ObjectID.
- `mongoc_cursor_destroy` called exactly once.

## Task

1. **Read input files** — Read `architecture/features/FEATURE-mongodb-connector.md` (full), `lib/include/ORM/connector/capabilities.hpp`, `lib/include/ORM/connector/trait.hpp`, existing stub `lib/include/ORM/db/connectors/MongoDB/mongodb_db.hpp`.

2. **Implement `mongodb_db.hpp`**:
   - Define `struct MockCursor` with fields: `mutable int cursor_destroy_count = 0`. Stub `next_doc()` returning false. Destructor increments `cursor_destroy_count`.
   - Define `struct MockCollection` with `last_filter`, `last_projection` as `std::string`; `find_with_opts(filter, projection)` stores args and returns `MockCursor`.
   - Define `struct MongoDB` tag type.
   - Implement `connector_trait<MongoDB>` in `namespace orm {}`:
     - `wire_type`; `cursor_type`; NO capability tag aliases.
     - `namespace mongo_detail {}` with:
       - `render_filter(rule_tree)` — recursively emit `$and`/`$or`/`$eq`/`$gt`/`$lt` JSON-like string
       - `render_projection(columns)` — emit `{"col1":1,...,"_id":0}` when `_id` absent
       - `hydrate_result(cursor, column_list)` — iterate cursor, map fields to entity
     - `execute()` overloads for SELECT (no params), SELECT (with params) — store filter+projection in `conn`, return `orm::result<>{}`

3. **Write unit tests** — Create `tests/unit/test_mongodb_connector.cpp`:
   - `TEST(MongoConnector, SatisfiesIsConnector)` — `static_assert`
   - `TEST(MongoConnector, EqFilterRendering)` — single EQ predicate → `last_filter` contains `"$eq"`
   - `TEST(MongoConnector, AndFilterRendering)` — AND of two predicates → `last_filter` contains `"$and"`
   - `TEST(MongoConnector, ProjectionSuppressId)` — two-column SELECT → `last_projection` contains `"_id":0`
   - `TEST(MongoConnector, CursorDestroyCalledOnce)` — execute, destroy cursor → `cursor_destroy_count == 1`
   - `TEST(MongoConnector, NoJoinsCapability)` — `static_assert(!orm::has_capability<orm::MongoDB, orm::cap::supports_joins>)`

4. **Update `tests/unit/CMakeLists.txt`** — Add `test_mongodb_connector.cpp`.

5. **Write `out/phase-04-mongodb-summary.md`**.

6. **Self-verify**.

## Acceptance Criteria

- [ ] `mongodb_db.hpp` contains full `connector_trait<MongoDB>` with BSON filter + projection rendering
- [ ] `is_connector<MongoDB>` satisfied
- [ ] `supports_joins` NOT declared (verified by `has_capability` static_assert test)
- [ ] EQ filter rendering test present
- [ ] AND filter rendering test present
- [ ] Projection `_id` suppression test present
- [ ] Cursor RAII `cursor_destroy_count` test present
- [ ] `test_mongodb_connector.cpp` with ≥ 5 TEST() cases; CMakeLists updated
- [ ] No TODO/TBD/FIXME; no unresolved `{variable}` outside code fences
- [ ] `out/phase-04-mongodb-summary.md` exists

## Output Format

```text
PHASE 4/12 COMPLETE
Status: PASS | FAIL
Files created/modified: {list}
Acceptance criteria: {checklist}
Notes: {any issues}
```

```text
I have a Cypilot execution plan at:
  .cypilot/.plans/implement-code-orm-connectors/plan.toml

Phase 4 is complete (PASS).
Please read the plan manifest, then execute Phase 5: "Redis Connector — connector_trait<RedisDB> + tests".
The phase file is: .cypilot/.plans/implement-code-orm-connectors/phase-05-redis-connector.md
It is self-contained — follow its instructions exactly.
After completion, report results and generate the prompt for the next phase.
```
