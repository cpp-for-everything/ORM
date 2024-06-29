# ORM main CMakeLists with all sub projects
cmake_minimum_required (VERSION 3.14)

project (WebframeORM
    VERSION 2.0.0.0 
    DESCRIPTION ""
    HOMEPAGE_URL ""
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
cmake_policy(SET CMP0057 NEW)

if (MSVC)
    # warning level 4 and all warnings as errors
    add_compile_options(/W4 /WX)
else()
    # lots of warnings and all warnings as errors
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

option(USE_MYSQL "Use MySQL Client" OFF)
option(RUN_TESTS "Run unit & integration tests" ON)

configure_file(ORM.hpp.in ${CMAKE_CURRENT_SOURCE_DIR}/lib/include/ORM/ORM-version.hpp)

# Include sub-projects.
add_subdirectory("lib")
list(APPEND EXTRA_LIBS Webframe::ORM)
list(APPEND EXTRA_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/lib/include")

if(USE_MYSQL)
    execute_process(COMMAND mysql_config --cflags
        OUTPUT_VARIABLE MYSQL_CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND mysql_config --libs
        OUTPUT_VARIABLE MYSQL_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)

    add_compile_options(${MYSQL_CFLAGS})
    add_definitions(-DUSE_MYSQL)
    list(APPEND EXTRA_LIBS ${MYSQL_LIBS})
endif()

if(RUN_TESTS)
    add_subdirectory("tests")
endif()

# Generate example source code
# add_executable(Webframe_ORM_Example example/ORM.cpp)
# target_compile_definitions(Webframe_ORM_Example PUBLIC DB_USERNAME="root" DB_PASSWORD="test")
# target_link_libraries(Webframe_ORM_Example PUBLIC ${EXTRA_LIBS})
# target_include_directories(Webframe_ORM_Example PUBLIC
#                             ${EXTRA_INCLUDES}
#                             ${PROJECT_BINARY_DIR}
#                           )
# add_executable(Webframe::ORM::Example ALIAS Webframe_ORM_Example)

#include(CPack)
