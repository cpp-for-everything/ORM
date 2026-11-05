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

### A gap this claim used to have

Until November 2026 a value could also be written straight into a query, as in
`field<&T::col> == value`, without a placeholder. It compiled, and no connector
bound it: each rendered it as an unbound parameter that the database read as
NULL. Measured on SQLite, a matching row was not returned, an `UPDATE` bound its
argument into the wrong slot, and a `DELETE` deleted nothing
([`security/probes`](security/probes/README.md)). The value never became SQL
text, so it was not injectable, but the result was silently wrong.

It is now a compile error at every execution entry point, enforced by
`detail::check_query_shape` in
[`connector/param_check.hpp`](lib/include/ORM/connector/param_check.hpp). The
same check rejects anything that is not a query, so query text such as a
`std::string` is refused at the API boundary with a message naming the problem.

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
([`PostgreSQLDB/postgresql_live.hpp:506`](lib/include/ORM/db/connectors/PostgreSQLDB/postgresql_live.hpp)).
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

## Verification

The claims above are tested, not only argued. See [`security/`](security/README.md).

- **18 compile checks.** 15 programs that must be rejected, each with a named
  diagnostic, and 3 controls that must compile. All pass under GCC 14.2 and
  Clang 18.1.
- **A differential test.** Every applicable payload from a sourced corpus goes
  through a deliberately naive string built query and through the library. On
  SQLite, 22 payloads: the naive query was compromised by 4, the library by 0.
- **The same differential against live servers**, run in CI in the `test-all`
  image against the compose services. Each backend has a naive path that is the
  realistic vulnerable construction for that store.

  | Backend (server) | Applicable | Naive compromised | Library with any effect | Conclusive |
  |-|-|-|-|-|
  | PostgreSQL 15.19 | 22 | 2 | 0 | yes |
  | MySQL (MariaDB 10.11.19) | 24 | 2 | 0 | yes |
  | MongoDB 7.0.45 | 15 | 2 | 0 | yes |
  | Redis 7.2.17 | 5 | 2 | 0 | yes |
  | Cassandra 4.1.12 | 4 | 0 | 0 | **no** |
  | Neo4j | 6 | not measured | not measured | no |

  Numbers are from `security/evidence/differential_<backend>_summary.txt`. On
  Cassandra the naive path was never compromised, so the library result there
  proves nothing by comparison. Neo4j is not measured: the CI image has no
  `libneo4j-client` and the compose file has no Neo4j service. For both, the
  guarantee rests on the source analysis above. The oracle sees effects only and
  the naive constructions are the author's choice, so the naive counts are lower
  bounds.

The suite runs in CI with `ORM_ENABLE_SECURITY=ON`.

Full analysis, including the attacker model and the verification plan, is in
[`docs/THREAT_MODEL.md`](docs/THREAT_MODEL.md).
