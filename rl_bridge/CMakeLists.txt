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
  target_link_libraries(bridge_node rl_transport::rl_transport)
elseif(TARGET rl_transport)
  target_link_libraries(bridge_node rl_transport)
else()
  message(FATAL_ERROR "find_package(rl_transport) 没有提供库目标")
endif()

install(TARGETS bridge_node DESTINATION lib/${PROJECT_NAME})

ament_package()
