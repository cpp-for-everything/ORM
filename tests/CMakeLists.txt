cmake_minimum_required(VERSION 3.20)

include(FetchContent)

# MSVC + GoogleTest default to different CRTs (/MT vs /MD) unless this is set.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
# Discover at ctest time so Windows DLL search paths (e.g. sqlite3) are in place.
set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

add_subdirectory(unit)
add_subdirectory(integration)
