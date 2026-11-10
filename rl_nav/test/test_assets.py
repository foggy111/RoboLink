#!/usr/bin/env python3
"""不启动 Gazebo 的检查：几何、参数、文档没有把待测写成成绩。"""

import ast
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NAV = ROOT / "rl_nav"
sys.path.insert(0, str(NAV / "scripts"))

import gen_room_assets  # noqa: E402
import nav_metrics_report  # noqa: E402
import room_spec  # noqa: E402


def _read(path):
    return Path(path).read_text(encoding="utf-8")


class RoomAssetsTest(unittest.TestCase):
    def test_generated_files_match_spec(self):
        with tempfile.TemporaryDirectory() as tmp:
            gen_room_assets.generate(tmp)
            tmp_path = Path(tmp)
            self.assertEqual((tmp_path / "maps" / "room.pgm").read_bytes(), (NAV / "maps" / "room.pgm").read_bytes())
            self.assertEqual((tmp_path / "maps" / "room.yaml").read_text(encoding="utf-8"), _read(NAV / "maps" / "room.yaml"))
            self.assertEqual((tmp_path / "worlds" / "room.world").read_text(encoding="utf-8"), _read(NAV / "worlds" / "room.world"))

    def test_spawn_and_goal_are_clear(self):
        needed = room_spec.ROBOT_RADIUS + room_spec.INFLATION_RADIUS + 0.05
        self.assertGreater(room_spec.distance_to_occupied(room_spec.SPAWN_X, room_spec.SPAWN_Y), needed)
        self.assertGreater(room_spec.distance_to_occupied(room_spec.GOAL_X, room_spec.GOAL_Y), needed)

    def test_urdf_matches_wheel_spec(self):
        urdf = _read(NAV / "urdf" / "diff_bot.urdf.xacro")
        self.assertIn(f"<wheel_separation>{room_spec.WHEEL_SEPARATION:.2f}</wheel_separation>", urdf)
        self.assertIn(f"<wheel_diameter>{2 * room_spec.WHEEL_RADIUS:.2f}</wheel_diameter>", urdf)
        self.assertIn(f'<cylinder radius="{room_spec.WHEEL_RADIUS:.2f}"', urdf)
        self.assertIn("libgazebo_ros_diff_drive.so", urdf)
        self.assertIn("libgazebo_ros_ray_sensor.so", urdf)
        self.assertIn("base_footprint", urdf)
        self.assertIn("laser_link", urdf)
        self.assertIn("<publish_odom>true</publish_odom>", urdf)
        self.assertIn("<robot_base_frame>base_footprint</robot_base_frame>", urdf)

    def test_nav2_params_cover_the_pipeline(self):
        text = _read(NAV / "config" / "nav2_params.yaml")
        for key in (
            "amcl:",
            "bt_navigator:",
            "controller_server:",
            "planner_server:",
            "global_costmap:",
            "local_costmap:",
            "behavior_server:",
            "map_server:",
            "velocity_smoother:",
        ):
            self.assertIn(key, text)
        self.assertIn("set_initial_pose: true", text)
        self.assertIn("initial_pose.x: 0.0", text)
        self.assertIn("initial_pose.y: 0.0", text)
        self.assertIn("initial_pose.yaw: 0.0", text)
        self.assertIn(f"robot_radius: {room_spec.ROBOT_RADIUS:.2f}", text)
        self.assertIn(f"inflation_radius: {room_spec.INFLATION_RADIUS:.2f}", text)
        self.assertIn("xy_goal_tolerance: 0.25", text)
        self.assertIn("controller_frequency: 20.0", text)
        self.assertIn("nav2_behaviors/Spin", text)
        self.assertIn("nav2_navfn_planner/NavfnPlanner", text)
        self.assertIn("dwb_core::DWBLocalPlanner", text)

    def test_launch_and_goal_defaults(self):
        launch = _read(NAV / "launch" / "sim_nav.launch.py")
        self.assertIn("bringup_launch.py", launch)
        self.assertIn("gzserver.launch.py", launch)
        self.assertIn('default_value="true"', launch)
        self.assertIn('"use_composition": "False"', launch)
        self.assertIn('"-z",\n            "0.01",', launch)
        self.assertIn('DeclareLaunchArgument("x", default_value="0.0"', launch)
        self.assertIn('DeclareLaunchArgument("y", default_value="0.0"', launch)
        self.assertIn('DeclareLaunchArgument("yaw", default_value="0.0"', launch)
        goal = _read(NAV / "scripts" / "nav_goal.py")
        self.assertIn("NavigateToPose", goal)
        self.assertIn(f'declare_default(self, "x", {room_spec.GOAL_X})', goal)
        self.assertIn(f'declare_default(self, "y", {room_spec.GOAL_Y})', goal)
        self.assertIn(f'declare_default(self, "yaw", {room_spec.GOAL_YAW})', goal)
        demo = _read(NAV / "scripts" / "demo_nav.sh")
        self.assertIn("sim_nav.launch.py", demo)
        self.assertIn(f"x:={room_spec.GOAL_X}", demo)
        self.assertIn(f"y:={room_spec.GOAL_Y}", demo)

    def test_python_sources_parse(self):
        for path in (NAV / "scripts").glob("*"):
            if path.suffix in {".py", ""} and path.is_file() and path.name != "demo_nav.sh":
                if path.suffix == ".py" or path.name in {"send_nav_goal", "collect_nav_metrics"}:
                    ast.parse(_read(path), filename=str(path))
        ast.parse(_read(NAV / "launch" / "sim_nav.launch.py"), filename="sim_nav.launch.py")
        ast.parse(_read(NAV / "test" / "test_assets.py"), filename="test_assets.py")


class MetricsReportTest(unittest.TestCase):
    def test_empty_samples_stay_unmeasured(self):
        report = nav_metrics_report.observe({"window_s": 10.0}, {"hostname": "unit"})
        self.assertEqual(report["label"], "待测")
        self.assertEqual(report["nav_tasks"]["success_rate"], "待测")
        self.assertFalse(report["committed_as_project_result"])
        self.assertIsNone(report["pipeline"]["cmd_vel"]["hz"])
        self.assertIsNone(report["planner_internal_ms"]["value"])
        text = nav_metrics_report.render_markdown(report)
        self.assertIn("待测", text)
        self.assertIn("未采集", text)

    def test_rate_uses_only_given_samples(self):
        report = nav_metrics_report.observe(
            {"window_s": 0.2, "cmd_vel_recv_s": [0.0, 0.1, 0.2]},
            {"hostname": "unit"},
        )
        self.assertEqual(report["label"], "部分采集")
        self.assertEqual(report["pipeline"]["cmd_vel"]["hz"], 10.0)
        self.assertEqual(report["pipeline"]["cmd_vel"]["messages"], 3)
        self.assertEqual(report["nav_tasks"]["success_rate"], "待测")
        self.assertIsNone(report["pipeline"]["scan"]["hz"])

    def test_success_rate_only_when_tasks_were_run(self):
        report = nav_metrics_report.observe(
            {"window_s": 1.0},
            {"hostname": "unit"},
            task_results=[True, False, True],
        )
        self.assertEqual(report["nav_tasks"]["attempts"], 3)
        self.assertEqual(report["nav_tasks"]["successes"], 2)
        self.assertEqual(report["nav_tasks"]["success_rate"], round(2 / 3, 6))
        self.assertIn("environment", report)


class HonestyDocsTest(unittest.TestCase):
    def test_docs_do_not_invent_results(self):
        files = [
            ROOT / "README.md",
            ROOT / "docs" / "nav_simulation_design.md",
            ROOT / "docs" / "nav_verification_plan.md",
            ROOT / "docs" / "ai_assisted_development.md",
            ROOT / "docs" / "ai_review_notes.md",
            ROOT / "benchmarks" / "results" / "README.md",
        ]
        for path in files:
            text = _read(path)
            for banned in ("95.83", "46/48", "48/48", "真机验证已完成", "80%", "百分之八十"):
                self.assertNotIn(banned, text, f"{path} 含有 {banned}")
        readme = _read(ROOT / "README.md")
        for phrase in (
            "待测",
            "sim_nav.launch.py",
            "send_nav_goal",
            "rl_odom",
            "WSLg",
            "gui:=false",
            "docs/ai_assisted_development.md",
            "docs/nav_verification_plan.md",
        ):
            self.assertIn(phrase, readme)
        self.assertIn("不是测到的控制频率", _read(ROOT / "docs" / "nav_verification_plan.md"))
        self.assertIn("本次 AI 用途", _read(ROOT / ".github" / "PULL_REQUEST_TEMPLATE.md"))
        self.assertIn("人先写设计说明", _read(ROOT / "docs" / "ai_assisted_development.md"))

    def test_no_result_files_without_environment(self):
        results = ROOT / "benchmarks" / "results"
        self.assertTrue(results.is_dir())
        for path in results.glob("*.json"):
            text = _read(path)
            self.assertIn("collected_at_utc", text)
            self.assertIn("os_release", text)


if __name__ == "__main__":
    unittest.main()
