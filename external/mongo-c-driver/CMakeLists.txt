cmake_minimum_required(VERSION 3.20)

# ── mongo-cxx-driver — built from submodule external/mongo-c-driver/mongo-cxx-driver (r4.2.0) ──
# Exports: mongocxx_static, bsoncxx_static (and their :: aliases)
# Requires: system mongoc-1.0 and libbson-1.0
#   Linux/macOS: apt install libmongoc-dev libbson-dev | brew install mongo-c-driver
#   Windows MSYS2: pacman -S mingw-w64-x86_64-libmongoc
#   Windows vcpkg: vcpkg install mongo-c-driver

set(MONGOCXX_BUILD_STATIC  ON  CACHE BOOL "" FORCE)
set(MONGOCXX_BUILD_SHARED  OFF CACHE BOOL "" FORCE)
set(BSONCXX_BUILD_STATIC   ON  CACHE BOOL "" FORCE)
set(BSONCXX_BUILD_SHARED   OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTS           OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING          OFF CACHE BOOL "" FORCE)
set(ENABLE_EXAMPLES        OFF CACHE BOOL "" FORCE)
set(ENABLE_UNINSTALL       OFF CACHE BOOL "" FORCE)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/mongo-cxx-driver
    ${CMAKE_CURRENT_BINARY_DIR}/mongo-cxx-driver-build
    EXCLUDE_FROM_ALL
)

set(ORM_MONGODB_HEADERS_FOUND TRUE CACHE INTERNAL "")
message(STATUS "ORM: mongo-cxx-driver built from submodule")
