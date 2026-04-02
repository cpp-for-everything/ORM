# FindCassandra.cmake
# Finds the Datastax cassandra-cpp-driver (libcassandra).
#
# Result variables:
#   Cassandra_FOUND
#   Cassandra_INCLUDE_DIRS
#   Cassandra_LIBRARIES
#
# Imported targets:
#   Cassandra::cassandra

find_path(Cassandra_INCLUDE_DIR
    NAMES cassandra.h
    PATHS
        /usr/local/include
        /usr/include
)

find_library(Cassandra_LIBRARY
    NAMES cassandra
    PATHS
        /usr/local/lib
        /usr/local/lib64
        /usr/lib
        /usr/lib64
        /usr/lib/x86_64-linux-gnu
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Cassandra
    REQUIRED_VARS Cassandra_LIBRARY Cassandra_INCLUDE_DIR
)

if(Cassandra_FOUND)
    set(Cassandra_INCLUDE_DIRS "${Cassandra_INCLUDE_DIR}")
    set(Cassandra_LIBRARIES    "${Cassandra_LIBRARY}")

    if(NOT TARGET Cassandra::cassandra)
        add_library(Cassandra::cassandra UNKNOWN IMPORTED)
        set_target_properties(Cassandra::cassandra PROPERTIES
            IMPORTED_LOCATION             "${Cassandra_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${Cassandra_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(Cassandra_INCLUDE_DIR Cassandra_LIBRARY)
