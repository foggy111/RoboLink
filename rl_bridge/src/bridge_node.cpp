#include "rl_bridge/bridge_node.hpp"

#include <exception>
#include <utility>

#include "odometry.pb.h"
#include "rl_bridge/convert.hpp"
#include "rl_transport/codec.hpp"
#include "rl_transport/publisher.hpp"
#include "rl_transport/subscriber.hpp"

namespace rl_bridge {

BridgeNode::BridgeNode(const rclcpp::NodeOptions& options)
    : rclcpp_lifecycle::LifecycleNode("rl_bridge", options) {
  uds_path_ = declare_parameter<std::string>("uds_path", "/tmp/robolink_cmd.sock");
  uds_odom_path_ = declare_parameter<std::string>("uds_odom_path", "/tmp/robolink_odom.sock");
  odom_topic_ = declare_parameter<std::string>("odom_topic", "rl_odom");
  odom_frame_ = declare_parameter<std::string>("odom_frame", "odom");
  base_frame_ = declare_parameter<std::string>("base_frame", "base_link");
}

BridgeNode::~BridgeNode() {
  release_transport();
}

BridgeNode::CallbackReturn BridgeNode::on_configure(const rclcpp_lifecycle::State&) {
  const auto qos = rclcpp::QoS(rclcpp::KeepLast(10));
  cmd_sub_ = create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel", qos, [this](geometry_msgs::msg::Twist::ConstSharedPtr msg) { on_cmd_vel(msg); });
  odom_pub_ = create_publisher<nav_msgs::msg::Odometry>(odom_topic_, qos);
  RCLCPP_INFO(get_logger(),
              "rl_bridge 已配置：订阅 cmd_vel，复制到 UDS %s；UDS 里程计 %s 解码后发布 %s。"
              "这条通路不替换 Nav2 和 Gazebo 之间的 cmd_vel。",
              uds_path_.c_str(), uds_odom_path_.c_str(), odom_topic_.c_str());
  return CallbackReturn::SUCCESS;
}

void BridgeNode::on_cmd_vel(geometry_msgs::msg::Twist::ConstSharedPtr msg) {
  std::lock_guard<std::mutex> lock(transport_mu_);
  if (!cmd_pub_ || !msg) {
    return;
  }
  try {
    const auto seq = seq_.fetch_add(1);
    cmd_pub_->publish(to_transport_twist(*msg, seq));
  } catch (const std::exception& ex) {
    RCLCPP_WARN(get_logger(), "cmd_vel 转发到 UDS 失败: %s", ex.what());
    return;
  }
  if (!logged_cmd_) {
    logged_cmd_ = true;
    RCLCPP_INFO(get_logger(), "已开始把 cmd_vel 复制到 %s（旁路）", uds_path_.c_str());
  }
}

void BridgeNode::on_transport_odom(const std::uint8_t* data, std::size_t len) {
  robolink::Odometry decoded;
  if (!rl::decode_message(data, len, decoded)) {
    const auto dropped = odom_drop_.fetch_add(1) + 1;
    if (dropped == 1 || dropped % 50 == 0) {
      RCLCPP_WARN(get_logger(), "UDS 里程计解析或 CRC 失败，累计丢弃 %llu 帧",
                  static_cast<unsigned long long>(dropped));
    }
    return;
  }
  if (!odom_pub_ || !odom_pub_->is_activated()) {
    return;
  }
  try {
    odom_pub_->publish(to_ros_odometry(decoded, odom_frame_, base_frame_));
  } catch (const std::exception& ex) {
    RCLCPP_WARN(get_logger(), "发布 %s 失败: %s", odom_topic_.c_str(), ex.what());
  }
}

BridgeNode::CallbackReturn BridgeNode::on_activate(const rclcpp_lifecycle::State&) {
  odom_pub_->on_activate();
  try {
    auto publisher = rl::make_publisher(rl::Backend::kUds, uds_path_);
    auto subscriber = rl::make_subscriber(rl::Backend::kUds, uds_odom_path_);
    subscriber->start(
        [this](const std::uint8_t* data, std::size_t len) { on_transport_odom(data, len); });
    std::lock_guard<std::mutex> lock(transport_mu_);
    cmd_pub_ = std::move(publisher);
    odom_sub_ = std::move(subscriber);
  } catch (const std::exception& ex) {
    release_transport();
    odom_pub_->on_deactivate();
    RCLCPP_ERROR(get_logger(), "启动 UDS 失败: %s", ex.what());
    return CallbackReturn::FAILURE;
  }
  RCLCPP_INFO(get_logger(),
              "rl_bridge 已激活。里程计订阅端若在约 30 秒内连不上 %s 会退出，%s 就没有数据；"
              "这不影响 Gazebo 自己的 /odom。",
              uds_odom_path_.c_str(), odom_topic_.c_str());
  return CallbackReturn::SUCCESS;
}

void BridgeNode::release_transport() {
  std::unique_ptr<rl::Subscriber> subscriber;
  std::unique_ptr<rl::Publisher> publisher;
  {
    std::lock_guard<std::mutex> lock(transport_mu_);
    subscriber = std::move(odom_sub_);
    publisher = std::move(cmd_pub_);
    logged_cmd_ = false;
  }
  if (subscriber) {
    subscriber->stop();
  }
  publisher.reset();
}

BridgeNode::CallbackReturn BridgeNode::on_deactivate(const rclcpp_lifecycle::State&) {
  release_transport();
  if (odom_pub_) {
    odom_pub_->on_deactivate();
  }
  RCLCPP_INFO(get_logger(), "rl_bridge 已停用");
  return CallbackReturn::SUCCESS;
}

BridgeNode::CallbackReturn BridgeNode::on_cleanup(const rclcpp_lifecycle::State&) {
  release_transport();
  cmd_sub_.reset();
  odom_pub_.reset();
  return CallbackReturn::SUCCESS;
}

BridgeNode::CallbackReturn BridgeNode::on_shutdown(const rclcpp_lifecycle::State&) {
  release_transport();
  cmd_sub_.reset();
  odom_pub_.reset();
  return CallbackReturn::SUCCESS;
}

}  // namespace rl_bridge
