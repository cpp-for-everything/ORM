add_library(orm_mysql INTERFACE)
add_library(orm::mysql ALIAS orm_mysql)
target_link_libraries(orm_mysql INTERFACE orm::orm)

if(ORM_ENABLE_MYSQL)
    if(ORM_USE_SYSTEM_LIBS)
        # Use system-installed MySQL client library
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(MYSQL REQUIRED mysqlclient)
    else()
        # Use submodule or fetch from GitHub
        message(FATAL_ERROR "MySQL client library submodule build not yet implemented")
    endif()

    add_library(orm_mysql_live INTERFACE)
    add_library(orm::mysql_live ALIAS orm_mysql_live)
    target_include_directories(orm_mysql_live INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
    )

    if(ORM_USE_SYSTEM_LIBS)
        target_link_libraries(orm_mysql_live INTERFACE
            orm::orm
            ${MYSQL_LIBRARIES}
        )
        target_include_directories(orm_mysql_live INTERFACE
            ${MYSQL_INCLUDE_DIRS}
        )
    else()
        # Link against built library
        target_link_libraries(orm_mysql_live INTERFACE orm::orm mysqlclient)
    endif()

    target_compile_definitions(orm_mysql_live INTERFACE ORM_MYSQL_LIVE_AVAILABLE=1)
    set(ORM_MYSQL_LIVE_ENABLED TRUE CACHE INTERNAL "")
    message(STATUS "ORM: MySQL client library found")
endif()
