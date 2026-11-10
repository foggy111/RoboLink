cmake_minimum_required(VERSION 3.16)
project(rl_nav VERSION 0.1.0)

find_package(ament_cmake REQUIRED)

# 运行时依赖 Nav2 和 Gazebo，但这里不 find_package 它们。
# CI 的 ros:humble 镜像可以安装本包的 launch 和资源，不必先装仿真器。

install(DIRECTORY launch urdf worlds maps config rviz
  DESTINATION share/${PROJECT_NAME}
)
install(PROGRAMS
  scripts/send_nav_goal
  scripts/collect_nav_metrics
  scripts/demo_nav.sh
  DESTINATION lib/${PROJECT_NAME}
)
install(FILES
  scripts/nav_metrics_report.py
  scripts/nav_goal.py
  scripts/room_spec.py
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  enable_testing()
  add_test(
    NAME rl_nav_assets
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/test/test_assets.py
  )
endif()

ament_package()
