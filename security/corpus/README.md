# Injection payload corpus

This corpus is the input to a differential test for the thesis. Each payload is fed
both to a deliberately naive, string-concatenating query builder and to the
compile-time C++ ORM, for several data stores. The test checks whether the ORM's
output differs from the naive query in a way that defeats the injection, for every
payload here.

## Files

- `payloads.csv`: one row per payload. Columns: `id,class,dialect,technique,payload,expected_effect,source_id`.
- `sources.bib`: one BibTeX entry per source, keyed by `source_id`.
- `README.md`: this file.

## Selection method

Every payload was copied, by reading the live page or raw file with a fetch tool,
from one of the public sources listed below. Nothing here was written from memory
or guessed. Where a source gave a payload as a URL query string (for example
`http://example.com/item?id=1 AND 1=1 --`), only the injected value was kept in the
`payload` field, since that value, not the surrounding URL, is what the differential
test feeds to the query builder. Where a source used bracket placeholders such as
`[BOOLEAN_QUERY]` or `[RANDNUM]`, those placeholders were kept as written in the
source rather than filled in with an invented value.

A payload was left out when I could not find it stated as an exact string in one of
the sources below, even if I believed such a payload probably exists. See "What I
could not source" below for the specific gaps this left.

## Sources used

- PayloadsAllTheThings, SQL Injection (generic), MySQL, PostgreSQL, SQLite, Cassandra
  and NoSQL Injection pages (github.com/swisskyrepo/PayloadsAllTheThings)
- PortSwigger Web Security Academy, SQL injection cheat sheet and NoSQL injection page
- OWASP wiki, Testing for NoSQL injection
- ExploitNotes, Neo4j (Cypher injection)
- GitHub Security Advisory GHSA-rgrr-p7gp-5xj7 (Netty codec-redis CRLF injection),
  read via osv.dev

Full citations are in `sources.bib`.

## Counts per class (counted from payloads.csv, 4 November 2026)

- sql: 36
- nosql: 15
- cypher: 6
- cql: 4
- redis: 5
- total: 66

## Escape convention for control characters

Some Redis payloads contain carriage return and line feed bytes. In `payloads.csv`
these are written as the two literal characters backslash-r (`\r`) and backslash-n
(`\n`), not as real CR or LF bytes. A row's `payload` field should be read with
`\r` and `\n` decoded back to CR and LF before it is sent to a test harness. The
same convention applies to the embedded double quotes in the two Redis
`CONFIG SET requirepass ""` payloads: they appear in the CSV as escaped quotes
inside the quoted field, representing a literal `""` (two double-quote characters)
in the Redis command itself.

## What I wanted but could not find a real source for

- **SQL, dialect-specific UNION for SQLite**: the SQLite Injection page in
  PayloadsAllTheThings has no UNION-based section. I did not add a guessed one.
- **SQL, stacked queries for SQLite**: the source's only stacked-query example is an
  `ATTACH DATABASE` chain that writes a web shell through a full RCE recipe. I left
  it out rather than splice out a partial payload that changes its meaning.
- **SQL, out-of-band for MySQL/MSSQL/Oracle**: PayloadsAllTheThings has out-of-band
  payloads (`LOAD_FILE`, `xp_dirtree`, `UTL_INADDR.get_host_address`) built around a
  live Burp Collaborator subdomain placeholder. I judged these as belonging more to
  exfiltration research than to a differential query-correctness test and left them
  out, except the PostgreSQL `COPY ... TO PROGRAM` entry, which is a direct
  injection-to-command-execution technique.
- **Cypher, UNION-based injection**: a web search turned up only a third-party
  "HackTricks-skills" page with UNION-style Cypher examples that read as speculative
  mixes of Cypher and SQL syntax, not a verified, citable payload. I did not include
  a UNION row for Cypher.
- **CQL, UNION and stacked queries**: a secondary source (Invicti's CQL injection
  research) states that CQL has no UNION, no joins, no subqueries, and that CQL
  client drivers do not support stacked statements at all. I treated that as a
  reason these techniques do not exist for CQL, rather than a gap to fill, and did
  not add rows for them. The task's CQL coverage ends up being comment syntax plus
  one quote-breaking authentication bypass, split across the username and password
  fields as the source presents it.
- **CQL, batch tricks**: no public source gave an exact batch-statement injection
  payload string. Left out.
- **Redis, EVAL-based injection**: I found general discussion of Lua scripting risk
  in Redis but no citable exact payload string demonstrating EVAL-based injection
  from user input. Left out.
- **Redis, CONFIG SET / FLUSHALL via gopher SSRF chain**: the assetnote
  blind-ssrf-chains project documents a real technique (flush the database, point
  `dir`/`dbfilename` at a cron spool or web root, then `SAVE`, all sent as one
  gopher-encoded request) to get remote code execution through an exposed Redis
  instance. I could not get the exact gopher-encoded payload strings reproduced
  through my fetch tooling, and judged that technique as SSRF-delivery rather than
  query/command injection against the data store itself, so it is not represented
  as a row here. The two CRLF command-smuggling payloads actually used
  (`REDIS-001`, `REDIS-002`) come from a different, verifiable source (a GitHub
  Security Advisory with an exact proof-of-concept string) and demonstrate the same
  underlying class: CRLF inside a value lets an attacker append a second Redis
  command.

## Definition of done, checked

- Every `source_id` in `payloads.csv` has a matching entry in `sources.bib`:
  `patt_sqli`, `patt_postgres`, `patt_mysql`, `patt_sqlite`, `patt_nosql`,
  `patt_cassandra`, `portswigger_nosql`, `portswigger_sqli_cheatsheet`,
  `owasp_wstg_nosql`, `exploitnotes_neo4j`, `ghsa_redis_crlf`.
- Per-class counts above were produced by counting rows in `payloads.csv`, not
  estimated.
- No payload in this corpus was invented. Anything I could not find a public,
  citable exact string for is listed above instead of being filled in.
