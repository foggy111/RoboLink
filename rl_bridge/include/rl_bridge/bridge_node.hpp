#pragma once

#include <memory>
#include <string>

#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_node.hpp"
#include "rclcpp_lifecycle/lifecycle_publisher.hpp"

namespace rl {
class Publisher;
}

namespace rl_bridge {

// 生命周期节点骨架。configure 时创建 ROS 话题，activate 时拉起 UDS 发布端。
// 真正的消息转换留在 M4，字段对应关系写在 bridge_node.cpp 的注释里。
class BridgeNode : public rclcpp_lifecycle::LifecycleNode {
 public:
  explicit BridgeNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());
  ~BridgeNode() override;

 private:
  using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

  CallbackReturn on_configure(const rclcpp_lifecycle::State& state) override;
  CallbackReturn on_activate(const rclcpp_lifecycle::State& state) override;
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State& state) override;
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State& state) override;
  CallbackReturn on_shutdown(const rclcpp_lifecycle::State& state) override;

  std::string uds_path_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_sub_;
  rclcpp_lifecycle::LifecyclePublisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  std::unique_ptr<rl::Publisher> cmd_pub_;
};

}  // namespace rl_bridge
