add_library(orm_mockdb INTERFACE)
add_library(orm::mockdb ALIAS orm_mockdb)

target_link_libraries(orm_mockdb INTERFACE orm::orm)
