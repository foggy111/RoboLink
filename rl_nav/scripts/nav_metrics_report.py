"""把一次导航观测收成报告。

没有样本的项保持「未采集」或「待测」，不填估计值。
本模块不读 ROS，方便在没有 Humble 的环境里测试计算公式。
"""

from __future__ import annotations

import json
import os
import platform
import socket
from datetime import datetime, timezone
from pathlib import Path

UNMEASURED = "待测"
NOT_COLLECTED = "未采集"

STAGE_TITLES = (
    ("scan", "传感器 /scan"),
    ("amcl_pose", "AMCL /amcl_pose"),
    ("global_costmap", "全局代价地图"),
    ("local_costmap", "局部代价地图"),
    ("plan", "全局路径 /plan"),
    ("cmd_vel", "控制输出 /cmd_vel"),
    ("odom", "底盘里程计 /odom"),
)


def collect_environment():
    uname = platform.uname()
    os_release = ""
    release_path = Path("/etc/os-release")
    if release_path.exists():
        os_release = release_path.read_text(encoding="utf-8", errors="replace").strip()
    proc = ""
    version_path = Path("/proc/version")
    if version_path.exists():
        proc = version_path.read_text(encoding="utf-8", errors="replace").strip()
    lowered = proc.lower()
    return {
        "uname_sysname": uname.system,
        "uname_release": uname.release,
        "uname_machine": uname.machine,
        "os_release": os_release,
        "proc_version": proc,
        "wsl": "microsoft" in lowered or "wsl" in lowered,
        "ros_distro": os.environ.get("ROS_DISTRO"),
        "hostname": socket.gethostname(),
        "python": platform.python_version(),
        "collected_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    }


def _rate(times, window_s):
    if not times or len(times) < 2 or window_s <= 0:
        return None
    return (len(times) - 1) / float(window_s)


def _mean(values):
    if not values:
        return None
    return sum(values) / float(len(values))


def _stage(times, delays, window_s):
    rate = _rate(times, window_s)
    delay = _mean(delays)
    messages = 0 if not times else len(times)
    return {
        "messages": messages,
        "hz": None if rate is None else round(rate, 6),
        "stamp_delay_ms_mean": None if delay is None else round(delay, 6),
        "status": "已采集" if messages else NOT_COLLECTED,
    }


def observe(samples, environment, task_results=None):
    """根据样本做报告。

    频率 = (消息数 - 1) / window_s。消息数少于 2 时频率为未采集。
    task_results 为空时，成功率保持待测，即使其它话题已经有样本。
    """
    window_s = float(samples.get("window_s") or 0.0)
    stages = {
        "scan": _stage(samples.get("scan_recv_s"), samples.get("scan_delay_ms"), window_s),
        "amcl_pose": _stage(samples.get("amcl_recv_s"), samples.get("amcl_delay_ms"), window_s),
        "global_costmap": _stage(samples.get("global_costmap_recv_s"), None, window_s),
        "local_costmap": _stage(samples.get("local_costmap_recv_s"), None, window_s),
        "plan": _stage(samples.get("plan_recv_s"), None, window_s),
        "cmd_vel": _stage(samples.get("cmd_vel_recv_s"), None, window_s),
        "odom": _stage(samples.get("odom_recv_s"), None, window_s),
    }
    if task_results:
        successes = sum(1 for item in task_results if item)
        tasks = {
            "attempts": len(task_results),
            "successes": successes,
            "success_rate": round(successes / float(len(task_results)), 6),
            "status": "本次运行",
        }
        label = "本次运行"
    else:
        tasks = {
            "attempts": None,
            "successes": None,
            "success_rate": UNMEASURED,
            "status": UNMEASURED,
        }
        any_messages = any(stage["messages"] for stage in stages.values())
        label = "部分采集" if any_messages else UNMEASURED

    return {
        "label": label,
        "committed_as_project_result": False,
        "note": (
            "频率 = (消息数 - 1) / 订阅窗口秒数，只用这次收到的消息。"
            "数字只描述传入的样本，不会写回 README。"
            "要把它当成项目成绩，必须连同 environment 原样放进 benchmarks/results/ 并提交。"
        ),
        "planner_internal_ms": {
            "value": None,
            "status": NOT_COLLECTED,
            "note": "本仓库没有在 Nav2 内部打点。行为树的重规划周期是上游默认树里的设定，不是实测。",
        },
        "pipeline": stages,
        "tf": samples.get("tf") or {},
        "nav_tasks": tasks,
        "task_definition": (
            "一次任务是一次 NavigateToPose。成功仅指动作结果为 SUCCEEDED。"
            "判定容差由 nav2_params.yaml 的 general_goal_checker.xy_goal_tolerance 决定，"
            "当前设定是 0.25 m。这不是定位误差。"
        ),
        "environment": environment,
        "window_s": window_s,
    }


def _cell(value):
    if value is None:
        return NOT_COLLECTED
    return str(value)


def render_markdown(report):
    lines = [
        "# 导航观测",
        "",
        f"状态：{report['label']}",
        "",
        report["note"],
        "",
        report["task_definition"],
        "",
        "| 环节 | 消息数 | 频率 Hz | 时间戳延迟均值 ms | 状态 |",
        "| --- | --- | --- | --- | --- |",
    ]
    for key, title in STAGE_TITLES:
        stage = report["pipeline"][key]
        lines.append(
            "| {title} | {messages} | {hz} | {delay} | {status} |".format(
                title=title,
                messages=stage["messages"],
                hz=_cell(stage["hz"]),
                delay=_cell(stage["stamp_delay_ms_mean"]),
                status=stage["status"],
            )
        )
    tasks = report["nav_tasks"]
    lines.extend(
        [
            "",
            "规划器内部耗时：{status}。{note}".format(
                status=report["planner_internal_ms"]["status"],
                note=report["planner_internal_ms"]["note"],
            ),
            "",
            "导航任务：尝试 {attempts}，成功 {successes}，成功率 {rate}。".format(
                attempts=_cell(tasks["attempts"]),
                successes=_cell(tasks["successes"]),
                rate=tasks["success_rate"],
            ),
            "",
            "TF：",
            "",
            "```json",
            json.dumps(report.get("tf") or {}, ensure_ascii=False, indent=2),
            "```",
            "",
            "环境：",
            "",
            "```json",
            json.dumps(report.get("environment") or {}, ensure_ascii=False, indent=2),
            "```",
            "",
        ]
    )
    return "\n".join(lines)
