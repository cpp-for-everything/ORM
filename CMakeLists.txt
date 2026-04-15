cmake_minimum_required(VERSION 3.20)
project(ORM VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add custom CMake modules directory
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ── Live connector opt-in options ──────────────────────────────────────────────
option(ORM_ENABLE_MYSQL      "Build live MySQL connector (requires libmysqlclient)"      OFF)
option(ORM_ENABLE_POSTGRESQL "Build live PostgreSQL connector (requires libpq)"          OFF)
option(ORM_ENABLE_MONGODB    "Build live MongoDB connector (requires mongo-c-driver)"    OFF)
option(ORM_ENABLE_REDIS      "Build live Redis connector (requires hiredis)"             OFF)
option(ORM_ENABLE_CASSANDRA  "Build live Cassandra connector (requires cassandra-cpp-driver)" OFF)
option(ORM_ENABLE_NEO4J      "Build live Neo4j connector (requires libneo4j-client)"     OFF)
option(ORM_ENABLE_SQLITE     "Build live SQLite connector (requires libsqlite3)"         OFF)

# ── Build strategy option ──────────────────────────────────────────────────────
option(ORM_USE_SYSTEM_LIBS   "Prefer system-installed libraries over submodules"         OFF)

enable_testing()

add_subdirectory(lib)
add_subdirectory(tests)

option(ORM_ENABLE_BENCHMARKS "Build benchmark executables (requires Google Benchmark)" OFF)
if(ORM_ENABLE_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
