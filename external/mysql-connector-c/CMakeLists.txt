cmake_minimum_required(VERSION 3.20)

# ── mysql-connector-cpp — built from submodule external/mysql-connector-c/mysql-connector-cpp (v9.6.0) ──
# Exports: connector target (the X DevAPI / mysqlx C++ connector)
# Requires: system OpenSSL
#   Linux/macOS: apt install libssl-dev | brew install openssl
#   Windows MSYS2: pacman -S mingw-w64-x86_64-openssl
#   Windows vcpkg: vcpkg install openssl

set(WITH_TESTS    OFF CACHE BOOL "" FORCE)
set(WITH_DOC      OFF CACHE BOOL "" FORCE)
set(WITH_JDBC     OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC  ON  CACHE BOOL "" FORCE)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/mysql-connector-cpp
    ${CMAKE_CURRENT_BINARY_DIR}/mysql-connector-cpp-build
    EXCLUDE_FROM_ALL
)

set(ORM_MYSQL_HEADERS_FOUND TRUE CACHE INTERNAL "")
message(STATUS "ORM: mysql-connector-cpp built from submodule")
