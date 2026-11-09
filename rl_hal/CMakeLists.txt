cmake_minimum_required(VERSION 3.16)
project(rl_hal VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ament_cmake QUIET)

add_library(rl_hal STATIC
  src/serial_frame.cpp
  src/sim_motor_driver.cpp
)
target_compile_features(rl_hal PUBLIC cxx_std_17)
target_include_directories(rl_hal
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_options(rl_hal PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(rl_hal PROPERTIES POSITION_INDEPENDENT_CODE ON)

include(CTest)
if(BUILD_TESTING)
  include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/rl_gtest.cmake)
  rl_import_gtest(RL_GTEST_MAIN)

  add_executable(serial_frame_test test/test_serial_frame.cpp)
  target_link_libraries(serial_frame_test PRIVATE rl_hal ${RL_GTEST_MAIN})
  target_compile_options(serial_frame_test PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME serial_frame_test COMMAND serial_frame_test)
endif()

install(DIRECTORY include/ DESTINATION include)

if(ament_cmake_FOUND)
  install(TARGETS rl_hal
    EXPORT export_rl_hal
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
  )
  ament_export_targets(export_rl_hal HAS_LIBRARY_TARGET)
  ament_package()
else()
  install(TARGETS rl_hal
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
  )
endif()
