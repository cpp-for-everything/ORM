add_library(orm_neo4j INTERFACE)
add_library(orm::neo4j ALIAS orm_neo4j)
target_link_libraries(orm_neo4j INTERFACE orm::orm)

if(ORM_ENABLE_NEO4J)
    find_package(Neo4jClient REQUIRED)
    
    if(Neo4jClient_FOUND)
        add_library(orm_neo4j_live INTERFACE)
        add_library(orm::neo4j_live ALIAS orm_neo4j_live)
        target_link_libraries(orm_neo4j_live INTERFACE 
            orm::orm 
            Neo4jClient::neo4j
        )
        target_compile_definitions(orm_neo4j_live INTERFACE ORM_NEO4J_LIVE_AVAILABLE=1)
        target_include_directories(orm_neo4j_live INTERFACE
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/include>
        )
        set(ORM_NEO4J_LIVE_ENABLED TRUE CACHE INTERNAL "")
        message(STATUS "ORM: libneo4j-client found")
    endif()
endif()
