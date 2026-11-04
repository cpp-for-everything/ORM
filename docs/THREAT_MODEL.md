# Threat model

Scope: the query construction and execution path of this library, across all seven supported
backends. Companion to [`SECURITY.md`](../SECURITY.md), which states the policy. This document
states the reasoning and the evidence.

Every claim below cites the code it came from. Where something has not been verified yet, it says
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

The cost is real and should be stated plainly: genuinely dynamic schemas are not supported.

### Level 3. The attacker controls structure

The attacker chooses the shape of the query, for example a filter builder that assembles
predicates from a request.

**Eliminated, with the same cost.** The query is a C++ type. A `select(...).where(...)` expression
produces an IR whose shape is fixed at compile time. Shape cannot be assembled from runtime data
because there is no runtime representation of shape to assemble.

A program that needs this must enumerate the permitted shapes as distinct compile time queries and
select among them. That is more work, and it is also the only form in which the choice is
auditable.

---

## 3. How each backend enforces it

The guarantee is uniform. The mechanism is not. This matters: a single abstraction is being
defended across four different protocols, and the argument has to hold separately for each.

| Backend | Mechanism | Evidence |
|---|---|---|
| PostgreSQL | `PQexecParams`, parameter arrays | `PostgreSQLDB/postgresql_live.hpp:503,509,537,543` |
| MySQL | `MYSQL_BIND` prepared statements | `MySQLDB/mysql_live.hpp:313` `param_binder` |
| SQLite | `sqlite3_prepare_v2` and `sqlite3_bind_*` | `SQLite/sqlite_db.hpp:62,67,72` |
| Cassandra | `cass_statement_bind_*` | `CassandraDB/cassandra_live.hpp:200-221` |
| MongoDB | bsoncxx document builder, `append` | `MongoDB/mongodb_live.hpp:212-236` |
| Neo4j | Bolt parameters, `neo4j_map_entry` | `Neo4jDB/neo4j_live.hpp:175` |
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
Bolt values (`neo4j_live.hpp:175`). **Formatting compile time constants into a template is not
interpolation of untrusted data.** The distinction should be demonstrated, not asserted.

**Redis `%s` is not `printf`.** hiredis does not substitute text; it length prefixes each argument
into a RESP frame, so argument content cannot be read as protocol. The `argv` form
(`redis_live.hpp:257`) makes this explicit.

Transaction control issues literal `BEGIN`, `COMMIT`, `ROLLBACK` constants
(`postgresql_live.hpp:463,475,487`) with no interpolation.

---

## 4. Residual risk

Honest limits. A model that only lists successes is not a model.

| Risk | Status | Why |
|---|---|---|
| `to_sql_literal` escaping path | **Dead code, tracked** | `MySQLDB/mysql_live.hpp:248` escapes and inlines rather than binds. Searching `lib/` and `tests/` finds the definition and no caller. Escaping is weaker than binding. It should be removed or put behind an explicit opt in before someone wires it up. |
| Second order injection | **Out of scope** | A value read from the database and later used as an identifier by application code is invisible here. |
| Application logic flaws | **Out of scope** | Missing authorisation and over broad filters are not injection. |
| Migrations and DDL | **Not yet analysed** | The claim in section 2 covers the query path. DDL needs its own pass. |
| Driver vulnerabilities | **Trusted** | libpq, hiredis, bsoncxx, libneo4j-client, the Cassandra driver and SQLite are assumed correct. |
| Denial of service | **Out of scope** | Unbounded result sets and expensive queries are a capacity question, not an injection one. |

---

## 5. How the claims get verified

Stated here so the model is falsifiable. None of this is implemented yet.

1. **Differential testing.** The same adversarial input down two paths: a deliberately naive string
   built query, and the library. Run against all seven backends using the existing dockerised
   integration harness (`tests/integration/`, `DockerRun.cmake`). The naive path must be shown to
   be exploitable, otherwise the comparison proves nothing.
2. **A payload corpus per backend**, not SQL only: SQL, MongoDB operator injection, Cypher, CQL,
   and Redis protocol level attempts. Provenance recorded for every payload. Payloads are not
   invented and then presented as a corpus.
3. **A static check** that no query reaching a driver was built from a runtime string. If this can
   be enforced in CI rather than argued in prose, the check is itself a result, and it is what
   stops the guarantee eroding later.
4. **Negative results reported with the positive ones.** Specifically: that dynamic identifiers are
   unsupported rather than protected, and that the escaping path would weaken the guarantee if
   revived.

---

## 6. Status

Written 4 Nov 2026 from a read of the code at that date. Sections 1 to 4 are measured. Section 5 is
a plan and nothing in it has been run.
