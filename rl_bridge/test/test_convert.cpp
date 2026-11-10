#include <unistd.h>

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "nav_msgs/msg/odometry.hpp"
#include "odometry.pb.h"
#include "rl_bridge/convert.hpp"
#include "rl_transport/codec.hpp"
#include "rl_transport/publisher.hpp"
#include "rl_transport/subscriber.hpp"
#include "twist.pb.h"

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string socket_path(const char* name) {
  return "/tmp/rl_bridge_" + std::to_string(::getpid()) + "_" + name + ".sock";
}

template <typename Msg>
class Inbox {
 public:
  void push(Msg msg) {
    std::lock_guard<std::mutex> lock(mu_);
    items_.push_back(std::move(msg));
    cv_.notify_all();
  }

  bool wait_for(std::size_t n, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mu_);
    return cv_.wait_for(lock, timeout, [&] { return items_.size() >= n; });
  }

  std::vector<Msg> snapshot() const {
    std::lock_guard<std::mutex> lock(mu_);
    return items_;
  }

 private:
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::vector<Msg> items_;
};

bool wait_subscribers(rl::Publisher& publisher, int n) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    if (publisher.subscriber_count() >= n) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return publisher.subscriber_count() >= n;
}

}  // namespace

TEST(Convert, TwistFields) {
  geometry_msgs::msg::Twist in;
  in.linear.x = 0.2;
  in.linear.y = -0.1;
  in.linear.z = 0.3;
  in.angular.x = 0.4;
  in.angular.y = -0.5;
  in.angular.z = 0.6;
  const robolink::Twist out = rl_bridge::to_transport_twist(in, 9);
  EXPECT_EQ(out.header().seq(), 9u);
  EXPECT_DOUBLE_EQ(out.linear_x(), 0.2);
  EXPECT_DOUBLE_EQ(out.linear_y(), -0.1);
  EXPECT_DOUBLE_EQ(out.linear_z(), 0.3);
  EXPECT_DOUBLE_EQ(out.angular_x(), 0.4);
  EXPECT_DOUBLE_EQ(out.angular_y(), -0.5);
  EXPECT_DOUBLE_EQ(out.angular_z(), 0.6);
}

TEST(Convert, OdometryYawAndStamp) {
  robolink::Odometry in;
  in.mutable_header()->set_timestamp_ns(1500000000123456789ull);
  in.set_x(1.5);
  in.set_y(-0.25);
  in.set_yaw(kPi / 2.0);
  in.set_vx(0.4);
  in.set_vyaw(-0.2);
  const nav_msgs::msg::Odometry out = rl_bridge::to_ros_odometry(in, "odom", "base_link");
  EXPECT_EQ(out.header.stamp.sec, 1500000000);
  EXPECT_EQ(out.header.stamp.nanosec, 123456789u);
  EXPECT_EQ(out.header.frame_id, "odom");
  EXPECT_EQ(out.child_frame_id, "base_link");
  EXPECT_DOUBLE_EQ(out.pose.pose.position.x, 1.5);
  EXPECT_DOUBLE_EQ(out.pose.pose.position.y, -0.25);
  EXPECT_NEAR(out.pose.pose.orientation.z, std::sin(kPi / 4.0), 1e-12);
  EXPECT_NEAR(out.pose.pose.orientation.w, std::cos(kPi / 4.0), 1e-12);
  EXPECT_DOUBLE_EQ(out.pose.pose.orientation.x, 0.0);
  EXPECT_DOUBLE_EQ(out.pose.pose.orientation.y, 0.0);
  EXPECT_DOUBLE_EQ(out.twist.twist.linear.x, 0.4);
  EXPECT_DOUBLE_EQ(out.twist.twist.angular.z, -0.2);
}

TEST(Convert, UdsTwistRoundTrip) {
  const std::string path = socket_path("twist");
  auto publisher = rl::make_publisher(rl::Backend::kUds, path);
  auto subscriber = rl::make_subscriber(rl::Backend::kUds, path);
  Inbox<robolink::Twist> inbox;
  subscriber->start([&inbox](const std::uint8_t* data, std::size_t len) {
    robolink::Twist twist;
    if (!rl::decode_message(data, len, twist)) {
      return;
    }
    inbox.push(std::move(twist));
  });
  ASSERT_TRUE(wait_subscribers(*publisher, 1));

  geometry_msgs::msg::Twist cmd;
  cmd.linear.x = 0.26;
  cmd.angular.z = -0.4;
  publisher->publish(rl_bridge::to_transport_twist(cmd, 4));

  ASSERT_TRUE(inbox.wait_for(1, std::chrono::seconds(3)));
  const auto got = inbox.snapshot();
  ASSERT_EQ(got.size(), 1u);
  EXPECT_EQ(got[0].header().seq(), 4u);
  EXPECT_DOUBLE_EQ(got[0].linear_x(), 0.26);
  EXPECT_DOUBLE_EQ(got[0].angular_z(), -0.4);
  EXPECT_GT(got[0].header().timestamp_ns(), 0u);
  EXPECT_TRUE(rl::verify_message(got[0]));
  subscriber->stop();
}

TEST(Convert, UdsOdometryRoundTrip) {
  const std::string path = socket_path("odom");
  auto publisher = rl::make_publisher(rl::Backend::kUds, path);
  auto subscriber = rl::make_subscriber(rl::Backend::kUds, path);
  Inbox<nav_msgs::msg::Odometry> inbox;
  subscriber->start([&inbox](const std::uint8_t* data, std::size_t len) {
    robolink::Odometry odom;
    if (!rl::decode_message(data, len, odom)) {
      return;
    }
    inbox.push(rl_bridge::to_ros_odometry(odom, "odom", "base_link"));
  });
  ASSERT_TRUE(wait_subscribers(*publisher, 1));

  robolink::Odometry odom;
  odom.mutable_header()->set_seq(3);
  odom.set_x(0.5);
  odom.set_y(0.25);
  odom.set_yaw(0.0);
  odom.set_vx(0.1);
  odom.set_vyaw(0.2);
  publisher->publish(odom);

  ASSERT_TRUE(inbox.wait_for(1, std::chrono::seconds(3)));
  const auto got = inbox.snapshot();
  ASSERT_EQ(got.size(), 1u);
  EXPECT_DOUBLE_EQ(got[0].pose.pose.position.x, 0.5);
  EXPECT_DOUBLE_EQ(got[0].pose.pose.position.y, 0.25);
  EXPECT_NEAR(got[0].pose.pose.orientation.w, 1.0, 1e-12);
  EXPECT_NEAR(got[0].pose.pose.orientation.z, 0.0, 1e-12);
  EXPECT_DOUBLE_EQ(got[0].twist.twist.linear.x, 0.1);
  EXPECT_DOUBLE_EQ(got[0].twist.twist.angular.z, 0.2);
  subscriber->stop();
}
