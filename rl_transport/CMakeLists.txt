cmake_minimum_required(VERSION 3.16)
project(rl_transport VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)
find_package(ament_cmake QUIET)

if(NOT TARGET protobuf::protoc)
  message(FATAL_ERROR "未找到 protobuf::protoc，请安装 protobuf-compiler")
endif()
if(NOT TARGET protobuf::libprotobuf)
  message(FATAL_ERROR "未找到 protobuf::libprotobuf，请安装 libprotobuf-dev")
endif()

set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../proto)
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})

set(PROTO_BASENAMES header twist odometry heartbeat)
set(PROTO_FILES)
set(PROTO_SRCS)
set(PROTO_HDRS)
foreach(name ${PROTO_BASENAMES})
  list(APPEND PROTO_FILES ${PROTO_DIR}/${name}.proto)
  list(APPEND PROTO_SRCS ${GEN_DIR}/${name}.pb.cc)
  list(APPEND PROTO_HDRS ${GEN_DIR}/${name}.pb.h)
endforeach()

add_custom_command(
  OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
  COMMAND $<TARGET_FILE:protobuf::protoc>
          --cpp_out=${GEN_DIR}
          -I ${PROTO_DIR}
          ${PROTO_FILES}
  DEPENDS ${PROTO_FILES}
  COMMENT "生成 Protobuf 源码"
  VERBATIM
)
set_source_files_properties(${PROTO_SRCS} ${PROTO_HDRS} PROPERTIES GENERATED TRUE)
set_source_files_properties(${PROTO_SRCS} PROPERTIES COMPILE_OPTIONS "-w")

add_library(rl_transport SHARED
  src/buffer.cpp
  src/crc32.cpp
  src/event_loop.cpp
  src/uds_backend.cpp
  src/tcp_backend.cpp
  src/shm_backend.cpp
  src/factory.cpp
  ${PROTO_SRCS}
)
target_compile_features(rl_transport PUBLIC cxx_std_17)
target_include_directories(rl_transport
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${GEN_DIR}>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(rl_transport PRIVATE protobuf::libprotobuf Threads::Threads)
target_compile_options(rl_transport PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(rl_transport PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
)

function(rl_add_transport_tool target_name)
  add_executable(${target_name} examples/${target_name}.cpp)
  target_link_libraries(${target_name} PRIVATE rl_transport protobuf::libprotobuf)
  target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
endfunction()

rl_add_transport_tool(uds_pub_demo)
rl_add_transport_tool(uds_sub_demo)

include(CTest)
if(BUILD_TESTING)
  include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/rl_gtest.cmake)
  rl_import_gtest(RL_GTEST_MAIN)

  function(rl_add_transport_test target_name source)
    add_executable(${target_name} ${source})
    target_link_libraries(${target_name} PRIVATE rl_transport protobuf::libprotobuf ${RL_GTEST_MAIN})
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME ${target_name} COMMAND ${target_name})
  endfunction()

  rl_add_transport_test(buffer_test test/test_buffer.cpp)
  rl_add_transport_test(header_crc_test test/test_header_crc.cpp)
  rl_add_transport_test(uds_pubsub_test test/test_uds_pubsub.cpp)
  rl_add_transport_test(backend_stub_test test/test_backend_stub.cpp)
  set_tests_properties(uds_pubsub_test PROPERTIES TIMEOUT 30)
endif()

if(ament_cmake_FOUND)
  set(RL_RUNTIME_DIR lib/${PROJECT_NAME})
else()
  set(RL_RUNTIME_DIR bin)
endif()

install(DIRECTORY include/ DESTINATION include)
install(FILES ${PROTO_HDRS} DESTINATION include)
install(TARGETS uds_pub_demo uds_sub_demo RUNTIME DESTINATION ${RL_RUNTIME_DIR})

if(ament_cmake_FOUND)
  install(TARGETS rl_transport
    EXPORT export_rl_transport
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
  )
  ament_export_targets(export_rl_transport HAS_LIBRARY_TARGET)
  ament_package()
else()
  install(TARGETS rl_transport
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
  )
endif()
