# Threat model

Scope: the query construction and execution path of this library, across all seven supported
backends. Companion to [`SECURITY.md`](../SECURITY.md), which states the policy. This document
states the reasoning and the evidence.

Every claim below cites the code it came from. Line numbers refer to commit
`a17f4e7`; the connector headers they point into have not changed since. Where something has not been verified yet, it says
so rather than assuming.

---

## 1. What is being protected

**Asset: the structure of the command sent to the data store.**

The value of a query is data. The structure is authority. An attacker who can change `WHERE
id = $1` into `WHERE id = $1 OR 1=1` has not changed data, they have changed what the database is
authorised to return. Every injection class is a variant of that single move.

This library's position is that structure is decided at compile time and therefore cannot be
reached at runtime at all.

---

## 2. Attacker model

Three levels, in increasing strength. The distinction matters because most discussion of injection
collapses them.

### Level 1. The attacker controls values

The ordinary case. A value arrives from a form, a header, a message, and ends up in a query.

**Eliminated.** Values enter only through typed positional placeholders (`orm::ph<T, _N>`) and are
passed to `prepared_query::execute(args...)`
([`connector/prepared_query.hpp`](../lib/include/ORM/connector/prepared_query.hpp)). The statement
and the values travel separately all the way to the driver. See section 3 for what each backend
does with them.

A value is also **typed**. `orm::ph<int, _1>` will not accept a string. A whole class of coercion
tricks is rejected by the compiler before it is a runtime question.

**A gap found and closed by this work.** The claim above was not fully true
until November 2026. The comparison operators accepted any value type, not only
placeholders, so `field<&T::name> == some_string` compiled and stored the value
in the IR. No connector bound it. Each rendered it as an unbound `?`
(`SQLite/sqlite_db.hpp:258`), and the driver read that as NULL.

Measured on SQLite before the fix (`security/probes`, `security/evidence/probe_*`):

| Query | Intent | Result |
|---|---|---|
| `SELECT` with a literal `banana` | find 1 row | 0 rows |
| `UPDATE ... SET` with a literal | rename one row | row unchanged |
| `DELETE ... WHERE` with a literal | delete one row | nothing deleted |

The `UPDATE` case is parameter confusion. Arguments bind positionally from slot
1 (`sqlite_db.hpp:214`), so the one argument meant for `WHERE id = ?` was bound
into the literal's `SET` slot.

This **failed closed**. The value never became SQL text, so it was not an
injection. It was a silent wrong result. The existing test suite never caught it
because literal operands appeared only in unit tests of the IR's shape
(`tests/unit/test_query.cpp`), never in a query that was executed.

It is now a compile error at every entry point that reaches a connector:
`db::operator<<`, `db::execute`, `db::find_one`, `db::prepare`,
`prepared_query::execute`, and the `async_db` equivalents
(`connector/param_check.hpp`, `check_query_shape`). Seven compile checks, one
per entry point and query kind, hold it there (`security/compile_fail/07` to
`13`).

### Level 2. The attacker controls identifiers

The attacker chooses a table name, a column name, or an ordering key. This is where parameter
binding normally fails, because no database lets you bind an identifier.

**Eliminated, by not being expressible.** Identifiers derive from `column_name()`, which is
`constexpr` and computed from a member pointer
([`details/member_pointer.hpp:44`](../lib/include/ORM/details/member_pointer.hpp),
[`entity/property.hpp:19`](../lib/include/ORM/entity/property.hpp)). There is no API taking a table
name, column name or query fragment as a runtime string. The attacker cannot reach the identifier
because the programmer cannot either.

**This is the most interesting claim in the model**, because it is the one normally conceded.
Libraries that bind values still interpolate identifiers, and that is where their injection bugs
live. Here the capability is absent rather than guarded.

It is stronger than "must be a constant". Because the column name is part of
the property's type, two columns of the same C++ type are still different types,
so one variable cannot range over two columns at all
(`security/compile_fail/17`).

The cost is real and should be stated plainly: genuinely dynamic schemas are not supported.

### Level 3. The attacker controls structure

The attacker chooses the shape of the query, for example a filter builder that assembles
predicates from a request.

**Eliminated, with the same cost.** The query is a C++ type. A `select(...).where(...)` expression
produces an IR whose shape is fixed at compile time. Shape cannot be assembled from runtime data
because there is no runtime representation of shape to assemble.

The entry points used to be unconstrained templates, so a `std::string` holding
query text got as far as connector overload resolution before it was rejected,
with an error about the connector. They now reject anything that is not one of
the four query kinds with `orm: this is not a query`
(`security/compile_fail/03`).

A program that needs this must enumerate the permitted shapes as distinct compile time queries and
select among them. That is more work, and it is also the only form in which the choice is
auditable.

---

## 3. How each backend enforces it

The guarantee is uniform. The mechanism is not. This matters: a single abstraction is being
defended across four different protocols, and the argument has to hold separately for each.

| Backend | Mechanism | Evidence |
|---|---|---|
| PostgreSQL | `PQexecParams`, parameter arrays | `PostgreSQLDB/postgresql_live.hpp:552,615` |
| MySQL | `MYSQL_BIND` prepared statements | `MySQLDB/mysql_live.hpp:316` `param_binder` |
| SQLite | `sqlite3_prepare_v2` and `sqlite3_bind_*` | `SQLite/sqlite_db.hpp:62,67,72` |
| Cassandra | `cass_statement_bind_*` | `CassandraDB/cassandra_live.hpp:200-221` |
| MongoDB | bsoncxx document builder, `append` | `MongoDB/mongodb_live.hpp:214-238` |
| Neo4j | Bolt parameters, `neo4j_map_entry` | `Neo4jDB/neo4j_live.hpp:174` |
| Redis | hiredis `%s` and argv forms | `RedisDB/redis_live.hpp:109,180,257` |

Three observations worth carrying into any evaluation.

**Not all of these parse a query language.** MongoDB builds a BSON document and Redis builds RESP
frames. There is no grammar for an attacker string to escape from, because the value is never part
of a parsed text at any point. This is a stronger position than binding, not merely an equivalent
one, and the distinction is usually lost when NoSQL injection is treated as a footnote to SQL
injection.

**Neo4j is the subtle case.** The Cypher text is assembled with `std::format`, for example
`MATCH (n:{}) RETURN {}{}` at `neo4j_live.hpp:261`. That looks like string building and deserves
scrutiny. The inserted fields are the label and the projection, both from `constexpr` type
metadata, not from input. Runtime values go through a separate parameter map transmitted as typed
Bolt values (`neo4j_live.hpp:174`). **Formatting compile time constants into a template is not
interpolation of untrusted data.** The distinction should be demonstrated, not asserted.

**Redis `%s` is not `printf`.** hiredis does not substitute text; it length prefixes each argument
into a RESP frame, so argument content cannot be read as protocol. The `argv` form
(`redis_live.hpp:257`) makes this explicit.

Transaction control issues literal `BEGIN`, `COMMIT`, `ROLLBACK` constants
(`postgresql_live.hpp:506,518,530`) with no interpolation.

---

## 4. Residual risk

Honest limits. A model that only lists successes is not a model.

| Risk | Status | Why |
|---|---|---|
| `to_sql_literal` escaping path | **Dead code, tracked** | `MySQLDB/mysql_live.hpp:249` escapes and inlines rather than binds. Searching `lib/` and `tests/` finds the definition and no caller. Escaping is weaker than binding. It should be removed or put behind an explicit opt in before someone wires it up. |
| Literal operands in the IR | **Fixed, November 2026** | Silently compared against NULL. Now a compile error at every entry point. See section 2, level 1. |
| Second order injection | **Out of scope** | A value read from the database and later used as an identifier by application code is invisible here. |
| Application logic flaws | **Out of scope** | Missing authorisation and over broad filters are not injection. |
| Migrations and DDL | **Not yet analysed** | The claim in section 2 covers the query path. DDL needs its own pass. |
| Driver vulnerabilities | **Trusted** | libpq, hiredis, bsoncxx, libneo4j-client, the Cassandra driver and SQLite are assumed correct. |
| Denial of service | **Out of scope** | Unbounded result sets and expensive queries are a capacity question, not an injection one. |

---

## 5. How the claims get verified

Stated so the model is falsifiable. The suite lives in
[`security/`](../security/README.md).

1. **Negative compilation tests. Done.** 15 programs that must be rejected, each
   with a named diagnostic, and 3 controls that must compile, so a rejection
   cannot pass by failing for an unrelated reason. All 18 pass under GCC 14.2
   and Clang 18.1, with diagnostics kept per compiler in
   `security/evidence/compile/`.
2. **Differential testing.** The same adversarial input down a deliberately
   naive string built query and down the library. **Done for SQLite:** 22
   applicable payloads, the naive query compromised by 4, the library by 0
   (`security/evidence/differential_sqlite.csv`). The naive path must be
   compromised at least once or the test fails, because otherwise the comparison
   proves nothing. **Pending** for the server backends, which run through the
   dockerised integration stack.
3. **A payload corpus per backend, every payload sourced. Done:** 66 payloads
   from 11 sources (`security/corpus/`). Three are published templates with an
   unfilled slot, and are run exactly as published.
4. **The static check** that nothing but a query IR reaches a connector.
   **Done**, as a compile time check rather than a lint: `check_query_shape`,
   held in place by the compile tests, which run in CI.
5. **Negative results reported with the positive ones.** The literal operand gap
   in section 2 is the first.

The differential oracle sees effects only, so a blind payload that changes the
logic but still returns no rows is not counted. The naive compromise count is
therefore a lower bound. That bias works against the claim, not for it.

---

## 6. Status

Sections 1 to 4 were written on 4 Nov 2026 from a read of the code, and revised
the same day after measurement found the literal operand gap. Section 5 records
what has been run.
