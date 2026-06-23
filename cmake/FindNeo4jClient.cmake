# FindNeo4jClient.cmake
# Finds libneo4j-client (cleishm/libneo4j-client).
#
# The upstream project ships a pkg-config file `neo4j-client.pc` but no
# CMake config package, so we discover it via pkg-config with a fallback
# to a direct path search (for installs that did not include pkg-config
# integration).
#
# Result variables:
#   Neo4jClient_FOUND
#   Neo4jClient_INCLUDE_DIRS
#   Neo4jClient_LIBRARIES
#
# Imported targets:
#   Neo4jClient::neo4j

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_NEO4J QUIET neo4j-client)
endif()

find_path(Neo4jClient_INCLUDE_DIR
    NAMES neo4j-client.h
    HINTS ${PC_NEO4J_INCLUDE_DIRS}
    PATHS
        /usr/local/include
        /usr/include
)

find_library(Neo4jClient_LIBRARY
    NAMES neo4j-client
    HINTS ${PC_NEO4J_LIBRARY_DIRS}
    PATHS
        /usr/local/lib
        /usr/local/lib64
        /usr/lib
        /usr/lib64
        /usr/lib/x86_64-linux-gnu
        /usr/lib/aarch64-linux-gnu
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Neo4jClient
    REQUIRED_VARS Neo4jClient_LIBRARY Neo4jClient_INCLUDE_DIR
)

if(Neo4jClient_FOUND)
    set(Neo4jClient_INCLUDE_DIRS "${Neo4jClient_INCLUDE_DIR}")
    set(Neo4jClient_LIBRARIES    "${Neo4jClient_LIBRARY}")

    if(NOT TARGET Neo4jClient::neo4j)
        add_library(Neo4jClient::neo4j UNKNOWN IMPORTED)
        set_target_properties(Neo4jClient::neo4j PROPERTIES
            IMPORTED_LOCATION             "${Neo4jClient_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${Neo4jClient_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(Neo4jClient_INCLUDE_DIR Neo4jClient_LIBRARY)
