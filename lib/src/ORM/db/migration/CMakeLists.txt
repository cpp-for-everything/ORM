add_library(orm_migration INTERFACE)
add_library(orm::migration ALIAS orm_migration)
target_link_libraries(orm_migration INTERFACE orm::orm)
