cmake_minimum_required (VERSION 3.14)

SET(SOURCE_COMPONENTS
	table
	limits
	ORM-version
	details/warnings
	details/orm_tuple
	details/fundamental_type
	details/member_pointer
	details/result_type
	details/string_literal
	CRUD/utils
	CRUD/select
	CRUD/insert
	db/connectors/interface
	db/connectors/MockDB/data
	db/connectors/MockDB/init
	rules      
	fields          
)

SET(HEADER_COMPONENTS 
	rules         
	fields          
	details/string_literal  
	details/result_type     
	details/fundamental_type
	details/member_pointer  
	details/warnings        
	details/select          
	limits          
	db/connectors/interface       
	table           
)

foreach(component ${HEADER_COMPONENTS})
	LIST(APPEND ORM_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/ORM/${component}.hpp)
endforeach()

foreach(component ${SOURCE_COMPONENTS})
	string(REPLACE "/" "_" lib_name ${component})

	add_library(Webframe_ORM_${lib_name} OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/src/ORM/${component}.cpp)
	target_include_directories(Webframe_ORM_${lib_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/pfr/include")
	set_target_properties(Webframe_ORM_${lib_name} PROPERTIES LINKER_LANGUAGE CXX)

	LIST(APPEND libs Webframe_ORM_${lib_name})
	LIST(APPEND ORM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/ORM/${component}.cpp)
endforeach()

add_library(Webframe_ORM STATIC src/ORM/ORM.cpp)
target_include_directories(Webframe_ORM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/pfr/include")
set_target_properties(Webframe_ORM PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(Webframe_ORM PUBLIC ${libs})

install(TARGETS Webframe_ORM DESTINATION ${PROJECT_BINARY_DIR})

add_library(Webframe::ORM ALIAS Webframe_ORM)
