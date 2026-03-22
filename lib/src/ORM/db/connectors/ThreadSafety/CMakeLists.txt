add_library(orm_thread_safety INTERFACE)
add_library(orm::thread_safety ALIAS orm_thread_safety)
target_link_libraries(orm_thread_safety INTERFACE orm::orm)
