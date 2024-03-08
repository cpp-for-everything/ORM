cmake_minimum_required(VERSION 3.14)

add_executable(Webframe_ORM_Tests main.cpp)
target_link_libraries(Webframe_ORM_Tests PUBLIC Webframe::ORM)

add_test(NAME Webframe-ORM-Tests COMMAND Webframe_ORM_Tests)