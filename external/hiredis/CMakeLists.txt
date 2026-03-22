cmake_minimum_required(VERSION 3.20)

# ── hiredis — built from submodule external/hiredis/hiredis ───────────────
# The submodule is hiredis v1.3.0 which defines hiredis::hiredis target.

set(DISABLE_TESTS   ON  CACHE BOOL "" FORCE)
set(ENABLE_SSL      OFF CACHE BOOL "" FORCE)
set(ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/hiredis
    ${CMAKE_CURRENT_BINARY_DIR}/hiredis-build
    EXCLUDE_FROM_ALL
)

target_include_directories(hiredis INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

set(ORM_REDIS_HEADERS_FOUND TRUE CACHE INTERNAL "")
message(STATUS "ORM: hiredis built from submodule")
