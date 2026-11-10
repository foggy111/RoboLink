# 仿真导航设计

这份说明对应职规赛 PPT《让机器人真正跑起来》第 13 页的导航链路：传感器输入、TF、AMCL、Costmap、行为树规划、控制、底盘执行。

目标是在 ROS 2 Humble 上把这条链路跑成可重复启动的仿真。不写新的规划器，不把竞赛识别率写进来，也不把没跑过的耗时和成功率写成数字。真机不在这次范围内。

几何常量在 `rl_nav/scripts/room_spec.py`。地图和 Gazebo 世界由 `rl_nav/scripts/gen_room_assets.py` 按同一份常量生成。

## 选用的组件

| PPT 环节 | 进程 / 插件 | 话题或坐标系 |
| --- | --- | --- |
| 传感器 | Gazebo `libgazebo_ros_ray_sensor.so` | `/scan`，`frame_id` 为 `laser_link` |
| TF | `robot_state_publisher` 发布 URDF 里的固定关节和轮子；差速插件发布 `odom` → `base_footprint`；AMCL 发布 `map` → `odom` | `/tf`、`/tf_static` |
| 定位 | `nav2_amcl` | `/amcl_pose`、`/particlecloud`，订阅 `/scan` 和里程计 |
| Costmap | `nav2_costmap_2d`（全局一张、局部一张） | `/global_costmap/costmap`、`/local_costmap/costmap` |
| 规划（行为树） | `nav2_bt_navigator` + `nav2_planner` 的 NavFn | 动作 `navigate_to_pose`，路径 `/plan` |
| 控制 | `nav2_controller` 的 DWB，再经过 `nav2_velocity_smoother` | 控制器输出被 Nav2 重映射到 `cmd_vel_nav`，平滑后发布 `/cmd_vel` |
| 底盘执行 | `libgazebo_ros_diff_drive.so` | 订阅 `/cmd_vel`，发布 `/odom`，并广播 `odom` → `base_footprint` |

Nav2 由已安装的 `nav2_bringup/launch/bringup_launch.py` 启动，`use_composition:=false`，这样 `ros2 node list` 里能分开看到 `map_server`、`amcl`、`planner_server`、`controller_server`、`bt_navigator`、`velocity_smoother`。

行为树用 `nav2_bt_navigator` 自带的默认 NavigateToPose 树（Humble 上的文件名是 `navigate_to_pose_w_replanning_and_recovery.xml`）。树里是大约 1 Hz 的重规划，跟踪路径，失败时清代价地图、原地转、等待、后退。这个周期是上游行为树文件里的设定。本仓库没有改那份 XML，也没有测过实际重规划耗时。

## 坐标系和模型

```text
map → odom → base_footprint → base_link → laser_link
                                  ├→ left_wheel_link
                                  ├→ right_wheel_link
                                  └→ caster_link
```

- 轮距 `0.28 m`，轮半径 `0.05 m`。插件里的 `wheel_diameter` 是 `0.10`。
- `base_link` 在 `base_footprint` 上方 `0.05 m`，和轮半径一样，轮子落在足迹平面上。
- 代价地图用圆形半径 `0.22 m`，膨胀半径 `0.30 m`。这是按底盘外包络留的余量，不是标定出来的。
- 差速插件 `robot_base_frame` 是 `base_footprint`。AMCL 的 `base_frame_id` 同样是 `base_footprint`。代价地图和 `bt_navigator` 用 `base_link`。两者之间是固定关节。
- `odometry_source` 取插件默认值 `1`，里程计来自 Gazebo 世界位姿。这是仿真里的模型位姿，不是编码器积分，也不是真车数据。

房间：

- 地图覆盖 `[-5, 5] × [-5, 5]` 米，分辨率 `0.05`，原点 `[-5, -5, 0]`。
- 四面墙厚 `0.15 m`，贴在边界内侧。高度 `0.6 m`，能挡住水平激光。
- 方柱中心 `(1.5, -1.5)`，边长 `0.6 m`。
- 默认出生点地图坐标 `(0, 0)`，朝向 `0`。生成时 `z = 0.01`，让轮子不要一开始嵌进地面。
- 默认目标 `(2.0, 1.0)`，朝向 `0`。方柱在目标的南侧，直线过去是空的。

AMCL `set_initial_pose` 为 true，初始位姿 `(0, 0, yaw 0)`，和默认出生点相同。只改 launch 里的出生点、不改 `rl_nav/config/nav2_params.yaml` 的 `initial_pose`，粒子会从旧位置开始。两处没有自动绑在一起。

## 速度指令怎么走

`navigation_launch.py`（Nav2 Humble）把 `controller_server` 的 `cmd_vel` 重映射成 `cmd_vel_nav`，`velocity_smoother` 订阅 `cmd_vel_nav`，再把平滑结果以 `/cmd_vel` 发出。Gazebo 差速插件订阅 `/cmd_vel`。

`rl_bridge` 也订阅 `/cmd_vel`，转成 `robolink.Twist` 后发到 Unix 域套接字。它不取消 Gazebo 的订阅。导航演示的 launch 不启动 `rl_bridge`。桥激活失败、套接字被占用、或者没有 UDS 订阅者时，仿真里的车仍然听 `/cmd_vel`。

桥发布的里程计话题默认叫 `rl_odom`，不占用 Gazebo 的 `/odom`。UDS 里程计路径默认 `/tmp/robolink_odom.sock`。当前仿真没有进程往这个套接字写 `robolink.Odometry`，所以联调时 `rl_odom` 没有数据。`rl_transport` 的订阅端只在启动后大约 30 秒内重试连接，超时后线程退出。这是已有行为，这次不改传输层。

## 取舍

- 用 Gazebo Classic 和 `gazebo_ros`。Humble 的 apt 包提供这套差速插件和射线传感器。
- 用 Nav2 的 AMCL、代价地图、行为树、DWB 和速度平滑。参数键和 Humble 的 `nav2_bringup` 对齐，数值按上面的模型改。DWB 的 critic 比例沿用 Nav2 示例里差速小车的一组，没有在本仓库重新标定。
- 用一张事先生成的静态地图。地图和墙来自同一份 `room_spec.py`，激光应该打在墙上。不在演示路径里先跑 SLAM。
- `use_composition` 设为 false，节点分开，方便对着 PPT 讲。
- 控制频率参数 `controller_frequency: 20.0` 和速度上限 `0.26 m/s` 是配置。测到的 `/cmd_vel` 频率要等 `collect_nav_metrics` 在一次真实仿真里写下来。
- 目标判定用 Nav2 的 `general_goal_checker`，`xy_goal_tolerance: 0.25`、`yaw_goal_tolerance: 0.25`。动作结果为 `SUCCEEDED` 时，脚本退出码为 0。0.25 m 是这个判定的设定，不是定位误差。
- CI 只做包安装、传输层测试、桥的转换测试和 `rl_nav` 的资产测试。GitHub Actions 里现有的 `ros:humble` 镜像没有 Gazebo，也没有稳定的 GPU，所以不在 CI 里启动仿真。

## 失败时会看到什么

- `send_nav_goal` 在 `server_timeout_sec`（默认 90）内等不到 `navigate_to_pose`：退出码 2。
- 动作结束但状态不是 `SUCCEEDED`，或者超过 `timeout_sec`（默认 180）：退出码 1，终端打印实际状态名。
- 桥在 `configure` 之后、`activate` 之前收到 `cmd_vel`：丢弃，因为 UDS 发布端还没创建。
- 桥 `activate` 时套接字 bind 失败：该次激活失败，ROS 侧导航不受影响（导航 launch 本来也不起桥）。
- UDS 里程计帧 CRC 或解析失败：丢弃，并按累计次数打日志。
- 启动后 RViz 暂时报固定坐标系 `map` 不存在：AMCL 还没发布 `map` → `odom`。生命周期起来之后应消失。若一直不出现，先看 AMCL 和 `/scan` 的日志，不要当成已经定位成功。

## 和这次实现对应的文件

| 文件 | 作用 |
| --- | --- |
| `rl_nav/launch/sim_nav.launch.py` | Gazebo、机器人、Nav2、可选 RViz |
| `rl_nav/urdf/diff_bot.urdf.xacro` | 差速模型和插件 |
| `rl_nav/worlds/room.world` | 生成出来的房间 |
| `rl_nav/maps/room.yaml`、`room.pgm` | 同一几何的栅格 |
| `rl_nav/config/nav2_params.yaml` | AMCL、代价地图、行为树导航、DWB、平滑器 |
| `rl_nav/scripts/send_nav_goal` | 发送一次 `NavigateToPose` |
| `rl_nav/scripts/collect_nav_metrics` | 按验证方案采集，缺样本就保持待测 |
| `rl_bridge` | `/cmd_vel` 旁路到 UDS，UDS 里程计解码到 `rl_odom` |
