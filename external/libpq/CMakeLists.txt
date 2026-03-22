cmake_minimum_required(VERSION 3.20)

# ── libpqxx — built from submodule external/libpq/libpqxx (v8.0.0) ───────────
# Exports: libpqxx::pqxx
# Requires: system libpq (PostgreSQL client library)
#   Linux/macOS: apt install libpq-dev | brew install libpq
#   Windows MSYS2: pacman -S mingw-w64-x86_64-libpq
#   Windows vcpkg: vcpkg install libpq

set(SKIP_BUILD_TEST     ON  CACHE BOOL "" FORCE)
set(SKIP_BUILD_EXAMPLES ON  CACHE BOOL "" FORCE)
set(BUILD_DOC           OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS   OFF CACHE BOOL "" FORCE)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/libpqxx
    ${CMAKE_CURRENT_BINARY_DIR}/libpqxx-build
    EXCLUDE_FROM_ALL
)

set(ORM_POSTGRESQL_HEADERS_FOUND TRUE CACHE INTERNAL "")
message(STATUS "ORM: libpqxx built from submodule")
