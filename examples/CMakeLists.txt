# ============================================================================
#  examples/CMakeLists.txt --- standalone build за всички примери
# ============================================================================
#  Автоматично открива всички *.cpp файлове и създава по един executable
#  за всеки. Изнася compile_commands.json (за clangd LSP).
#
#  Употреба:
#    cmake -G Ninja -B build
#    ninja -C build
#  За да изпробваш конкретен пример:
#    ./build/01_capability_gating   ./build/04_param_arity
#    ./build/02_query_anatomy       ./build/05_backend_shape
#    ./build/03_type_safety         ./build/06_relationships   ./build/08_async_task
#  (07_live_relationships е живо демо през docker — виж run_demos.sh / run_live.sh)
#
#  Някои примери са умишлено „провалящи се при компилация" (демонстрация
#  на compile-time gating). Те са изключени по подразбиране от ninja и
#  трябва да бъдат активирани explicit:
#    cmake -B build -DORM_EXAMPLES_BUILD_FAILING=ON
#    ninja -C build 01_capability_gating_mongodb_fails
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(orm_examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Намираме корена на ORM хранилището (1 ниво нагоре) ────────────────────
get_filename_component(ORM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
if(NOT EXISTS "${ORM_ROOT}/lib/include/ORM/ORM.hpp")
    message(FATAL_ERROR
        "Не намирам ORM/ORM.hpp в ${ORM_ROOT}/lib/include. "
        "Очаквам да съм в examples/ на ORM хранилището.")
endif()

# ── Boost.PFR ──────────────────────────────────────────────────────────────
set(_orm_pfr_candidates
    "${ORM_ROOT}/build-wsl-uring/_deps/boost_pfr-src/include"
    "${ORM_ROOT}/build/_deps/boost_pfr-src/include"
    "${ORM_ROOT}/build-win/_deps/boost_pfr-src/include"
    "${ORM_ROOT}/build-win-clang/_deps/boost_pfr-src/include"
    # Pre-cached в Docker образа на демото (виж doc/v2/bg_presentation/demo/Dockerfile)
    "/opt/boost_pfr/include")

set(_orm_pfr_found "")
foreach(_cand ${_orm_pfr_candidates})
    if(EXISTS "${_cand}/boost/pfr.hpp")
        set(_orm_pfr_found "${_cand}")
        break()
    endif()
endforeach()

if(_orm_pfr_found)
    message(STATUS "ORM examples: Boost.PFR намерен в ${_orm_pfr_found}")
    add_library(orm_examples_pfr INTERFACE)
    target_include_directories(orm_examples_pfr INTERFACE "${_orm_pfr_found}")
else()
    message(STATUS "ORM examples: Boost.PFR не е намерен локално --- fetch.")
    include(FetchContent)
    FetchContent_Declare(boost_pfr
        URL https://github.com/boostorg/pfr/archive/refs/tags/2.2.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(boost_pfr)
    add_library(orm_examples_pfr INTERFACE)
    target_link_libraries(orm_examples_pfr INTERFACE boost_pfr)
endif()

# ── Общи флагове ──────────────────────────────────────────────────────────
# std::chrono::utc_clock е наличен на libstdc++ (Linux/g++) но не на libc++ (macOS).
if(APPLE)
    set(_orm_has_utc 0)
else()
    set(_orm_has_utc 1)
endif()

function(_orm_example_setup target)
    target_compile_definitions(${target} PRIVATE
        ORM_HAS_REFLECTION=0
        ORM_HAS_UTC_CLOCK=${_orm_has_utc})
    target_include_directories(${target} PRIVATE
        "${ORM_ROOT}/lib/include")
    target_link_libraries(${target} PRIVATE orm_examples_pfr)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -fdiagnostics-color=always)
    endif()
endfunction()

# ── Автоматично откриване на всички *.cpp файлове ─────────────────────────
file(GLOB ORM_EXAMPLE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(SORT ORM_EXAMPLE_SOURCES)

foreach(src ${ORM_EXAMPLE_SOURCES})
    get_filename_component(name "${src}" NAME_WE)

    # 07_live_relationships е ЖИВО демо: нужни са клиентски библиотеки (libpq /
    # libmariadb / libmongoc), вдигната база и backend макрос (-DORM_DEMO_*).
    # Строи се само през docker-compose (виж 07_live_relationships/run_live.sh
    # или examples/run_demos.sh 7), не през този CMake.
    if(name STREQUAL "07_live_relationships")
        continue()
    endif()

    add_executable(${name} "${src}")
    _orm_example_setup(${name})

    # 01_capability_gating отваря in-memory SQLite (sqlite3*); другите примери
    # 02–06 не пипат база и са изцяло header-only.
    if(name STREQUAL "01_capability_gating")
        target_link_libraries(${name} PRIVATE sqlite3)
    endif()
endforeach()

# ── Опция за билд на „умишлено пропадащите" варианти ──────────────────────
#  Активирай с -DORM_EXAMPLES_BUILD_FAILING=ON. Полезно за демонстрация
#  на compile-time gating: ninja ще покаже readable static_assert грешка.
option(ORM_EXAMPLES_BUILD_FAILING
    "Включи варианти, които умишлено пропадат при компилация (capability демо)"
    OFF)
if(ORM_EXAMPLES_BUILD_FAILING)
    add_executable(01_capability_gating_mongodb_fails 01_capability_gating.cpp)
    _orm_example_setup(01_capability_gating_mongodb_fails)
    target_link_libraries(01_capability_gating_mongodb_fails PRIVATE sqlite3)
    target_compile_definitions(01_capability_gating_mongodb_fails
        PRIVATE ORM_DEMO_DB_MONGODB)
    message(STATUS "ORM examples: 01_capability_gating_mongodb_fails е "
                   "активиран --- очаквайте умишлен compile error.")
endif()

# ── Копиране на compile_commands.json до source dir за clangd ─────────────
add_custom_target(orm_examples_clangd_link ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/compile_commands.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json"
    COMMENT "Копирам compile_commands.json до ${CMAKE_CURRENT_SOURCE_DIR}/ за clangd")
