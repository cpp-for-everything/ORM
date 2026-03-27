cmake_minimum_required (VERSION 3.14)

add_subdirectory(src/ORM)

install(TARGETS Webframe_ORM DESTINATION ${PROJECT_BINARY_DIR})

add_library(Webframe::ORM ALIAS Webframe_ORM)
