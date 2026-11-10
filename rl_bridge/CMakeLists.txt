cmake_minimum_required(VERSION 3.16)
project(rl_bridge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rl_transport REQUIRED)
find_package(Protobuf REQUIRED)

add_executable(bridge_node
  src/bridge_node.cpp
  src/bridge_main.cpp
)
target_compile_features(bridge_node PUBLIC cxx_std_17)
target_include_directories(bridge_node PRIVATE include)
target_compile_options(bridge_node PRIVATE -Wall -Wextra -Wpedantic)
ament_target_dependencies(bridge_node
  rclcpp
  rclcpp_lifecycle
  geometry_msgs
  nav_msgs
)

if(TARGET rl_transport::rl_transport)
  target_link_libraries(bridge_node rl_transport::rl_transport protobuf::libprotobuf)
elseif(TARGET rl_transport)
  target_link_libraries(bridge_node rl_transport protobuf::libprotobuf)
else()
  message(FATAL_ERROR "find_package(rl_transport) 没有提供库目标")
endif()

if(BUILD_TESTING)
  enable_testing()
  include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/rl_gtest.cmake)
  rl_import_gtest(RL_GTEST_MAIN)

  add_executable(convert_test test/test_convert.cpp)
  target_compile_features(convert_test PUBLIC cxx_std_17)
  target_include_directories(convert_test PRIVATE include)
  target_compile_options(convert_test PRIVATE -Wall -Wextra -Wpedantic)
  ament_target_dependencies(convert_test geometry_msgs nav_msgs)
  if(TARGET rl_transport::rl_transport)
    target_link_libraries(convert_test
      rl_transport::rl_transport
      protobuf::libprotobuf
      ${RL_GTEST_MAIN}
    )
  else()
    target_link_libraries(convert_test rl_transport protobuf::libprotobuf ${RL_GTEST_MAIN})
  endif()
  add_test(NAME convert_test COMMAND convert_test)
  set_tests_properties(convert_test PROPERTIES TIMEOUT 30)
endif()

install(TARGETS bridge_node DESTINATION lib/${PROJECT_NAME})

ament_package()
