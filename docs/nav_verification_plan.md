# 导航仿真验证方案

对应 PPT 第 14 页的四项：导航链路分段耗时、局部控制频率、导航任务成功率、TF 变换一致性。

仓库里现在没有一次保存下来的仿真运行。下表在有原始记录之前都是待测。`rl_nav/config/nav2_params.yaml` 里的 `controller_frequency: 20.0` 是参数设定，不是测到的控制频率。

| 指标 | 仓库中的结果 | 怎么采 |
| --- | --- | --- |
| 分段耗时 | 待测 | `collect_nav_metrics` 记录 `/scan`、`/amcl_pose` 的收到频率，以及消息头时间戳到本节点时钟的延迟。规划器内部耗时没有打点，该项保持未采集。 |
| 局部控制频率 | 待测 | 同一脚本统计 `/cmd_vel` 的收到频率。这是速度平滑之后、Gazebo 订阅的那条指令。 |
| 导航任务成功率 | 待测 | 任务定义：一次 `NavigateToPose`，结果为 `SUCCEEDED` 算成功。判定容差是参数 `xy_goal_tolerance: 0.25`（米）和 `yaw_goal_tolerance: 0.25`（弧度）。脚本参数 `goal_attempts` 大于 0 时才统计；默认不发目标，成功率保持待测。 |
| TF 一致性 | 待测 | 脚本在窗口结束时查询 `map` → `base_link`、`odom` → `base_footprint`、`base_link` → `laser_link` 是否存在。查到了只说明变换在当时能解析。相对 Gazebo 模型真值的定位误差没有做，保持待测。 |

## 采集步骤

仿真已经起来之后，另开终端：

```bash
source /opt/ros/humble/setup.bash
source <工作空间>/install/setup.bash
ros2 run rl_nav collect_nav_metrics --ros-args \
  -p use_sim_time:=true \
  -p duration_sec:=20.0 \
  -p output:=/tmp/rl_nav_metrics.json
```

要连成功率一起记，把 `goal_attempts` 设成实际要跑的次数，例如 `-p goal_attempts:=3`。每次目标都是参数 `x`、`y`、`yaw`，默认 `(2.0, 1.0, 0)`。脚本按顺序发，上一次结束才发下一次。

脚本把 JSON 写到 `output`。旁边会再写一份同名的 `.md`，方便打开看。它不修改 README，也不把数字写回本文件。

要把某一次运行当成项目成绩，把那份 JSON 原样放进 `benchmarks/results/`，并确认里面有这些环境字段：`uname_sysname`、`uname_release`、`uname_machine`、`os_release`、`proc_version`、`wsl`、`ros_distro`、`hostname`、`python`、`collected_at_utc`。同时在提交说明里写上：是否 WSL2、Humble 的安装方式、`CMAKE_BUILD_TYPE` 或 apt 包、这次有没有开 Gazebo GUI、目标点、`goal_attempts`。缺环境信息的数字不能拿来和 PPT 对照。

频率的算法写在 `rl_nav/scripts/nav_metrics_report.py`：`(消息数 - 1) / 订阅窗口秒数`。消息数少于 2，或者窗口长度不大于 0，该项为未采集，不补一个估计值。

## 不在这项验证里的内容

- 真机、编码器、串口帧上的轮速。
- 定位误差相对 Gazebo 实体位姿的对照。要做的话得另订 Gazebo 的模型位姿，并写明采样方式。现在没做。
- `rl_bridge` 的 UDS 延迟。传输层 README 里的 p50 / p99 仍然是待测。
- CRAIC 官方数据集上的识别率。那是另一项竞赛的结果，不是这次导航仿真的结果。

## 每次改仿真之后至少要过的自动检查

这些检查不启动 Gazebo，也不能代替上面四项：

- `python3 rl_nav/test/test_assets.py`：地图和世界是否还由 `room_spec.py` 生成、默认目标是否落在空地上、Nav2 参数是否还包含 AMCL、行为树、代价地图和控制器、文档是否仍把四项标成待测。
- `rl_bridge` 的 `convert_test`：`geometry_msgs/Twist` 和 `robolink.Twist` 的字段，以及 UDS 往返后的里程计解码。
- 已有的 `rl_transport`、`rl_hal` 测试。
