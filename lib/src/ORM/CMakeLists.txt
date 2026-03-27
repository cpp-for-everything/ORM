cmake_minimum_required (VERSION 3.14)

set(SRCS 
    ${CMAKE_CURRENT_SOURCE_DIR}/ORM.cpp
    #${CMAKE_CURRENT_SOURCE_DIR}/tools/property.cpp
)

add_library(Webframe_ORM ${SRCS})
target_include_directories(Webframe_ORM PUBLIC ${CMAKE_SOURCE_DIR}/lib/include PUBLIC ${CMAKE_SOURCE_DIR}/lib/src)
set_target_properties(Webframe_ORM PROPERTIES LINKER_LANGUAGE CXX)
