# ORM Thesis Structure Design

**Date:** 2026-04-01  
**Status:** Approved by user — ready for implementation planning  
**Author:** Design session with Alex Tsvetanov

---

## 1. Goals

Design a thesis/documentation structure for the ORM project that:

1. Produces an approximately 100-page **Bulgarian** thesis in `doc/v2/bg`.
2. Produces a **synchronized English mirror** in `doc/v2/en` with the same logical structure, chapter order, technical claims, figures, tables, and code evidence.
3. Preserves the canonical project framing as a **`Compile-time Object-Relational Mapping библиотека`** rather than replacing ORM with a broader but less natural Bulgarian term.
4. Explains not only the relational ORM path, but also the implemented support for **non-relational backends** via the current connector set.
5. Explicitly shows **how the library abstracts over database type** through `connector_trait`, capability tags, and storage-agnostic query IR.
6. Explicitly shows **how the C++ entity/query model dictates the physical data structure or schema** for each backend family.
7. Adds a dedicated chapter on **verification and extensibility**, including the formal contract for implementing a new connector.
8. Anchors every backend scenario in theory and/or an explicit reference to the theoretical chapter.
9. Uses only claims that are supported by the actual code and tests in this repository.

---

## 2. Non-goals

This design does **not** yet:

- Write the full LaTeX chapter content.
- Change the ORM library architecture itself.
- Invent connector capabilities or backend behaviors that are not present in the code.
- Treat the English document as a short appendix; it must remain a structure-synchronized mirror.

---

## 3. Constraints and Source Material

### 3.1 Thesis guidance and document scaffolding

Primary writing and structure constraints come from:

- `example_doc/thesis_writing_guidelines_bg.md`
- `doc/v2/bg/Main.tex`
- `doc/v2/en/Main.tex`
- `doc/references.bib`

### 3.2 Code and architecture evidence

Primary technical evidence comes from:

- `lib/include/ORM/ORM.hpp`
- `lib/include/ORM/entity/property.hpp`
- `lib/include/ORM/entity/table.hpp`
- `lib/include/ORM/entity/relationship.hpp`
- `lib/include/ORM/query/select.hpp`
- `lib/include/ORM/connector/trait.hpp`
- `lib/include/ORM/connector/capabilities.hpp`
- `lib/include/ORM/connector/db.hpp`
- `lib/include/ORM/db/migration/migration.hpp`
- connector implementations under `lib/include/ORM/db/connectors/`

### 3.3 Test evidence

Primary verification evidence comes from:

- `tests/unit/test_property.cpp`
- `tests/unit/test_relationship.cpp`
- `tests/unit/test_query.cpp`
- `tests/unit/test_cpp26_reflection.cpp`
- `tests/unit/test_schema_migration.cpp`
- `tests/unit/test_thread_safety.cpp`
- `tests/unit/test_wire_protocol.cpp`
- backend-specific unit tests under `tests/unit/`
- backend-specific live integration tests under `tests/integration/`

### 3.4 Bilingual documentation rule

All core documentation must remain synchronized between:

- `doc/v2/bg/`
- `doc/v2/en/`

The Bulgarian version is the primary thesis text; the English version mirrors the same chapter logic and technical substance.

---

## 4. Terminology and Framing Decisions

### 4.1 Primary term

The thesis should consistently use the term:

- **`Compile-time Object-Relational Mapping библиотека`**

This keeps the work aligned with the project's identity and the established ORM vocabulary while still allowing explicit discussion of non-relational connectors.

### 4.2 Canonical framing statement

A recommended framing sentence for the introduction and abstract is:

> The developed system is a `Compile-time Object-Relational Mapping` library for C++ whose architecture also enables unified work with relational and non-relational databases through specialized connectors.

### 4.3 Central technical claims

The thesis should explicitly argue the following:

1. The user-facing domain model is described in C++ through entity properties, relationships, and table/container naming traits.
2. Query structure is captured as a **storage-agnostic compile-time IR**, not as raw SQL or backend-native strings.
3. A backend is integrated by specializing `connector_trait<DB>`, which materializes the same logical model into a backend-specific physical representation.
4. Therefore, the **C++ model is the primary source of truth for the logical data model**, while each connector determines how that model is materialized physically.

---

## 5. Approved Logical Chapter Map

The document should follow this logical order in **both** BG and EN outputs:

1. Abstract
2. Introduction
3. Existing Solutions
4. Theoretical Foundations of Compile-time ORM and Storage Models
5. Library Architecture
6. Implementation of Core Subsystems
7. Scenario Analysis Across Relational and Non-relational Databases
8. Verification and Extensibility via New Connectors
9. Limitations and Future Work
10. Conclusion
11. Glossary / Appendices / Bibliography

### 5.1 Note on file naming vs chapter numbering

The logical chapter order above is authoritative for the thesis narrative.

During implementation, LaTeX file naming may reuse or extend the current template layout, but:

- the **document order** in `Main.tex` must match the approved logical structure;
- BG and EN chapter sets must remain synchronized.

---

## 6. Detailed Chapter Requirements

## 6.1 Chapter 4 (logical): Theoretical Foundations of Compile-time ORM and Storage Models

### Purpose

This chapter provides the formal theoretical base for the later implementation and scenario chapters. It must explain the data models and compile-time ORM principles before they are used in connector-specific scenarios.

### Required subsections

- **4.1. Compile-time Object-Relational Mapping as an architectural approach**
  - ORM definition.
  - Runtime vs compile-time ORM.
  - Why C++ templates/concepts matter.

- **4.2. Static data modeling in C++**
  - `property<T, Name>`.
  - `table_name_trait<T>`.
  - `is_entity`.
  - C++ entity model as logical schema source.

- **4.3. Typed query IR**
  - `field`, `Rule`, `Placeholder`.
  - `select_query`, `insert_query`, `update_query`, `delete_query`.
  - Why the system starts from typed query objects instead of backend-native text.

- **4.4. Relationship theory and storage strategies**
  - one-to-one and one-to-many.
  - `relationship<...>`.
  - `store_as::reference` vs `store_as::embed`.

- **4.5. Relational model foundations**
  - table, row, column, primary key, foreign key, join, schema, DDL.

- **4.6. Document model foundations**
  - collections, documents, embedding, references, projection, filtering.

- **4.7. Key-value model foundations**
  - keys, values, hash structures, lookup-by-key constraints.

- **4.8. Graph model foundations**
  - nodes, edges, properties, property-graph semantics.

- **4.9. Wide-column model foundations**
  - partition keys, clustering, constrained access patterns.

- **4.10. Boundaries of a unified abstraction**
  - what can be shared across backends;
  - what must remain backend-specific.

### Required figures

- **Figure 4.1** — storage model taxonomy.
- **Figure 4.2** — mapping from C++ entity declarations to logical schema.
- **Figure 4.3** — query IR pipeline.
- **Figure 4.4** — `reference` vs `embed` relationship strategies.

### Required tables

- **Table 4.1** — C++ constructs vs theoretical ORM concepts.
- **Table 4.2** — relationship/storage strategy comparison.
- **Table 4.3** — comparison of relational, document, key-value, graph, and wide-column models.

### Required listings

- `lib/include/ORM/entity/property.hpp`
- `lib/include/ORM/entity/table.hpp`
- `lib/include/ORM/entity/relationship.hpp`
- `lib/include/ORM/query/select.hpp`
- selected excerpt from `tests/unit/test_cpp26_reflection.cpp`

### Required test evidence

- `tests/unit/test_property.cpp`
  - `Property.ColumnNameInt`
  - `Entity.IsEntityTrueForAggregate`
  - `Table.CustomTableName`

- `tests/unit/test_relationship.cpp`
  - `StoreAs.ValuesAreDifferent`
  - `Relationship.StoreAsReferenceStrategy`
  - `Relationship.StoreAsEmbedStrategy`
  - `Relationship.OneToManyVectorCollection`

- `tests/unit/test_query.cpp`
  - `Rule.EqualityFieldVsPlaceholder`
  - `SelectQuery.WithJoinAddsClause`
  - `UpdateQuery.WithSetAddsStatement`
  - `DeleteQuery.WithWhereAddsClause`

- `tests/unit/test_cpp26_reflection.cpp`
- `tests/unit/test_wire_protocol.cpp`

---

## 6.2 Chapter 7 (logical): Scenario Analysis Across Relational and Non-relational Databases

### Purpose

This chapter demonstrates how the same C++ modeling and query abstractions are materialized differently across the implemented backends. Each scenario must be tied back to the theoretical chapter and supported by real code and tests.

### Scenario template

Every backend subsection must follow the same internal pattern:

1. **Theoretical basis**
   - short reminder and explicit reference to the corresponding subsection from the theoretical chapter.
2. **Logical mapping**
   - how the C++ entity/query model is interpreted for this backend family.
3. **Connector realization**
   - how `connector_trait<DB>` renders or executes the query IR.
4. **Backend-specific constraints and capabilities**
   - what is supported and what is intentionally absent.
5. **Verification**
   - exact tests and what they prove.

### Required subsections

- **7.1. Methodology of scenario analysis**
- **7.2. Relational baseline: SQLite**
- **7.3. Relational portability: PostgreSQL and MySQL**
- **7.4. Document scenario: MongoDB**
- **7.5. Key-value scenario: Redis**
- **7.6. Graph scenario: Neo4j**
- **7.7. Wide-column scenario: Cassandra**
- **7.8. Comparative synthesis**

### Required theory links

- SQLite / PostgreSQL / MySQL -> relational foundations
- MongoDB -> document model foundations
- Redis -> key-value model foundations
- Neo4j -> graph model foundations
- Cassandra -> wide-column model foundations

### Required figures

- **Figure 7.1** — scenario analysis method.
- **Figure 7.2** — SQLite data flow.
- **Figure 7.3** — PostgreSQL/MySQL rendering comparison.
- **Figure 7.4** — MongoDB filter/projection pipeline.
- **Figure 7.5** — Redis key/hash materialization.
- **Figure 7.6** — Neo4j node/property/Cypher mapping.
- **Figure 7.7** — Cassandra partition-constrained access model.
- **Figure 7.8** — comparative backend materialization map.

### Required tables

- **Table 7.1** — theory reference map by backend.
- **Table 7.2** — operation/capability matrix by backend.
- **Table 7.3** — physical data structure comparison by backend.
- **Table 7.4** — test coverage matrix for scenario validation.

### Required listings

- `lib/include/ORM/db/connectors/SQLite/sqlite_db.hpp`
- `lib/include/ORM/db/connectors/PostgreSQLDB/postgresql_db.hpp`
- `lib/include/ORM/db/connectors/MySQLDB/mysql_db.hpp`
- `lib/include/ORM/db/connectors/MongoDB/mongodb_db.hpp`
- `lib/include/ORM/db/connectors/RedisDB/redis_db.hpp`
- `lib/include/ORM/db/connectors/Neo4jDB/neo4j_db.hpp`
- `lib/include/ORM/db/connectors/CassandraDB/cassandra_db.hpp`

### Required test evidence by backend

#### SQLite

- `tests/integration/test_sqlite.cpp`
  - `PrepareSelectWithParamFiltersCorrectly`
  - `PrepareCanBeReusedWithDifferentParams`
  - `PrepareInsertExecutesWithParams`
  - `IndexedPlaceholderTwoDistinctArgsFilter`

#### PostgreSQL

- `tests/unit/test_postgresql_connector.cpp`
  - `PGConnector.DollarParamRendering`
  - `PGConnector.IndexedPlaceholderNativeReuse`
  - `PGConnector.PQClearCalledOnce`
- `tests/integration/test_postgresql_live.cpp`
  - `SelectWithWhereFiltersById`
  - `InsertAddsRow`
  - `UpdateChangesName`
  - `DeleteRemovesRow`

#### MySQL

- `tests/unit/test_mysql_connector.cpp`
  - `MySQLConnector.SelectPositionalPlaceholder`
  - `MySQLConnector.IndexedPlaceholderRewrite`
  - `MySQLConnector.RaiiStmtCloseCalledOnce`
- `tests/integration/test_mysql_live.cpp`
  - `SelectWithWhereFiltersById`
  - `InsertAddsRow`
  - `UpdateChangesName`
  - `DeleteRemovesRow`

#### MongoDB

- `tests/unit/test_mongodb_connector.cpp`
  - `MongoConnector.EqFilterRendering`
  - `MongoConnector.AndFilterRendering`
  - `MongoConnector.ProjectionSuppressId`
  - `MongoConnector.CursorDestroyCalledOnce`
- `tests/integration/test_mongodb_live.cpp`
  - `InsertAndSelectReturnsRow`
  - `InsertMultipleDocuments`
  - `SelectWithWhereFilter`
  - `DeleteRemovesDocuments`

#### Redis

- `tests/unit/test_redis_connector.cpp`
  - `RedisConnector.SingleColumnInsertIssuesSet`
  - `RedisConnector.MultiColumnInsertIssuesHset`
  - `RedisConnector.SingleColumnSelectIssuesGet`
  - `RedisConnector.MultiColumnSelectIssuesHgetall`
- `tests/integration/test_redis_live.cpp`
  - `InsertSingleFieldAndGet`
  - `InsertMultiFieldAndHGetAll`
  - `DeleteRemovesKey`
  - `SelectWithPkReturnsValue`

#### Neo4j

- `tests/unit/test_neo4j_connector.cpp`
  - `Neo4jConnector.SimpleCypherContainsMatchAndReturn`
  - `Neo4jConnector.WhereClauseRendered`
  - `Neo4jConnector.NamedParamInMap`
  - `Neo4jConnector.CloseResultsCalledOnce`
- `tests/integration/test_neo4j_live.cpp`
  - `InsertAndSelectReturnsNode`
  - `InsertMultipleNodes`
  - `SelectWithWhereFilterByAge`
  - `DeleteRemovesNodes`

#### Cassandra

- `tests/unit/test_cassandra_connector.cpp`
  - `CassandraConnector.NoJoinsCapability`
  - `CassandraConnector.SupportsTransactions`
  - `CassandraConnector.SelectWithPartitionKeyRendersCorrectCql`
  - `CassandraConnector.ResultFreeCalledOnce`
- `tests/integration/test_cassandra_live.cpp`
  - `InsertAndSelectById`
  - `InsertMultipleRows`
  - `DeleteRemovesRow`
  - `SelectPriorityColumn`

---

## 6.3 Chapter 8 (logical): Verification and Extensibility via New Connectors

### Purpose

This chapter must explain both how the existing architecture is verified and how the library is extended through additional connectors. It is the explicit place where the thesis formalizes the connector contract.

### Core architectural decision

The thesis must clearly state that the ORM does **not** use an inheritance-based `IConnector` interface. Instead, extensibility is achieved through:

- a `DB` tag/handle type;
- `connector_trait<DB>` specialization;
- compile-time concepts and `requires` constraints;
- compile-time capability tags;
- backend-specific execution/rendering logic.

### Required subsections

- **8.1. Verification goals of the connector-agnostic architecture**
- **8.2. Formal connector-layer contract**
- **8.3. Mandatory elements of a minimal working connector**
- **8.4. Optional capabilities and compile-time gating**
- **8.5. Transactions, thread safety, and prepared queries**
- **8.6. Migrations and schema-aware integration**
- **8.7. Methodology for implementing a new backend**
- **8.8. Criteria for a fully implemented and functioning connector**
- **8.9. Assessment of existing connectors against the contract**

### Formal connector requirements that must be described

#### Mandatory structural elements

A connector must provide:

- a backend type such as `SQLiteDB`, `MongoDB`, `RedisDB`, `Neo4jDB`, `CassandraDB`, etc.;
- a specialization of `connector_trait<DB>`;
- `template <typename T> struct wire_type { using type = ...; };`
- `cursor_type`;
- execution entry points compatible with the ORM query objects it claims to support.

#### Optional capability declarations

A connector may declare only the capabilities it really supports:

- `supports_joins`
- `supports_transactions`
- `supports_aggregation`
- `supports_embedding`
- `supports_upsert`
- `supports_bulk_insert`

Absence of a capability is itself a meaningful part of the contract.

#### Transaction-related hooks

If a connector declares `supports_transactions`, it must also support the transaction layer used by the thread-safety utilities.

#### Migration / DDL hooks

If a connector is intended to participate in schema migration, it must support DDL generation compatible with `migrate<DB>` and its `ddl_for(...)` usage.

#### Mapping obligations

A fully integrated connector must consistently honor:

- `table_name_trait` / `table_name()`
- `property::column_name()`
- relationship semantics expressed through `relationship` and `store_as`

### Required figures

- **Figure 8.1** — architectural connector dispatch path.
- **Figure 8.2** — flowchart for implementing a new connector.
- **Figure 8.3** — connector maturity model.

### Required tables

- **Table 8.1** — mandatory vs optional connector elements.
- **Table 8.2** — connector contract matrix.
- **Table 8.3** — capability matrix of existing connectors.
- **Table 8.4** — checklist for a fully implemented connector.

### Required listings

- `lib/include/ORM/connector/trait.hpp`
- `lib/include/ORM/connector/capabilities.hpp`
- `lib/include/ORM/connector/db.hpp`
- `lib/include/ORM/db/connectors/ThreadSafety/thread_safety.hpp`
- `lib/include/ORM/db/migration/migration.hpp`
- `lib/include/ORM/db/connectors/MockDB/mock_db_migration.hpp`
- one complete reference `connector_trait` implementation, preferably `sqlite_db.hpp`

### Required verification evidence

#### Concept and capability compliance

- `tests/unit/test_mongodb_connector.cpp`
- `tests/unit/test_redis_connector.cpp`
- `tests/unit/test_neo4j_connector.cpp`
- `tests/unit/test_cassandra_connector.cpp`
- `tests/unit/test_postgresql_connector.cpp`
- `tests/unit/test_mysql_connector.cpp`

Representative tests:

- `SatisfiesIsConnector`
- capability presence/absence assertions

#### Thread safety and transaction behavior

- `tests/unit/test_thread_safety.cpp`
  - `PoolInstantiatesWithSupportedConnector`
  - `PoolN1SecondThreadBlocksUntilRelease`
  - `TransactionAutoRollback`
  - `TransactionCommit`
  - `SupportsConcurrentExecutePresent`

#### Migration contract

- `tests/unit/test_schema_migration.cpp`
  - `DiffCreateTableForMissingTable`
  - `DiffAddColumnForMissingColumn`
  - `DdlGenerationCreateTable`
  - `StateMachineReachesDone`

#### Prepared query and query reuse

- `tests/integration/test_mockdb.cpp`
  - `PrepareReturnsExecutableObject`
  - `PrepareWithParamExecutesCorrectly`
  - `PrepareCanBeCalledMultipleTimesWithDifferentParams`
  - `PrepareExposesQueryIR`
- `tests/integration/test_sqlite.cpp`
  - `PrepareSelectAndExecuteReturnsRows`
  - `PrepareCanBeReusedWithDifferentParams`
  - `PrepareInsertExecutesWithParams`

#### Extended execution / wire integration

- `tests/unit/test_wire_protocol.cpp`

---

## 7. Cross-chapter Writing Rules

The final thesis text must follow these rules:

1. Every backend scenario must begin with a **theoretical basis** subsection or an explicit reference to the relevant theoretical subsection.
2. Every backend scenario must end with a **verification** subsection tied to exact unit/integration tests.
3. Chapter 8 must explicitly explain that connector extensibility is **trait-based, not inheritance-based**.
4. Capabilities must never be described generically; they must match the actual declarations in connector traits.
5. The text must clearly distinguish:
   - **logical data model**;
   - **query IR**;
   - **physical materialization per backend**.
6. Inline technical names should use the exact code identifiers from the repository.
7. BG and EN versions must preserve the same claims, structure, figure/table numbering strategy, and evidence references.

---

## 8. Evidence Map for the Final Writing Phase

### Core entity and query abstractions

- `lib/include/ORM/entity/property.hpp`
- `lib/include/ORM/entity/table.hpp`
- `lib/include/ORM/entity/relationship.hpp`
- `lib/include/ORM/query/select.hpp`
- `tests/unit/test_property.cpp`
- `tests/unit/test_relationship.cpp`
- `tests/unit/test_query.cpp`
- `tests/unit/test_cpp26_reflection.cpp`

### Backend abstraction and capability gating

- `lib/include/ORM/connector/trait.hpp`
- `lib/include/ORM/connector/capabilities.hpp`
- `lib/include/ORM/connector/db.hpp`

### Migration and extensibility

- `lib/include/ORM/db/migration/migration.hpp`
- `lib/include/ORM/db/connectors/MockDB/mock_db_migration.hpp`
- `tests/unit/test_schema_migration.cpp`
- `tests/unit/test_thread_safety.cpp`

### Backend scenario evidence

- SQLite -> `sqlite_db.hpp`, `tests/integration/test_sqlite.cpp`
- PostgreSQL -> `postgresql_db.hpp`, `tests/unit/test_postgresql_connector.cpp`, `tests/integration/test_postgresql_live.cpp`
- MySQL -> `mysql_db.hpp`, `tests/unit/test_mysql_connector.cpp`, `tests/integration/test_mysql_live.cpp`
- MongoDB -> `mongodb_db.hpp`, `tests/unit/test_mongodb_connector.cpp`, `tests/integration/test_mongodb_live.cpp`
- Redis -> `redis_db.hpp`, `tests/unit/test_redis_connector.cpp`, `tests/integration/test_redis_live.cpp`
- Neo4j -> `neo4j_db.hpp`, `tests/unit/test_neo4j_connector.cpp`, `tests/integration/test_neo4j_live.cpp`
- Cassandra -> `cassandra_db.hpp`, `tests/unit/test_cassandra_connector.cpp`, `tests/integration/test_cassandra_live.cpp`

---

## 9. LaTeX Implementation Implications

The later writing phase must:

1. Expand or reorganize `doc/v2/bg/Main.tex` so that the logical chapter order matches this approved design.
2. Create the missing Bulgarian chapter files under `doc/v2/bg/chapters/`.
3. Update the English chapter set under `doc/v2/en/` so that it mirrors the same logical structure.
4. Reuse and adapt the existing English introductory material where it is already aligned with the approved structure.
5. Keep `doc/references.bib` as the central bibliography source.

---

## 10. Readiness for the Next Phase

This design is approved and ready for implementation planning.

The next phase should produce:

- a concrete chapter-file plan for BG and EN LaTeX sources;
- a writing sequence for the thesis content;
- a synchronization strategy for bilingual chapters;
- a verification plan for LaTeX structure and the final document build.
