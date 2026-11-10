# 人机协作：这个仓库怎么用 AI

职规赛要看的是过程，不是一句「用了 AI」。下面是本仓库约定的做法，以及这次仿真改动里实际发生的事。这里不写「百分之多少由 AI 完成」这种比例。

## 约定的循环

每个模块按这个顺序做：

1. 人先写设计说明和对外接口：架构、取舍、失败时的行为、测试计划。接口包括话题名、坐标系、launch 参数、退出码。
2. AI 可以起草这些配套材料：CMake / colcon、launch、CI、Docker、测试用例列表、GTest 或 launch 测试的草稿、不熟悉的 ROS 2 / Nav2 报错的阅读笔记、diff 评审意见、README 和注释和提交说明的第一稿。
3. 人必须能自己讲清楚三条，不能靠背 AI 的稿：导航链路（传感器 → TF → AMCL → Costmap → 行为树规划 → 控制 → 底盘），中间件选型（UDS、epoll、Protobuf、CRC 的算法），以及任何一个已经写进仓库的测量数字。AI 不能编造基准或导航成功率。没有跑过的格子保持「待测」。
4. 一个模块的落地顺序是：设计说明和接口 → AI 起草测试 → 实现到测试通过 → 把 AI 的评审意见记在 `docs/ai_review_notes.md` 或当次 PR 的「本次 AI 用途」。

PR 模板里有「本次 AI 用途」。每次改动填：人已经决定什么，AI 起草了什么，人需要能讲什么，哪些指标仍然待测。

## 这次仿真改动实际是怎么发生的

学生 PPT 和仓库里已有的 README 已经定了这些事：七段导航链路、M1 的 UDS / epoll / Protobuf / CRC、里程碑里 Nav2 + Gazebo 还没做、不把没测的数字填上、不把 CRAIC 的识别率写成 ROS 成绩、不声称真机。

这次的设计说明 `docs/nav_simulation_design.md`、验证方案 `docs/nav_verification_plan.md` 和 `rl_nav/scripts/room_spec.py` 里的常量，是编码代理根据 PPT 和已有代码起草的，并先作为接口写进仓库。随后的 launch、URDF、Nav2 参数、地图生成、目标脚本、指标脚本、`rl_bridge` 的字段转换和 GTest、CI 和 Dockerfile 的修改、README 第一稿，也是编码代理写的。

代理没有在这台机器上跑通 Gazebo 里的 `NavigateToPose`。四项 PPT 指标因此继续标待测，没有填「大概能到多少」。

学生答辩时要能离开稿子讲清楚：

- 仿真里每一段对应哪个进程和哪条话题，`/cmd_vel` 为什么经过速度平滑。
- 为什么 `rl_bridge` 只是旁路，Gazebo 仍然直接订阅 `/cmd_vel`。
- UDS 发布端是监听套接字的一方，epoll 为什么只在写不出去时才看 `EPOLLOUT`，CRC 为什么要先把字段清零再加域盐。这些在 `rl_transport` 的源码和 README 里，不在 Nav2 里。
- 参数文件里的 20 Hz 和控制指令真正的发送频率不是同一件事。

## 不允许的写法

- 用 AI 填上 QPS、P99、CPU、导航耗时、控制频率、成功率、定位误差。
- 把 CRAIC 官方数据集的识别率写成本仓库或 ROS 2 的成绩。
- 把 Gazebo 世界位姿写成编码器或真车里程计。
- 写一个无法核对的比例来形容 AI 写了多少。
