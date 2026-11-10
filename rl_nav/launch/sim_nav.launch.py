"""启动 Gazebo 房间、差速机器人和 Nav2。

Nav2 比 Gazebo 晚 5 秒启动，用来等 /clock 和模型。这是启动顺序，不是链路耗时。
默认出生点 (0, 0, yaw 0) 要和 config/nav2_params.yaml 里 AMCL 的 initial_pose 一致。
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, TimerAction
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command, LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    share = get_package_share_directory("rl_nav")
    gazebo_share = get_package_share_directory("gazebo_ros")
    nav2_share = get_package_share_directory("nav2_bringup")

    use_sim_time = LaunchConfiguration("use_sim_time")
    gui = LaunchConfiguration("gui")
    rviz = LaunchConfiguration("rviz")
    verbose = LaunchConfiguration("verbose")

    urdf = os.path.join(share, "urdf", "diff_bot.urdf.xacro")
    world = os.path.join(share, "worlds", "room.world")
    params = os.path.join(share, "config", "nav2_params.yaml")
    map_yaml = os.path.join(share, "maps", "room.yaml")
    rviz_config = os.path.join(share, "rviz", "sim_nav.rviz")

    robot_description = ParameterValue(Command(["xacro ", urdf]), value_type=str)

    gazebo_server = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(gazebo_share, "launch", "gzserver.launch.py")),
        launch_arguments={"world": world, "verbose": verbose}.items(),
    )
    gazebo_client = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(gazebo_share, "launch", "gzclient.launch.py")),
        condition=IfCondition(gui),
    )

    robot_state = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        output="screen",
        parameters=[{"use_sim_time": use_sim_time, "robot_description": robot_description}],
    )
    spawn = Node(
        package="gazebo_ros",
        executable="spawn_entity.py",
        output="screen",
        arguments=[
            "-entity",
            "rl_diff_bot",
            "-topic",
            "robot_description",
            "-x",
            LaunchConfiguration("x"),
            "-y",
            LaunchConfiguration("y"),
            "-z",
            "0.01",
            "-Y",
            LaunchConfiguration("yaw"),
        ],
    )
    nav2 = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(nav2_share, "launch", "bringup_launch.py")),
        launch_arguments={
            "slam": "False",
            "map": map_yaml,
            "use_sim_time": use_sim_time,
            "params_file": params,
            "autostart": "True",
            "use_composition": "False",
        }.items(),
    )
    rviz_node = Node(
        package="rviz2",
        executable="rviz2",
        output="screen",
        arguments=["-d", rviz_config],
        parameters=[{"use_sim_time": use_sim_time}],
        condition=IfCondition(rviz),
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument("use_sim_time", default_value="true"),
            DeclareLaunchArgument("gui", default_value="true", description="false 时不打开 Gazebo 窗口"),
            DeclareLaunchArgument("rviz", default_value="true", description="false 时不打开 RViz"),
            DeclareLaunchArgument("verbose", default_value="false"),
            DeclareLaunchArgument("x", default_value="0.0"),
            DeclareLaunchArgument("y", default_value="0.0"),
            DeclareLaunchArgument("yaw", default_value="0.0"),
            gazebo_server,
            gazebo_client,
            robot_state,
            spawn,
            TimerAction(period=5.0, actions=[nav2, rviz_node]),
        ]
    )
