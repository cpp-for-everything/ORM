# Security evaluation suite

The evidence behind [`SECURITY.md`](../SECURITY.md) and
[`docs/THREAT_MODEL.md`](../docs/THREAT_MODEL.md). Every claim those documents
make should trace to a test here, and every number to a file in `evidence/`.

## Running it

```sh
cmake -S . -B build-security -DORM_ENABLE_SECURITY=ON -DORM_ENABLE_SQLITE=ON
cmake --build build-security
ctest --test-dir build-security -L security --output-on-failure
```

The negative compilation tests need GCC or Clang. The runtime tests need
`ORM_ENABLE_SQLITE=ON`.

## What is in here

| Directory | What it proves |
|---|---|
| `compile_fail/` | Programs that must not compile, each rejected with a named diagnostic, plus three controls that must compile. Identifiers and query structure cannot be reached from input known only at run time, and a literal operand cannot reach execution. |
| `runtime/` | The documented placeholder path on SQLite, as unit tests, and the differential test over the corpus. |
| `corpus/` | The attack payloads, with a source for every one. |
| `probes/` | The literal operand defect, reproduced against the library before the fix. |
| `evidence/` | What the runs produced. Written into the source tree on purpose. |

## Why controls matter

A test that expects a program to fail to compile passes for any failure,
including a missing include. Two things stop that from happening here:

1. Each rejection must match a diagnostic pattern, so it fails for the reason
   under test.
2. Three controls must compile: the documented query, a `nullptr` comparison and
   a column to column comparison. They use the same fixture as the rejections,
   so a broken fixture fails the controls too.

## Reading the differential results

The naive path is deliberately vulnerable. It exists so that the ORM's result
means something: a payload that does nothing to either path proves nothing.
The test fails if the naive path is never compromised.

The oracle sees effects only. A blind payload that changes the query's logic but
still returns zero rows is not counted as a compromise, so the naive compromise
count is a lower bound. That bias works against the claim, not for it.

## Server backends

`runtime/differential_<backend>.cpp` repeats the differential for PostgreSQL,
MySQL (the MariaDB server of the compose file), MongoDB, Redis and Cassandra.
They are built when the matching `orm::<backend>_live` target exists and need a
running server, reached through the `ORM_<BACKEND>_*` variables that
`tests/integration` uses. CI runs them in the `test-all` image against the
compose services and uploads `evidence/` as the `security-evidence` artifact.

The naive path is the realistic vulnerable construction for each store:
concatenated SQL with stacked statements allowed (PostgreSQL, MySQL), concatenated
CQL sent unprepared (Cassandra), a JSON filter or `$where` expression built by
concatenation (MongoDB, five constructions, most severe outcome kept), and a
command sent in the inline protocol so that CR LF starts a second command (Redis).

The failure rule differs from SQLite on purpose. A server harness fails only if a
payload has an effect through the library. If the naive path is never compromised
the harness passes and the summary says the comparison is inconclusive for that
backend. Neo4j is not measured: the image has no `libneo4j-client` and the compose
file has no Neo4j service. See `evidence/differential_neo4j_summary.txt`.
