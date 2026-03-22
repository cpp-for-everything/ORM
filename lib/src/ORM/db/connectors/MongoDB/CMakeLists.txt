add_library(orm_mongodb INTERFACE)
add_library(orm::mongodb ALIAS orm_mongodb)
target_link_libraries(orm_mongodb INTERFACE orm::orm)

if(ORM_ENABLE_MONGODB)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MONGOC REQUIRED libmongoc-1.0)
    pkg_check_modules(BSON REQUIRED libbson-1.0)
    
    if(MONGOC_FOUND AND BSON_FOUND)
        add_library(orm_mongodb_live INTERFACE)
        add_library(orm::mongodb_live ALIAS orm_mongodb_live)
        target_link_libraries(orm_mongodb_live INTERFACE 
            orm::orm 
            ${MONGOC_LIBRARIES}
            ${BSON_LIBRARIES}
        )
        target_include_directories(orm_mongodb_live INTERFACE
            ${MONGOC_INCLUDE_DIRS}
            ${BSON_INCLUDE_DIRS}
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/include>
        )
        target_compile_definitions(orm_mongodb_live INTERFACE ORM_MONGODB_LIVE_AVAILABLE=1)
        set(ORM_MONGODB_LIVE_ENABLED TRUE CACHE INTERNAL "")
        message(STATUS "ORM: libmongoc (MongoDB C driver) found")
    endif()
endif()
