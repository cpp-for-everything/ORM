add_executable(test_integration
    test_mockdb.cpp
)

target_link_libraries(test_integration PRIVATE
    orm::orm
    orm::mockdb
    GTest::gtest_main
)

target_compile_options(test_integration PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
)

gtest_discover_tests(test_integration)

# ── SQLite integration tests (only when SQLite3 was found) ────────────────────
if(TARGET orm::sqlite)
    add_executable(test_sqlite
        test_sqlite.cpp
    )

    target_link_libraries(test_sqlite PRIVATE
        orm::sqlite
        orm::mockdb
        GTest::gtest_main
    )

    target_compile_options(test_sqlite PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    )

    gtest_discover_tests(test_sqlite)
endif()

# ── DockerRun.cmake path (lives beside this file) ─────────────────────────────
set(ORM_DOCKER_RUN_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/DockerRun.cmake")

# ── MySQL live integration tests ──────────────────────────────────────────────
if(TARGET orm::mysql_live AND ORM_MYSQL_LIVE_ENABLED)
    add_executable(test_mysql_live test_mysql_live.cpp)
    target_link_libraries(test_mysql_live PRIVATE orm::mysql_live GTest::gtest_main)
    target_compile_options(test_mysql_live PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    )

    # Direct test for docker-compose environment (uses existing services)
    add_test(NAME MySQLLive COMMAND test_mysql_live)
    set_tests_properties(MySQLLive PROPERTIES TIMEOUT 60)
endif()

# ── PostgreSQL live integration tests ─────────────────────────────────────────
if(TARGET orm::postgresql_live AND ORM_POSTGRESQL_LIVE_ENABLED)
    add_executable(test_postgresql_live test_postgresql_live.cpp)
    target_link_libraries(test_postgresql_live PRIVATE orm::postgresql_live GTest::gtest_main)
    target_compile_options(test_postgresql_live PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    )

    # Direct test for docker-compose environment (uses existing services)
    add_test(NAME PostgreSQLLive COMMAND test_postgresql_live)
    set_tests_properties(PostgreSQLLive PROPERTIES TIMEOUT 60)
endif()

# ── MongoDB live integration tests ────────────────────────────────────────────
if(TARGET orm::mongodb_live AND ORM_MONGODB_LIVE_ENABLED)
    add_executable(test_mongodb_live test_mongodb_live.cpp)
    target_link_libraries(test_mongodb_live PRIVATE orm::mongodb_live GTest::gtest_main)
    target_compile_options(test_mongodb_live PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    )

    # Direct test for docker-compose environment (uses existing services)
    add_test(NAME MongoDBLive COMMAND test_mongodb_live)
    set_tests_properties(MongoDBLive PROPERTIES TIMEOUT 60)
endif()

# ── Redis live integration tests ──────────────────────────────────────────────
if(TARGET orm::redis_live AND ORM_REDIS_LIVE_ENABLED)
    add_executable(test_redis_live test_redis_live.cpp)
    target_link_libraries(test_redis_live PRIVATE orm::redis_live GTest::gtest_main)
    target_compile_options(test_redis_live PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    )

    # Direct test for docker-compose environment (uses existing services)
    add_test(NAME RedisLive COMMAND test_redis_live)
    set_tests_properties(RedisLive PROPERTIES TIMEOUT 60)
endif()

# ── Cassandra live integration tests ──────────────────────────────────────────
if(TARGET orm::cassandra_live AND ORM_CASSANDRA_LIVE_ENABLED)
    add_executable(test_cassandra_live test_cassandra_live.cpp)
    target_link_libraries(test_cassandra_live PRIVATE orm::cassandra_live GTest::gtest_main)
    target_compile_options(test_cassandra_live PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    )

    # Direct test for docker-compose environment (uses existing services)
    add_test(NAME CassandraLive COMMAND test_cassandra_live)
    set_tests_properties(CassandraLive PROPERTIES TIMEOUT 60)
endif()

# ── Neo4j live integration tests ──────────────────────────────────────────────
if(TARGET orm::neo4j_live AND ORM_NEO4J_LIVE_ENABLED)
    add_executable(test_neo4j_live test_neo4j_live.cpp)
    target_link_libraries(test_neo4j_live PRIVATE orm::neo4j_live GTest::gtest_main)
    target_compile_options(test_neo4j_live PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    )

    add_test(
        NAME IntegrationNeo4j
        COMMAND ${CMAKE_COMMAND}
            -DDB=neo4j
            -DIMAGE=neo4j:5
            -DHOST_PORT=7688
            -DCONTAINER_PORT=7687
            "-DEXTRA_DOCKER_ARGS=-e;NEO4J_AUTH=neo4j/orm_test_password"
            "-DEXTRA_ENV_ARGS=ORM_NEO4J_USER=neo4j;ORM_NEO4J_PASSWORD=orm_test_password;ORM_NEO4J_PORT=7688"
            -DTEST_BINARY=$<TARGET_FILE:test_neo4j_live>
            -P ${ORM_DOCKER_RUN_SCRIPT}
    )
    set_tests_properties(IntegrationNeo4j PROPERTIES TIMEOUT 240)
endif()
