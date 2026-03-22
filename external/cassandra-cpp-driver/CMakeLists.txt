cmake_minimum_required(VERSION 3.20)

# ── cassandra-cpp-driver — built from submodule ───────────────────────────────
# Submodule: external/cassandra-cpp-driver/cassandra-cpp-driver (apache v2.17.1)
# Builds cassandra_static target. Header: cassandra.h

set(CASS_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(CASS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CASS_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(CASS_BUILD_SHARED   OFF CACHE BOOL "" FORCE)
set(CASS_BUILD_STATIC   ON  CACHE BOOL "" FORCE)
set(CASS_USE_STATIC_LIBS ON  CACHE BOOL "" FORCE)
set(CASS_USE_OPENSSL    ON  CACHE BOOL "" FORCE)
set(CASS_USE_ZLIB       OFF CACHE BOOL "" FORCE)

# Prevent format target collision by tricking ClangFormat.cmake into failing
set(CLANG_FORMAT_EXE_NAME "dummy-clang-format-disable")
unset(CLANG_FORMAT_EXE CACHE)

# Alternatively, intercept include(ClangFormat) with a dummy module
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake_dummy")

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/cassandra-cpp-driver
    ${CMAKE_CURRENT_BINARY_DIR}/cassandra-build
    EXCLUDE_FROM_ALL
)

set(ORM_CASSANDRA_HEADERS_FOUND TRUE CACHE INTERNAL "")
message(STATUS "ORM: cassandra-cpp-driver built from submodule")
