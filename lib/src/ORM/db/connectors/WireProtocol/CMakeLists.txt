add_library(orm_wire_protocol INTERFACE)
add_library(orm::wire_protocol ALIAS orm_wire_protocol)
target_link_libraries(orm_wire_protocol INTERFACE orm::orm)
