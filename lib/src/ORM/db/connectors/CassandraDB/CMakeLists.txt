add_library(orm_cassandra INTERFACE)
add_library(orm::cassandra ALIAS orm_cassandra)
target_link_libraries(orm_cassandra INTERFACE orm::orm)

if(ORM_ENABLE_CASSANDRA)
    find_package(Cassandra REQUIRED)
    
    if(Cassandra_FOUND)
        add_library(orm_cassandra_live INTERFACE)
        add_library(orm::cassandra_live ALIAS orm_cassandra_live)
        target_link_libraries(orm_cassandra_live INTERFACE 
            orm::orm 
            Cassandra::cassandra
        )
        target_compile_definitions(orm_cassandra_live INTERFACE ORM_CASSANDRA_LIVE_AVAILABLE=1)
        target_include_directories(orm_cassandra_live INTERFACE
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/include>
        )
        set(ORM_CASSANDRA_LIVE_ENABLED TRUE CACHE INTERNAL "")
        message(STATUS "ORM: cassandra-cpp-driver found")
    endif()
endif()
