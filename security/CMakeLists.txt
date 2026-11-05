# ─────────────────────────────────────────────────────────────────────────────
#  Security evaluation suite (ORM_ENABLE_SECURITY=ON).
#
#  The evidence behind SECURITY.md and docs/THREAT_MODEL.md, in three parts:
#
#    compile_fail/  programs that must NOT compile, each rejected with a named
#                   diagnostic, plus controls that MUST compile. Evidence that
#                   identifiers and query structure cannot be reached from input
#                   known only at run time.
#    runtime/       SQLite tests of the documented path, and the differential
#                   test: every payload in corpus/payloads.csv sent through a
#                   deliberately naive string built query and through the ORM.
#    evidence/      what the runs produced. Written into the SOURCE tree on
#                   purpose, so a run is a reviewable diff and can be cited.
# ─────────────────────────────────────────────────────────────────────────────
include(FetchContent)
include(GoogleTest)
FetchContent_GetProperties(boost_pfr)

set(ORM_SEC_EVIDENCE "${CMAKE_CURRENT_SOURCE_DIR}/evidence")
file(MAKE_DIRECTORY "${ORM_SEC_EVIDENCE}/compile/${CMAKE_CXX_COMPILER_ID}")

# ── 1. negative compilation tests ────────────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(_orm_sec_inc "${PROJECT_SOURCE_DIR}/lib/include|${boost_pfr_SOURCE_DIR}/include")

    function(orm_compile_check name expect regex)
        add_test(NAME security.compile.${name}
            COMMAND "${CMAKE_COMMAND}"
                "-DCXX=${CMAKE_CXX_COMPILER}"
                "-DCXX_ID=${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
                "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/${name}.cpp"
                "-DINCLUDES=${_orm_sec_inc}"
                "-DROOT=${PROJECT_SOURCE_DIR}"
                "-DEXPECT=${expect}"
                "-DREGEX=${regex}"
                "-DLOG=${ORM_SEC_EVIDENCE}/compile/${CMAKE_CXX_COMPILER_ID}/${name}.log"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_compile.cmake")
        set_tests_properties(security.compile.${name} PROPERTIES LABELS "security;compile")
    endfunction()

    # Controls. They must compile, otherwise every rejection below could be
    # failing for the same unrelated reason and would prove nothing.
    orm_compile_check(00_control_wellformed       pass "")
    orm_compile_check(14_control_null_compare     pass "")
    orm_compile_check(15_control_field_compare    pass "")

    # Level 1, values.
    orm_compile_check(06_placeholder_type_mismatch fail "incompatible")

    # Level 2, identifiers.
    orm_compile_check(01_runtime_column_name        fail "constant expression|not usable in a constant")
    orm_compile_check(16_column_name_from_string    fail "deduction failed|no matching|no viable|could not convert")
    orm_compile_check(02_runtime_table_name         fail "constant expression|constexpr")
    orm_compile_check(04_runtime_member_pointer     fail "constant expression|not usable in a constant|not a valid template argument")
    orm_compile_check(17_columns_are_distinct_types fail "distinct pointer types|incompatible operand types")

    # Level 3, structure.
    orm_compile_check(03_raw_sql_string_execute    fail "this is not a query")
    orm_compile_check(05_where_raw_string          fail "constraint|not satisfied")

    # Literal operands: the defect this work found. One case per entry point.
    foreach(_c 07_literal_where_execute 08_literal_where_stream 09_literal_where_find_one
               10_literal_where_prepared 11_literal_update_set 12_literal_delete_where
               13_literal_constant)
        orm_compile_check(${_c} fail "literal operand")
    endforeach()

    # IN / NOT IN. The list has a control of its own, then one case per way a value
    # could get round the placeholder.
    orm_compile_check(18_control_list               pass "")
    orm_compile_check(19_list_literal_values        fail "no matching|no viable|deduction failed|could not convert")
    orm_compile_check(20_list_element_type          fail "incompatible element type")
    orm_compile_check(21_list_argument_type         fail "incompatible with the")
    orm_compile_check(22_list_scalar_argument       fail "incompatible with the")
    orm_compile_check(23_list_in_update_set         fail "literal operand")
    orm_compile_check(24_list_with_indexed          fail "cannot share a query")
    orm_compile_check(25_list_without_argument      fail "needs its argument")
    orm_compile_check(26_list_unsupported_connector fail "does not support IN")
    orm_compile_check(27_list_equals                fail "incompatible operand types")

    # Migrations.
    orm_compile_check(30_control_migration_declared pass "")
    orm_compile_check(28_migration_runtime_column   fail "no matching function for call to.*entity_meta_of")
    orm_compile_check(29_migration_no_ddl_path      fail "no DDL path")
else()
    message(STATUS "ORM security: negative compilation tests need GCC or Clang, skipped for ${CMAKE_CXX_COMPILER_ID}.")
endif()

# One migration differential: the corpus and the constructed identifier cases, through
# the four positions described in runtime/migration_differential.hpp.
function(orm_migration_differential name target labels)
    add_executable(security_migration_${name} runtime/differential_migration_${name}.cpp)
    target_link_libraries(security_migration_${name} PRIVATE ${target})
    add_test(NAME security.migration.differential.${name}
        COMMAND security_migration_${name}
            "${CMAKE_CURRENT_SOURCE_DIR}/corpus/payloads.csv"
            "${CMAKE_CURRENT_SOURCE_DIR}/corpus/identifier_cases.csv"
            "${ORM_SEC_EVIDENCE}/migration_${name}.csv"
            "${ORM_SEC_EVIDENCE}/migration_${name}_summary.txt")
    set_tests_properties(security.migration.differential.${name} PROPERTIES
        LABELS "security;migration;differential;${labels}"
        RESOURCE_LOCK "${name}_server"
        TIMEOUT 900)
endfunction()

# ── 2. runtime tests against SQLite ──────────────────────────────────────────
if(TARGET orm::sqlite)
    add_executable(security_sqlite_unit runtime/test_injection_sqlite.cpp)
    target_link_libraries(security_sqlite_unit PRIVATE orm::sqlite GTest::gtest_main)
    gtest_discover_tests(security_sqlite_unit
        TEST_PREFIX "security.sqlite."
        PROPERTIES LABELS "security;sqlite")

    add_executable(security_list_sqlite runtime/test_list_sqlite.cpp)
    target_link_libraries(security_list_sqlite PRIVATE orm::sqlite GTest::gtest_main)
    gtest_discover_tests(security_list_sqlite
        TEST_PREFIX "security.list."
        PROPERTIES LABELS "security;sqlite;list")

    add_executable(security_migration_sqlite_unit runtime/test_migration_sqlite.cpp)
    target_link_libraries(security_migration_sqlite_unit PRIVATE orm::sqlite GTest::gtest_main)
    gtest_discover_tests(security_migration_sqlite_unit
        TEST_PREFIX "security.migration."
        PROPERTIES LABELS "security;sqlite;migration")

    add_executable(security_differential_sqlite runtime/differential_sqlite.cpp)
    target_link_libraries(security_differential_sqlite PRIVATE orm::sqlite)
    add_test(NAME security.differential.sqlite
        COMMAND security_differential_sqlite
            "${CMAKE_CURRENT_SOURCE_DIR}/corpus/payloads.csv"
            "${ORM_SEC_EVIDENCE}/differential_sqlite.csv"
            "${ORM_SEC_EVIDENCE}/differential_sqlite_summary.txt")
    set_tests_properties(security.differential.sqlite PROPERTIES LABELS "security;sqlite;differential")

    orm_migration_differential(sqlite orm::sqlite sqlite)
else()
    message(STATUS "ORM security: runtime tests need ORM_ENABLE_SQLITE=ON, skipped.")
endif()

# ── 3. differential tests against live servers ───────────────────────────────
#  One harness per backend, registered only when its live connector was built.
#  They need a running server, reached through the same ORM_<BACKEND>_* variables
#  as tests/integration. docker/docker-compose.yml provides them in CI.
#  A server harness fails only when a payload has an effect through the library.
#  If the naive path is never compromised it passes and the summary says the
#  comparison is inconclusive.
foreach(_b postgresql mysql mongodb redis cassandra neo4j)
    if(TARGET orm::${_b}_live)
        add_executable(security_differential_${_b} runtime/differential_${_b}.cpp)
        target_link_libraries(security_differential_${_b} PRIVATE orm::${_b}_live)
        add_test(NAME security.differential.${_b}
            COMMAND security_differential_${_b}
                "${CMAKE_CURRENT_SOURCE_DIR}/corpus/payloads.csv"
                "${ORM_SEC_EVIDENCE}/differential_${_b}.csv"
                "${ORM_SEC_EVIDENCE}/differential_${_b}_summary.txt")
        set_tests_properties(security.differential.${_b} PROPERTIES
            LABELS "security;${_b};differential;server"
            RESOURCE_LOCK "${_b}_server"
            TIMEOUT 900)
    else()
        message(STATUS "ORM security: ${_b} differential skipped, orm::${_b}_live is not built.")
    endif()
endforeach()

foreach(_b postgresql mysql)
    if(TARGET orm::${_b}_live)
        orm_migration_differential(${_b} orm::${_b}_live "${_b};server")
    endif()
endforeach()

# ── 4. layout of the thesis PDF ──────────────────────────────────────────────
#  A clean LaTeX build says nothing about a page that holds one stranded line.
#  Needs Python with PyMuPDF. Registered only where that is present.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    execute_process(COMMAND "${Python3_EXECUTABLE}" -c "import fitz"
                    RESULT_VARIABLE _orm_no_fitz OUTPUT_QUIET ERROR_QUIET)
    if(_orm_no_fitz EQUAL 0)
        set(_orm_doc_tools "${PROJECT_SOURCE_DIR}/doc/mcspc/tools")
        add_test(NAME thesis.layout.selftest
            COMMAND "${Python3_EXECUTABLE}" -m unittest test_check_layout
            WORKING_DIRECTORY "${_orm_doc_tools}")
        add_test(NAME thesis.layout
            COMMAND "${Python3_EXECUTABLE}" "${_orm_doc_tools}/check_layout.py"
                    "${PROJECT_SOURCE_DIR}/doc/mcspc/bg/Main.pdf")
        set_tests_properties(thesis.layout.selftest thesis.layout PROPERTIES LABELS "thesis")
    else()
        message(STATUS "ORM security: thesis layout check needs PyMuPDF, skipped.")
    endif()
endif()
