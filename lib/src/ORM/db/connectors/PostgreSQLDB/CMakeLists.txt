add_library(orm_postgresql INTERFACE)
add_library(orm::postgresql ALIAS orm_postgresql)
target_link_libraries(orm_postgresql INTERFACE orm::orm)

if(ORM_ENABLE_POSTGRESQL)
    find_package(PostgreSQL REQUIRED)
    
    if(PostgreSQL_FOUND)
        add_library(orm_postgresql_live INTERFACE)
        add_library(orm::postgresql_live ALIAS orm_postgresql_live)
        target_link_libraries(orm_postgresql_live INTERFACE 
            orm::orm 
            PostgreSQL::PostgreSQL
        )
        target_compile_definitions(orm_postgresql_live INTERFACE ORM_POSTGRESQL_LIVE_AVAILABLE=1)
        target_include_directories(orm_postgresql_live INTERFACE
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/include>
        )
        set(ORM_POSTGRESQL_LIVE_ENABLED TRUE CACHE INTERNAL "")
        message(STATUS "ORM: libpq (PostgreSQL C library) found")
    endif()
endif()
