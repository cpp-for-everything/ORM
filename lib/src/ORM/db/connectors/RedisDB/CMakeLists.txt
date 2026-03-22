add_library(orm_redis INTERFACE)
add_library(orm::redis ALIAS orm_redis)
target_link_libraries(orm_redis INTERFACE orm::orm)

if(ORM_ENABLE_REDIS)
    find_package(Hiredis REQUIRED)
    
    if(Hiredis_FOUND)
        add_library(orm_redis_live INTERFACE)
        add_library(orm::redis_live ALIAS orm_redis_live)
        target_link_libraries(orm_redis_live INTERFACE 
            orm::orm 
            Hiredis::hiredis
        )
        target_compile_definitions(orm_redis_live INTERFACE ORM_REDIS_LIVE_AVAILABLE=1)
        target_include_directories(orm_redis_live INTERFACE
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/include>
        )
        set(ORM_REDIS_LIVE_ENABLED TRUE CACHE INTERNAL "")
        message(STATUS "ORM: hiredis found")
    endif()
endif()
