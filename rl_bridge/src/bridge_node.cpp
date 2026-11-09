#include "rl_bridge/bridge_node.hpp"

#include "rl_transport/publisher.hpp"

namespace rl_bridge {

BridgeNode::BridgeNode(const rclcpp::NodeOptions& options)
    : rclcpp_lifecycle::LifecycleNode("rl_bridge", options) {
  uds_path_ = declare_parameter<std::string>("uds_path", "/tmp/robolink_cmd.sock");
}

BridgeNode::~BridgeNode() = default;

BridgeNode::CallbackReturn BridgeNode::on_configure(const rclcpp_lifecycle::State&) {
  const auto qos = rclcpp::QoS(rclcpp::KeepLast(10));
  cmd_sub_ = create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel", qos, [this](geometry_msgs::msg::Twist::ConstSharedPtr msg) {
        // TODO(M4): 转成 robolink.Twist 再 publish。
        //   header.seq 本地递增
        //   linear_x/y/z、angular_x/y/z 按同名字段拷贝
        //   调用 cmd_pub_->publish(twist)，seal 会填时间戳和 CRC
        if (cmd_pub_) {
          RCLCPP_DEBUG(get_logger(), "收到 cmd_vel linear.x=%.3f，尚未转发到 %s", msg->linear.x,
                       uds_path_.c_str());
        }
      });
  odom_pub_ = create_publisher<nav_msgs::msg::Odometry>("odom", qos);
  RCLCPP_INFO(get_logger(), "rl_bridge 已配置");
  return CallbackReturn::SUCCESS;
}

BridgeNode::CallbackReturn BridgeNode::on_activate(const rclcpp_lifecycle::State&) {
  odom_pub_->on_activate();
  try {
    // 激活时才占用套接字，停用时析构发布端并 unlink。
    cmd_pub_ = rl::make_publisher(rl::Backend::kUds, uds_path_);
  } catch (const std::exception& ex) {
    odom_pub_->on_deactivate();
    RCLCPP_ERROR(get_logger(), "启动 UDS 发布端失败: %s", ex.what());
    return CallbackReturn::FAILURE;
  }
  // TODO(M4): 再开一个 UdsSubscriber 接里程计路径，解码 robolink.Odometry 后发布 odom。
  RCLCPP_INFO(get_logger(), "rl_bridge 已激活，UDS 发布端 %s", uds_path_.c_str());
  return CallbackReturn::SUCCESS;
}

BridgeNode::CallbackReturn BridgeNode::on_deactivate(const rclcpp_lifecycle::State&) {
  cmd_pub_.reset();
  if (odom_pub_) {
    odom_pub_->on_deactivate();
  }
  RCLCPP_INFO(get_logger(), "rl_bridge 已停用");
  return CallbackReturn::SUCCESS;
}

BridgeNode::CallbackReturn BridgeNode::on_cleanup(const rclcpp_lifecycle::State&) {
  cmd_pub_.reset();
  cmd_sub_.reset();
  odom_pub_.reset();
  return CallbackReturn::SUCCESS;
}

BridgeNode::CallbackReturn BridgeNode::on_shutdown(const rclcpp_lifecycle::State&) {
  cmd_pub_.reset();
  cmd_sub_.reset();
  odom_pub_.reset();
  return CallbackReturn::SUCCESS;
}

}  // namespace rl_bridge
