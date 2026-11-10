#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_node.hpp"
#include "rclcpp_lifecycle/lifecycle_publisher.hpp"

namespace rl {
class Publisher;
class Subscriber;
}  // namespace rl

namespace rl_bridge {

// 生命周期节点。activate 时拉起 UDS 发布端，把 cmd_vel 复制过去。
// 同时订阅另一条 UDS，把 robolink.Odometry 发到 rl_odom。
// 不替换 Gazebo 对 /cmd_vel 和 /odom 的订阅。
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

  void on_cmd_vel(geometry_msgs::msg::Twist::ConstSharedPtr msg);
  void on_transport_odom(const std::uint8_t* data, std::size_t len);
  void release_transport();

  std::string uds_path_;
  std::string uds_odom_path_;
  std::string odom_topic_;
  std::string odom_frame_;
  std::string base_frame_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_sub_;
  rclcpp_lifecycle::LifecyclePublisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  std::unique_ptr<rl::Publisher> cmd_pub_;
  std::unique_ptr<rl::Subscriber> odom_sub_;
  std::mutex transport_mu_;
  std::atomic<std::uint64_t> seq_{0};
  std::atomic<std::uint64_t> odom_drop_{0};
  bool logged_cmd_{false};
};

}  // namespace rl_bridge
