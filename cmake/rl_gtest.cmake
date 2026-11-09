# 优先用系统安装的 GTest（找到 GTest::gtest_main 即可）。
# Ubuntu 22.04 的 libgtest-dev 往往没有可用的 CMake 目标，这时从固定版本拉取。
function(rl_import_gtest out_var)
  if(TARGET GTest::gtest_main)
    set(${out_var} GTest::gtest_main PARENT_SCOPE)
    return()
  endif()

  find_package(GTest CONFIG QUIET)
  if(TARGET GTest::gtest_main)
    set(${out_var} GTest::gtest_main PARENT_SCOPE)
    return()
  endif()

  if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
  endif()

  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    URL_HASH SHA256=1f357c27ca988c3f7c6b4bf68a9395005ac6761f034046e9dde0896e3aba00e4
  )
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  if(NOT TARGET GTest::gtest_main)
    message(FATAL_ERROR "未能得到 GTest::gtest_main")
  endif()
  set(${out_var} GTest::gtest_main PARENT_SCOPE)
endfunction()
