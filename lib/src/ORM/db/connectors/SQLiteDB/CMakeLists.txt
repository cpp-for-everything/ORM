# SQLite Database Connector
# Uses system-installed SQLite3 library

if(NOT ORM_ENABLE_SQLITE)
    return()
endif()

find_package(SQLite3 REQUIRED)

if(SQLite3_FOUND)
    message(STATUS "ORM: SQLite3 found — building orm::sqlite connector")
    
    add_library(orm_sqlite INTERFACE)
    add_library(orm::sqlite ALIAS orm_sqlite)
    
    # Handle both SQLite3::SQLite3 target and variables
    if(TARGET SQLite3::SQLite3)
        target_link_libraries(orm_sqlite INTERFACE orm::orm SQLite3::SQLite3)
    else()
        target_link_libraries(orm_sqlite INTERFACE orm::orm ${SQLite3_LIBRARIES})
        target_include_directories(orm_sqlite INTERFACE ${SQLite3_INCLUDE_DIRS})
    endif()
    
    target_include_directories(orm_sqlite INTERFACE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/include>
    )
else()
    message(FATAL_ERROR "ORM: SQLite3 not found but ORM_ENABLE_SQLITE=ON")
endif()
