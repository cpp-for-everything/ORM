add_executable(test_unit
    test_types.cpp
    test_property.cpp
    test_relationship.cpp
    test_join_infer.cpp
    test_query.cpp
    test_mysql_connector.cpp
    test_postgresql_connector.cpp
    test_mongodb_connector.cpp
    test_redis_connector.cpp
    test_cassandra_connector.cpp
    test_neo4j_connector.cpp
    test_thread_safety.cpp
    test_wire_protocol.cpp
    test_cpp26_reflection.cpp
    test_schema_migration.cpp
)

target_link_libraries(test_unit PRIVATE
    orm::orm
    GTest::gtest_main
)

target_compile_options(test_unit PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
)

gtest_discover_tests(test_unit)

# ── Async infrastructure tests ───────────────────────────────────────────────
add_executable(test_async_unit
    test_task.cpp
    test_io_context.cpp
    test_thread_pool.cpp
    test_async_connector.cpp
)

target_link_libraries(test_async_unit PRIVATE
    orm::async
    GTest::gtest_main
)

target_compile_options(test_async_unit PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
)

gtest_discover_tests(test_async_unit)
