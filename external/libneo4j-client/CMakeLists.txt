cmake_minimum_required(VERSION 3.20)

# libneo4j-client: C client library for Neo4j using the Bolt protocol.
# We use the actively maintained fork by majensen which supports Neo4j 4.x/5.x.
#
# The library requires autoconf/automake on Linux/macOS. On Windows it is not
# natively supported without WSL or MSYS2.

include(ExternalProject)

set(NEO4J_CLIENT_VERSION "4.0.1")
set(NEO4J_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/neo4j-client-install")

set(ORM_NEO4J_HEADERS_FOUND FALSE CACHE INTERNAL "")

if(WIN32)
    # On Windows, attempt to find a pre-built installation (e.g. via MSYS2).
    find_path(NEO4J_CLIENT_INCLUDE_DIR neo4j-client.h
        PATHS
            "C:/msys64/mingw64/include"
            "C:/msys64/ucrt64/include"
    )
    find_library(NEO4J_CLIENT_LIBRARY
        NAMES neo4j-client
        PATHS
            "C:/msys64/mingw64/lib"
            "C:/msys64/ucrt64/lib"
    )

    if(NEO4J_CLIENT_INCLUDE_DIR AND NEO4J_CLIENT_LIBRARY)
        add_library(neo4j::client IMPORTED UNKNOWN GLOBAL)
        set_target_properties(neo4j::client PROPERTIES
            IMPORTED_LOCATION "${NEO4J_CLIENT_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${NEO4J_CLIENT_INCLUDE_DIR}"
        )
        set(ORM_NEO4J_HEADERS_FOUND TRUE CACHE INTERNAL "")
        message(STATUS "ORM: libneo4j-client found at ${NEO4J_CLIENT_LIBRARY}")
    else()
        message(WARNING
            "ORM: libneo4j-client not found on Windows. "
            "Install via MSYS2: pacman -S mingw-w64-x86_64-neo4j-client")
        add_library(neo4j::client INTERFACE IMPORTED GLOBAL)
    endif()
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(NEO4J_CLIENT QUIET libneo4j-client)
    endif()

    if(NEO4J_CLIENT_FOUND)
        add_library(neo4j::client INTERFACE IMPORTED GLOBAL)
        target_include_directories(neo4j::client INTERFACE ${NEO4J_CLIENT_INCLUDE_DIRS})
        target_link_libraries(neo4j::client INTERFACE ${NEO4J_CLIENT_LINK_LIBRARIES})
        set(ORM_NEO4J_HEADERS_FOUND TRUE CACHE INTERNAL "")
        message(STATUS "ORM: libneo4j-client found via pkg-config")
    else()
        # Build from source via ExternalProject
        file(MAKE_DIRECTORY "${NEO4J_INSTALL_DIR}/include")
        file(MAKE_DIRECTORY "${NEO4J_INSTALL_DIR}/lib")

        ExternalProject_Add(
            libneo4j_client_external
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libneo4j-client
            CONFIGURE_COMMAND autoreconf -fi <SOURCE_DIR>
                      COMMAND <SOURCE_DIR>/configure
                                  --prefix=${NEO4J_INSTALL_DIR}
                                  --disable-shared
                                  --enable-static
                                  --without-tls
            BUILD_COMMAND     make -j4
            INSTALL_COMMAND   make install
            BUILD_IN_SOURCE   FALSE
            LOG_CONFIGURE     TRUE
            LOG_BUILD         TRUE
            LOG_INSTALL       TRUE
        )

        add_library(neo4j::client STATIC IMPORTED GLOBAL)
        add_dependencies(neo4j::client libneo4j_client_external)
        set_target_properties(neo4j::client PROPERTIES
            IMPORTED_LOCATION "${NEO4J_INSTALL_DIR}/lib/libneo4j-client.a"
            INTERFACE_INCLUDE_DIRECTORIES "${NEO4J_INSTALL_DIR}/include"
        )
        set(ORM_NEO4J_HEADERS_FOUND TRUE CACHE INTERNAL "")
        message(STATUS "ORM: libneo4j-client will be built from source")
    endif()
endif()
