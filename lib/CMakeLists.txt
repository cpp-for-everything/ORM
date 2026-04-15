cmake_minimum_required(VERSION 3.20)

include(FetchContent)

# ── Boost.PFR (header-only, no full Boost required) ──────────────────────────
FetchContent_Declare(
    boost_pfr
    URL https://github.com/boostorg/pfr/archive/refs/tags/2.2.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(boost_pfr)

# ── ORM interface library ─────────────────────────────────────────────────────
add_library(orm INTERFACE)
add_library(orm::orm ALIAS orm)

target_include_directories(orm INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(orm INTERFACE cxx_std_23)

target_compile_options(orm INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# ── Link Boost.PFR ────────────────────────────────────────────────────────────
target_link_libraries(orm INTERFACE boost_pfr)

# ── C++26 reflection detection (ORM_HAS_REFLECTION) ──────────────────────────
# GCC 16 / Bloomberg Clang fork define __cpp_impl_reflection.
# We pass ORM_HAS_REFLECTION=1 when detected so headers can #ifdef on it.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-std=c++26")
check_cxx_source_compiles("
    int main() {
#ifndef __cpp_impl_reflection
        static_assert(false, \"no reflection\");
#endif
        return 0;
    }
" ORM_CPP26_REFLECTION_AVAILABLE)
unset(CMAKE_REQUIRED_FLAGS)

# ── C++23 utc_clock detection (ORM_HAS_UTC_CLOCK) ───────────────────────────
# Some standard library implementations used with C++23 do not yet expose
# std::chrono::utc_clock. Detect the feature at configure time and provide a
# compile-time macro so headers and tests can stay portable.
set(CMAKE_REQUIRED_FLAGS "-std=c++23")
check_cxx_source_compiles("
    #include <chrono>
    int main() {
        using timestamp_type = std::chrono::utc_clock::time_point;
        (void)sizeof(timestamp_type);
        return 0;
    }
" ORM_CPP23_UTC_CLOCK_AVAILABLE)
unset(CMAKE_REQUIRED_FLAGS)

if(ORM_CPP26_REFLECTION_AVAILABLE)
    message(STATUS "ORM: C++26 static reflection available (^^operator path active)")
    target_compile_definitions(orm INTERFACE ORM_HAS_REFLECTION=1)
else()
    message(STATUS "ORM: C++26 reflection not available — using Boost.PFR fallback")
    target_compile_definitions(orm INTERFACE ORM_HAS_REFLECTION=0)
endif()

if(ORM_CPP23_UTC_CLOCK_AVAILABLE)
    message(STATUS "ORM: std::chrono::utc_clock available")
    target_compile_definitions(orm INTERFACE ORM_HAS_UTC_CLOCK=1)
else()
    message(STATUS "ORM: std::chrono::utc_clock not available — using system_clock fallback for orm::timestamp")
    target_compile_definitions(orm INTERFACE ORM_HAS_UTC_CLOCK=0)
endif()

# ── MockDB compiled connector ─────────────────────────────────────────────
add_subdirectory(src/ORM/db/connectors/MockDB)

# ── MySQL connector ────────────────────────────────────────────────────────
add_subdirectory(src/ORM/db/connectors/MySQLDB)

# ── PostgreSQL connector ───────────────────────────────────────────────────
add_subdirectory(src/ORM/db/connectors/PostgreSQLDB)

# ── MongoDB connector ──────────────────────────────────────────────────────
add_subdirectory(src/ORM/db/connectors/MongoDB)

# ── Redis connector ────────────────────────────────────────────────────────
add_subdirectory(src/ORM/db/connectors/RedisDB)

# ── Cassandra connector ────────────────────────────────────────────────────
add_subdirectory(src/ORM/db/connectors/CassandraDB)

# ── Neo4j connector ────────────────────────────────────────────────────────
add_subdirectory(src/ORM/db/connectors/Neo4jDB)

# ── Thread safety infrastructure ──────────────────────────────────────────
add_subdirectory(src/ORM/db/connectors/ThreadSafety)

# ── Wire-protocol optimisations ───────────────────────────────────────────
add_subdirectory(src/ORM/db/connectors/WireProtocol)

# ── Async infrastructure ─────────────────────────────────────────────────────
add_subdirectory(src/ORM/async)

# ── Schema migration tool ─────────────────────────────────────────────────────
add_subdirectory(src/ORM/db/migration)

# ── SQLite connector ──────────────────────────────────────────────────────
add_subdirectory(src/ORM/db/connectors/SQLiteDB)
