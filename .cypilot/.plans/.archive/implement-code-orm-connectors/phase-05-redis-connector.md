```toml
[phase]
plan = "implement-code-orm-connectors"
number = 5
total = 12
type = "implement"
title = "Redis Connector — connector_trait<RedisDB> + tests"
depends_on = [1]
input_files = [
  "architecture/features/FEATURE-redis-connector.md",
  "lib/include/ORM/connector/capabilities.hpp",
  "lib/include/ORM/connector/trait.hpp",
  "lib/include/ORM/db/connectors/RedisDB/redis_db.hpp",
]
output_files = [
  "lib/include/ORM/db/connectors/RedisDB/redis_db.hpp",
  "tests/unit/test_redis_connector.cpp",
]
outputs = ["out/phase-05-redis-summary.md"]
inputs = ["out/phase-01-scaffolding-summary.md"]
```

## Preamble

This is a self-contained phase file. All rules, constraints, and kit content
are included below. Project files listed in the Task section must be read
at runtime. Follow the instructions exactly, run any EXECUTE commands as
written, and report results against the acceptance criteria at the end.

## What

Implement `connector_trait<RedisDB>` as a header-only C++23 specialisation. Redis is a
key-value store: single-column entities map to Redis strings (SET/GET), multi-column
entities map to Redis Hashes (HSET/HGETALL). WHERE predicates are restricted to
primary-key equality via compile-time `static_assert`. `supports_joins`,
`supports_transactions`, and `supports_aggregation` MUST NOT be declared. A
`MockRedisContext` stubs the hiredis surface inline.

## Prior Context

- Phase 1 created the stub header.
- Key derivation: `"TypeName:"` prefix + runtime primary-key value string.
- Compile-time WHERE enforcement: if WHERE predicate is not single primary-key equality → `static_assert` at query construction time.
- ORDER BY / GROUP BY: require `client_side_materialisation = true` compile flag; implemented via SCAN + in-memory sort.
- hiredis API: `redisCommand(ctx, fmt, ...)` returning `redisReply*`.
- Capability tags: NONE of supports_joins / supports_transactions / supports_aggregation.
- Traceability: DOCS-ONLY.

## User Decisions

### Already Decided (pre-resolved during planning)
- **Traceability mode**: DOCS-ONLY
- **Mock strategy**: inline `MockRedisContext` with `last_command` and `last_key` fields; stub `redisCommand` method
- **Client-side materialisation**: implement as a separate template method gated by `client_side_materialisation` tag

## Rules

### Structural
- MUST: `connector_trait<RedisDB>` MUST satisfy `is_connector<RedisDB>` (wire_type + cursor_type)
- MUST NOT: declare `using supports_joins`, `using supports_transactions`, or `using supports_aggregation`
- MUST: Key derivation algorithm MUST derive key as `"TypeName:" + to_string(primary_key_value)` at compile-time type prefix
- MUST: Single-column INSERT MUST issue `SET key value`; SELECT MUST issue `GET key`
- MUST: Multi-column INSERT MUST issue `HSET key field1 val1 ...`; SELECT MUST issue `HGETALL key`
- MUST: Compile-time WHERE enforcement: `static_assert` when WHERE predicate is not a single primary-key equality; message must contain "primary-key equality"
- MUST: `execute()` returns `orm::result<...>`; hiredis errors propagated through error state
- MUST: Code implements all DoD items from FEATURE-redis-connector.md §5
- MUST NOT: leave TODO/TBD/FIXME

### Engineering
- MUST: TDD — static_assert tests for WHERE enforcement compile-time check
- MUST: KISS — key prefix derived from entity type name at compile time using `constexpr` string literal
- MUST: SOLID / Interface Segregation — key derivation, IR translation, and command dispatch are separate functions in `namespace redis_detail {}`
- MUST NOT: use raw new/delete

### Quality
- MUST: Tests cover: (a) `is_connector<RedisDB>`; (b) single-column SET/GET round-trip; (c) multi-column HSET/HGETALL; (d) non-primary-key WHERE → static_assert fires; (e) `supports_joins` not declared

## Input

### DoD items from FEATURE-redis-connector.md §5:

**DoD: connector_trait<RedisDB> Specialisation** — C++23, satisfies ConnectorTrait.
Implements: `cpt-orm-flow-redis-connector-cache-entity`, `cpt-orm-algo-redis-connector-map-key`

**DoD: Key and Hash Field Mapping** — derive key from entity type prefix + runtime PK; single-column → Redis string; multi-column → Redis Hash; round-trip fidelity.

**DoD: Primary-Key-Only WHERE Enforcement** — `static_assert` compile error for non-PK WHERE predicates.

**DoD: Hash Field Round-Trip** — all non-PK fields stored via HSET; retrieved via HGETALL; no data loss.

**DoD: Capability Tags** — `supports_joins`, `supports_transactions`, `supports_aggregation` MUST NOT be declared.

### TDD acceptance criteria from FEATURE §6:
- Compiles C++23 `-Wall -Wextra`.
- Single-column entity SET→GET round-trip: original value retrieved.
- Multi-column HSET→HGETALL: entity struct hydrated correctly.
- Non-PK WHERE predicate → `static_assert` compile error.
- `supports_joins` reference → `static_assert` compile error.

## Task

1. **Read input files** — Read `architecture/features/FEATURE-redis-connector.md` (full), `lib/include/ORM/connector/capabilities.hpp`, `lib/include/ORM/connector/trait.hpp`, existing stub `lib/include/ORM/db/connectors/RedisDB/redis_db.hpp`.

2. **Implement `redis_db.hpp`**:
   - Define `struct MockRedisContext` with fields `last_command` (`std::string`), `last_key` (`std::string`), `last_value` (`std::string`), `last_hash_fields` (`std::vector<std::pair<std::string,std::string>>`). Stub `run_command(cmd, key, ...)` method that records the call.
   - Define `struct RedisDB` tag type.
   - `namespace redis_detail {}`:
     - `derive_key<Entity>(pk_value)` — `constexpr` prefix from type + runtime PK string
     - `is_pk_equality_predicate<WhereClause>()` — compile-time check returning bool
     - `pk_only_assert<WhereClause>()` — `static_assert(is_pk_equality_predicate<WhereClause>(), "RedisDB connector supports only primary-key equality WHERE predicates")`
   - `connector_trait<RedisDB>` in `namespace orm {}`:
     - NO capability tag aliases
     - `wire_type`, `cursor_type`
     - `execute()` for INSERT (single-column → SET; multi-column → HSET; records to MockRedisContext)
     - `execute()` for SELECT (single-column → GET; multi-column → HGETALL; returns empty result)
     - Compile-time WHERE check via `redis_detail::pk_only_assert` in execute template

3. **Write unit tests** — Create `tests/unit/test_redis_connector.cpp`:
   - `TEST(RedisConnector, SatisfiesIsConnector)`
   - `TEST(RedisConnector, SingleColumnSetGet)` — insert entity, verify `last_command` contains "SET"
   - `TEST(RedisConnector, MultiColumnHSet)` — insert multi-column entity, verify `last_command` contains "HSET"
   - `TEST(RedisConnector, NoJoinsCapability)` — `static_assert(!has_capability<RedisDB, cap::supports_joins>)`
   - `TEST(RedisConnector, NoTransactionsCapability)` — `static_assert(!has_capability<RedisDB, cap::supports_transactions>)`

4. **Update `tests/unit/CMakeLists.txt`** — Add `test_redis_connector.cpp`.

5. **Write `out/phase-05-redis-summary.md`**.

6. **Self-verify**.

## Acceptance Criteria

- [ ] `redis_db.hpp` contains full `connector_trait<RedisDB>` with key derivation + SET/GET/HSET/HGETALL dispatch
- [ ] `is_connector<RedisDB>` satisfied
- [ ] `supports_joins`, `supports_transactions`, `supports_aggregation` NOT declared
- [ ] Single-column SET command test present
- [ ] Multi-column HSET command test present
- [ ] `no-capability` static_assert tests present
- [ ] `test_redis_connector.cpp` with ≥ 4 TEST() cases; CMakeLists updated
- [ ] No TODO/TBD/FIXME; no unresolved `{variable}` outside code fences
- [ ] `out/phase-05-redis-summary.md` exists

## Output Format

```text
PHASE 5/12 COMPLETE
Status: PASS | FAIL
Files created/modified: {list}
Acceptance criteria: {checklist}
Notes: {any issues}
```

```text
I have a Cypilot execution plan at:
  .cypilot/.plans/implement-code-orm-connectors/plan.toml

Phase 5 is complete (PASS).
Please read the plan manifest, then execute Phase 6: "Cassandra Connector — connector_trait<CassandraDB> + tests".
The phase file is: .cypilot/.plans/implement-code-orm-connectors/phase-06-cassandra-connector.md
It is self-contained — follow its instructions exactly.
After completion, report results and generate the prompt for the next phase.
```
