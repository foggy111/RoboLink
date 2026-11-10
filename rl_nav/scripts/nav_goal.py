"""NavigateToPose 客户端。send_nav_goal 和 collect_nav_metrics 共用。"""

import math

import rclpy
from action_msgs.msg import GoalStatus
from geometry_msgs.msg import PoseStamped
from nav2_msgs.action import NavigateToPose
from rclpy.action import ActionClient
from rclpy.executors import SingleThreadedExecutor
from rclpy.node import Node
from rclpy.parameter import Parameter

STATUS_NAMES = {
    GoalStatus.STATUS_UNKNOWN: "UNKNOWN",
    GoalStatus.STATUS_ACCEPTED: "ACCEPTED",
    GoalStatus.STATUS_EXECUTING: "EXECUTING",
    GoalStatus.STATUS_CANCELING: "CANCELING",
    GoalStatus.STATUS_SUCCEEDED: "SUCCEEDED",
    GoalStatus.STATUS_CANCELED: "CANCELED",
    GoalStatus.STATUS_ABORTED: "ABORTED",
}


def spin_until(nodes, future, timeout_sec):
    executor = SingleThreadedExecutor()
    added = []
    try:
        for node in nodes:
            executor.add_node(node)
            added.append(node)
        executor.spin_until_future_complete(future, timeout_sec=timeout_sec)
    finally:
        for node in added:
            executor.remove_node(node)


def declare_default(node, name, value):
    """命令行已经声明过的参数保留原值。两个节点在同一进程里会看到同一批覆盖。"""
    if not node.has_parameter(name):
        node.declare_parameter(name, value)


class SendNavGoal(Node):
    def __init__(self):
        super().__init__(
            "send_nav_goal",
            automatically_declare_parameters_from_overrides=True,
            parameter_overrides=[Parameter("use_sim_time", Parameter.Type.BOOL, True)],
        )
        declare_default(self, "x", 2.0)
        declare_default(self, "y", 1.0)
        declare_default(self, "yaw", 0.0)
        declare_default(self, "server_timeout_sec", 90.0)
        declare_default(self, "timeout_sec", 180.0)
        self._client = ActionClient(self, NavigateToPose, "navigate_to_pose")
        self._last_feedback_s = 0.0

    def run(self, extra_nodes=()):
        nodes = (self, *extra_nodes)
        server_timeout = float(self.get_parameter("server_timeout_sec").value)
        if not self._client.wait_for_server(timeout_sec=server_timeout):
            self.get_logger().error(
                f"{server_timeout:.0f} 秒内没有等到 navigate_to_pose。先确认 sim_nav 已启动并且 Nav2 完成激活。"
            )
            return 2

        yaw = float(self.get_parameter("yaw").value)
        goal = NavigateToPose.Goal()
        goal.pose = PoseStamped()
        goal.pose.header.frame_id = "map"
        goal.pose.header.stamp = self.get_clock().now().to_msg()
        goal.pose.pose.position.x = float(self.get_parameter("x").value)
        goal.pose.pose.position.y = float(self.get_parameter("y").value)
        goal.pose.pose.orientation.z = math.sin(yaw * 0.5)
        goal.pose.pose.orientation.w = math.cos(yaw * 0.5)
        self.get_logger().info(
            "发送 NavigateToPose: x={x:.3f} y={y:.3f} yaw={yaw:.3f} frame=map".format(
                x=goal.pose.pose.position.x,
                y=goal.pose.pose.position.y,
                yaw=yaw,
            )
        )

        send_future = self._client.send_goal_async(goal, feedback_callback=self._feedback)
        spin_until(nodes, send_future, 30.0)
        if not send_future.done():
            self.get_logger().error("发送目标后 30 秒没有收到接受或拒绝")
            return 1
        goal_handle = send_future.result()
        if goal_handle is None or not goal_handle.accepted:
            self.get_logger().error("navigate_to_pose 拒绝了这个目标")
            return 1

        result_future = goal_handle.get_result_async()
        timeout = float(self.get_parameter("timeout_sec").value)
        spin_until(nodes, result_future, timeout)
        if not result_future.done():
            self.get_logger().error(f"动作在 {timeout:.0f} 秒内没有结束，准备取消")
            cancel_future = goal_handle.cancel_goal_async()
            spin_until(nodes, cancel_future, 10.0)
            return 1

        status = result_future.result().status
        name = STATUS_NAMES.get(status, str(status))
        self.get_logger().info(f"动作结束，状态 {name} ({status})")
        if status == GoalStatus.STATUS_SUCCEEDED:
            return 0
        return 1

    def _feedback(self, feedback_msg):
        now_s = self.get_clock().now().nanoseconds / 1e9
        if now_s - self._last_feedback_s < 1.0:
            return
        self._last_feedback_s = now_s
        remaining = feedback_msg.feedback.distance_remaining
        recoveries = feedback_msg.feedback.number_of_recoveries
        self.get_logger().info(f"剩余距离 {remaining:.3f} m，恢复动作 {recoveries} 次")


def main():
    rclpy.init()
    node = SendNavGoal()
    code = 1
    try:
        code = node.run()
    except KeyboardInterrupt:
        code = 1
    finally:
        node.destroy_node()
        rclpy.shutdown()
    return code
