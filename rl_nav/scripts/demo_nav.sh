#!/usr/bin/env bash
# 答辩时的两条入口。先 source /opt/ros/humble/setup.bash 和工作空间 install/setup.bash。
set -euo pipefail

if ! command -v ros2 >/dev/null 2>&1; then
  echo "找不到 ros2。先执行: source /opt/ros/humble/setup.bash && source <工作空间>/install/setup.bash" >&2
  exit 1
fi

mode="${1:-up}"
if [[ $# -gt 0 ]]; then
  shift
fi

case "${mode}" in
  up)
    exec ros2 launch rl_nav sim_nav.launch.py "$@"
    ;;
  goal)
    exec ros2 run rl_nav send_nav_goal --ros-args -p use_sim_time:=true -p x:=2.0 -p y:=1.0 -p yaw:=0.0
    ;;
  *)
    echo "用法: $0 up [launch 参数...] | $0 goal" >&2
    exit 2
    ;;
esac
