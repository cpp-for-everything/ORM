# Security policy

## Reporting a vulnerability

Open a [private security advisory](https://github.com/cpp-for-everything/ORM/security/advisories/new)
rather than a public issue. Include the backend, a minimal reproducing entity and query, and the
version or commit.

Please report anything that lets attacker controlled input change the **structure** of a query
rather than only its **values**. That is the property this library exists to protect, and a
counterexample to it is the most valuable report we can receive.

## The security property this library claims

> **Attacker controlled input can supply values to a query. It cannot change the query's shape,
> its target table, its projected columns, or the command being issued.**

The claim rests on where each part of a query comes from.

| Part of a query | Origin | Can runtime input reach it? |
|---|---|---|
| Table and column names | `constexpr` member pointer metadata | **No** |
| Query shape (select, where, join, order) | C++ types in the query IR | **No** |
| Values | typed positional placeholders, `orm::ph<T, _N>` | **Yes, as data only** |

Identifiers come from `column_name()`, which is `constexpr` and derived from the member pointer
([`details/member_pointer.hpp:44`](lib/include/ORM/details/member_pointer.hpp),
[`entity/property.hpp:19`](lib/include/ORM/entity/property.hpp)). They are fixed when the program
is compiled. There is no API that accepts a table name, a column name or a query fragment as a
runtime string.

Values travel separately from the statement, through `prepared_query::execute(args...)`
([`connector/prepared_query.hpp`](lib/include/ORM/connector/prepared_query.hpp)), and each backend
hands them to its driver as parameters rather than splicing them into text.

## How the property is enforced, per backend

The guarantee is uniform. **The mechanism is not.** Each connector uses the strongest facility its
driver provides.

| Backend | Mechanism | Kind |
|---|---|---|
| PostgreSQL | `PQexecParams` with parameter arrays | server side binding |
| MySQL | `MYSQL_BIND` prepared statements | server side binding |
| SQLite | `sqlite3_prepare_v2` plus `sqlite3_bind_*` | server side binding |
| Cassandra | `cass_statement_bind_*` | server side binding |
| MongoDB | bsoncxx document builder | structural, no query text is parsed |
| Neo4j | Bolt parameters via `neo4j_map_entry` | driver level typed parameters |
| Redis | hiredis `%s` and argv forms | protocol level, length prefixed encoding |

Three of these do not involve a query language at all. MongoDB builds a BSON document, Redis
builds RESP frames, and neither parses attacker text as syntax. That is a different and in some
respects stronger position than parameter binding.

Transactions issue the literal constants `BEGIN`, `COMMIT` and `ROLLBACK`
([`PostgreSQLDB/postgresql_live.hpp:463`](lib/include/ORM/db/connectors/PostgreSQLDB/postgresql_live.hpp)).
These carry no user data.

## Known boundaries

The claim above is deliberately narrow. These are outside it.

1. **Dynamic identifiers are not supported, and that is the point.** If you need a table or column
   chosen at runtime, this library will not do it for you. Do not add a string overload to work
   around it. That overload would be the vulnerability.
2. **Second order injection is not addressed.** If your application reads a value out of the
   database and then uses it as an identifier somewhere else, that is your code's problem and this
   library cannot see it.
3. **Application logic flaws are out of scope.** Missing authorisation, returning another user's
   rows, and overly broad filters are not injection and are not prevented here.
4. **Migrations and DDL** are not covered by the statement above and need their own review.
5. **The driver is trusted.** A flaw in libpq, hiredis, bsoncxx, libneo4j-client or the Cassandra
   driver is outside this boundary.

## Latent risk being tracked

`to_sql_literal` in
[`MySQLDB/mysql_live.hpp:248`](lib/include/ORM/db/connectors/MySQLDB/mysql_live.hpp) builds SQL by
**escaping and inlining** a value instead of binding it. Its own comment describes it as being for
queries without prepared statements.

**It is currently dead code.** A search of `lib/` and `tests/` finds the definition and no call
site, so no live path uses it. It is recorded here because escaping is a weaker mechanism than
binding, and because a future contributor could wire it up without realising the guarantee is
different on that path. It should be removed or quarantined behind an explicit opt in.

Full analysis, including the attacker model and the verification plan, is in
[`docs/THREAT_MODEL.md`](docs/THREAT_MODEL.md).
