#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "rl_hal/serial_frame.hpp"
#include "rl_hal/sim_motor_driver.hpp"

TEST(Crc16, KnownVector) {
  const auto* text = reinterpret_cast<const std::uint8_t*>("123456789");
  EXPECT_EQ(rl::hal::crc16_modbus(text, 9), 0x4b37);
}

TEST(SerialFrame, DocumentedVelocitySample) {
  const std::uint8_t sample[] = {0xAA, 0x55, 0x04, 0x01, 0x64, 0x00, 0xCE, 0xFF, 0xF8, 0x5D};
  rl::hal::Frame frame;
  std::string error;
  ASSERT_TRUE(rl::hal::decode_frame(sample, sizeof(sample), frame, &error)) << error;
  EXPECT_EQ(frame.cmd, rl::hal::kCmdSetVelocity);

  rl::hal::WheelVelocity velocity;
  ASSERT_TRUE(rl::hal::decode_velocity(frame, velocity));
  EXPECT_EQ(velocity.left_mm_s, 100);
  EXPECT_EQ(velocity.right_mm_s, -50);

  const auto encoded = rl::hal::encode_velocity(100, -50);
  ASSERT_EQ(encoded.size(), sizeof(sample));
  EXPECT_TRUE(std::equal(encoded.begin(), encoded.end(), sample));
}

TEST(SerialFrame, RejectsBadCrcAndBadMagic) {
  auto frame_bytes = rl::hal::encode_velocity(1, 2);
  frame_bytes.back() ^= 0x01;
  rl::hal::Frame frame;
  std::string error;
  EXPECT_FALSE(rl::hal::decode_frame(frame_bytes.data(), frame_bytes.size(), frame, &error));
  EXPECT_EQ(error, "CRC 不匹配");

  const std::uint8_t junk[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
  EXPECT_FALSE(rl::hal::decode_frame(junk, sizeof(junk), frame, &error));
  EXPECT_EQ(error, "魔数不匹配");
}

TEST(SerialFrame, TruncatedFrame) {
  const auto encoded = rl::hal::encode_frame(rl::hal::kCmdHeartbeat, nullptr, 0);
  ASSERT_EQ(encoded.size(), 6u);
  rl::hal::Frame frame;
  EXPECT_FALSE(rl::hal::decode_frame(encoded.data(), encoded.size() - 1, frame, nullptr));
}

TEST(FrameParser, HalfStickyAndNoise) {
  const auto first = rl::hal::encode_velocity(10, 20);
  const auto second = rl::hal::encode_frame(rl::hal::kCmdHeartbeat, nullptr, 0);

  rl::hal::FrameParser parser;
  rl::hal::Frame frame;
  const std::uint8_t noise[] = {0x00, 0x11, 0xAA};
  parser.append(noise, sizeof(noise));
  EXPECT_EQ(parser.next(frame), rl::hal::FrameParser::Status::kNeedMore);

  parser.append(first.data(), 3);
  EXPECT_EQ(parser.next(frame), rl::hal::FrameParser::Status::kNeedMore);
  parser.append(first.data() + 3, first.size() - 3);

  std::vector<std::uint8_t> sticky = second;
  parser.append(sticky.data(), sticky.size());

  ASSERT_EQ(parser.next(frame), rl::hal::FrameParser::Status::kOk);
  rl::hal::WheelVelocity velocity;
  ASSERT_TRUE(rl::hal::decode_velocity(frame, velocity));
  EXPECT_EQ(velocity.left_mm_s, 10);
  EXPECT_EQ(velocity.right_mm_s, 20);

  ASSERT_EQ(parser.next(frame), rl::hal::FrameParser::Status::kOk);
  EXPECT_EQ(frame.cmd, rl::hal::kCmdHeartbeat);
  EXPECT_TRUE(frame.payload.empty());
  EXPECT_EQ(parser.next(frame), rl::hal::FrameParser::Status::kNeedMore);
}

TEST(SimMotorDriver, MemoryFrameRoundTrip) {
  rl::hal::SimMotorDriver driver("/dev/ttyUSB0");
  EXPECT_FALSE(driver.is_open());
  EXPECT_THROW(driver.set_wheel_speed(1, 2), std::logic_error);
  EXPECT_TRUE(driver.open());
  EXPECT_EQ(driver.device(), "/dev/ttyUSB0");

  driver.set_wheel_speed(-1000, 1000);
  rl::hal::Frame frame;
  ASSERT_TRUE(rl::hal::decode_frame(driver.last_tx().data(), driver.last_tx().size(), frame));
  rl::hal::WheelVelocity velocity;
  ASSERT_TRUE(rl::hal::decode_velocity(frame, velocity));
  EXPECT_EQ(velocity.left_mm_s, -1000);
  EXPECT_EQ(velocity.right_mm_s, 1000);
}
