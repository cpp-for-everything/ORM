# Compilation Brief: Phase 5/12 — Redis Connector

--- CONTEXT BOUNDARY ---
Disregard all previous context. This brief is self-contained.
Read ONLY the files listed below. Follow the instructions exactly.
---

## Phase Metadata
```toml
[phase]
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

## Load Instructions

1. **Codebase rules**: Read `.cypilot/config/kits/sdlc/codebase/rules.md` (lines 60–260, ~200 lines)
   - Inline → Rules section; skip Traceability (DOCS-ONLY), Checkbox Cascade

2. **FEATURE spec**: Read `architecture/features/FEATURE-redis-connector.md` (whole file, ~280 lines)
   - Runtime read → Task step 1 (extract key-mapping algo, partial IR translation algo, client-side materialisation algo, DoD)

3. **capabilities.hpp**: Read `lib/include/ORM/connector/capabilities.hpp` (whole file, ~67 lines)
   - Runtime read → Task step 1

4. **Existing stub**: Read `lib/include/ORM/db/connectors/RedisDB/redis_db.hpp` (whole file)
   - Runtime read → Task step 2

**Do NOT load**: other connector headers, other FEATURE specs, checklist.md.

## Compile Phase File
Write to: `.cypilot/.plans/implement-code-orm-connectors/phase-05-redis-connector.md`

Key differentiators:
- Key-value store; no SQL. Entity type prefix + primary key → Redis key string.
- Single-column → Redis string (SET/GET); multi-column → Redis Hash (HSET/HGETALL).
- Compile-time WHERE enforcement: `static_assert` when WHERE predicate is not primary-key equality.
- `supports_joins`, `supports_transactions`, `supports_aggregation` MUST NOT be declared.
- Mock hiredis surface inline (MockRedisContext struct, stub redisCommand).
- Client-side materialisation (SCAN+sort) is a separate algorithm method.

## Context Budget
- Phase file target: ≤ 600 lines
- Total execution context: phase (~600) + FEATURE (~280) + capabilities (~67) = ~947 lines — within budget
