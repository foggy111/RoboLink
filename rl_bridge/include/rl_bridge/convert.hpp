#pragma once

#include <cmath>
#include <cstdint>
#include <string>

#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "odometry.pb.h"
#include "twist.pb.h"

namespace rl_bridge {

// 只拷贝字段。时间戳和 CRC 由 rl::Publisher::publish 里的 seal_message 填写。
inline robolink::Twist to_transport_twist(const geometry_msgs::msg::Twist& in, std::uint64_t seq) {
  robolink::Twist out;
  out.mutable_header()->set_seq(seq);
  out.set_linear_x(in.linear.x);
  out.set_linear_y(in.linear.y);
  out.set_linear_z(in.linear.z);
  out.set_angular_x(in.angular.x);
  out.set_angular_y(in.angular.y);
  out.set_angular_z(in.angular.z);
  return out;
}

// child_frame_id 由调用方指定。仿真里的 Gazebo 里程计不走这条函数。
inline nav_msgs::msg::Odometry to_ros_odometry(const robolink::Odometry& in,
                                               const std::string& frame_id,
                                               const std::string& child_frame_id) {
  nav_msgs::msg::Odometry out;
  const std::uint64_t ns = in.header().timestamp_ns();
  out.header.stamp.sec = static_cast<std::int32_t>(ns / 1000000000ull);
  out.header.stamp.nanosec = static_cast<std::uint32_t>(ns % 1000000000ull);
  out.header.frame_id = frame_id;
  out.child_frame_id = child_frame_id;
  out.pose.pose.position.x = in.x();
  out.pose.pose.position.y = in.y();
  const double half = in.yaw() * 0.5;
  out.pose.pose.orientation.z = std::sin(half);
  out.pose.pose.orientation.w = std::cos(half);
  out.twist.twist.linear.x = in.vx();
  out.twist.twist.angular.z = in.vyaw();
  return out;
}

}  // namespace rl_bridge
