#include <cstdint>
#include <string>

#include "gtest/gtest.h"
#include "heartbeat.pb.h"
#include "odometry.pb.h"
#include "rl_transport/codec.hpp"
#include "rl_transport/crc32.hpp"
#include "twist.pb.h"

TEST(Crc32, KnownVector) {
  const auto* text = reinterpret_cast<const std::uint8_t*>("123456789");
  EXPECT_EQ(rl::crc32_ieee(text, 9), 0xcbf43926u);
  EXPECT_EQ(rl::crc32_ieee(nullptr, 0), 0u);
}

TEST(HeaderCrc, SealAndVerifyTwist) {
  robolink::Twist twist;
  twist.mutable_header()->set_seq(7);
  twist.set_linear_x(1.25);
  twist.set_angular_z(-0.5);
  rl::seal_message(twist);

  EXPECT_GT(twist.header().timestamp_ns(), 0u);
  EXPECT_NE(twist.header().crc32(), 0u);
  EXPECT_TRUE(rl::verify_message(twist));

  std::string bytes;
  ASSERT_TRUE(twist.SerializeToString(&bytes));
  robolink::Twist decoded;
  ASSERT_TRUE(rl::decode_message(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size(),
                                 decoded));
  EXPECT_EQ(decoded.header().seq(), 7u);
  EXPECT_DOUBLE_EQ(decoded.linear_x(), 1.25);
  EXPECT_DOUBLE_EQ(decoded.angular_z(), -0.5);
}

TEST(HeaderCrc, TamperBreaksCrc) {
  robolink::Twist twist;
  twist.mutable_header()->set_seq(1);
  twist.set_linear_x(0.2);
  rl::seal_message(twist);
  ASSERT_TRUE(rl::verify_message(twist));

  twist.set_linear_x(0.3);
  EXPECT_FALSE(rl::verify_message(twist));

  twist.set_linear_x(0.2);
  EXPECT_TRUE(rl::verify_message(twist));
  twist.mutable_header()->set_crc32(twist.header().crc32() ^ 0x1u);
  EXPECT_FALSE(rl::verify_message(twist));
}

TEST(HeaderCrc, OdometryAndHeartbeat) {
  robolink::Odometry odom;
  odom.mutable_header()->set_seq(3);
  odom.set_x(1.0);
  odom.set_yaw(0.1);
  odom.set_vx(0.4);
  rl::seal_message(odom);
  EXPECT_TRUE(rl::verify_message(odom));
  odom.set_yaw(0.2);
  EXPECT_FALSE(rl::verify_message(odom));

  robolink::Heartbeat beat;
  beat.mutable_header()->set_seq(9);
  beat.set_node_name("rl_transport");
  beat.set_pid(42);
  beat.set_status(robolink::Heartbeat::STATUS_OK);
  rl::seal_message(beat);
  EXPECT_TRUE(rl::verify_message(beat));

  std::string bytes;
  ASSERT_TRUE(beat.SerializeToString(&bytes));
  robolink::Heartbeat decoded;
  ASSERT_TRUE(rl::decode_message(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size(),
                                 decoded));
  EXPECT_EQ(decoded.node_name(), "rl_transport");
  EXPECT_EQ(decoded.status(), robolink::Heartbeat::STATUS_OK);

  beat.set_node_name("other");
  EXPECT_FALSE(rl::verify_message(beat));
}

TEST(HeaderCrc, UnsealedDefaultDoesNotPass) {
  // 全默认消息序列化后几乎是空的。域盐保证「没签过名」不会碰巧通过。
  robolink::Twist twist;
  EXPECT_FALSE(rl::verify_message(twist));
}
