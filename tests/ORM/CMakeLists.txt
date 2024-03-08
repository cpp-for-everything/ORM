cmake_minimum_required(VERSION 3.14)

include(CTest)
enable_testing()

add_executable(Webframe_ORM_Tests main.cpp)
target_link_libraries(Webframe_ORM_Tests PUBLIC Webframe::ORM)

add_test(NAME Usage COMMAND Webframe_ORM_Tests)
set_tests_properties(Usage
  PROPERTIES 
  PASS_REGULAR_EXPRESSION "+ [0-9]+)"
  FAIL_REGULAR_EXPRESSION "x [0-9]+)"
)
