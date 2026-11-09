#include <cstdint>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "rl_transport/buffer.hpp"

namespace {

std::vector<std::uint8_t> raw_frame(const std::string& payload) {
  const auto len = static_cast<std::uint32_t>(payload.size());
  std::vector<std::uint8_t> bytes = {
      static_cast<std::uint8_t>((len >> 24) & 0xff),
      static_cast<std::uint8_t>((len >> 16) & 0xff),
      static_cast<std::uint8_t>((len >> 8) & 0xff),
      static_cast<std::uint8_t>(len & 0xff),
  };
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  return bytes;
}

}  // namespace

TEST(Buffer, HalfPacketAcrossLengthAndPayload) {
  rl::Buffer buffer;
  std::vector<std::uint8_t> payload;

  const std::uint8_t part1[] = {0x00, 0x00};
  buffer.append(part1, sizeof(part1));
  EXPECT_EQ(buffer.next_frame(payload), rl::Buffer::Status::kNeedMore);

  const std::uint8_t part2[] = {0x00, 0x05, 'h', 'e'};
  buffer.append(part2, sizeof(part2));
  EXPECT_EQ(buffer.next_frame(payload), rl::Buffer::Status::kNeedMore);

  const std::uint8_t part3[] = {'l', 'l', 'o'};
  buffer.append(part3, sizeof(part3));
  ASSERT_EQ(buffer.next_frame(payload), rl::Buffer::Status::kOk);
  EXPECT_EQ(std::string(payload.begin(), payload.end()), "hello");
  EXPECT_EQ(buffer.size(), 0u);
  EXPECT_EQ(buffer.next_frame(payload), rl::Buffer::Status::kNeedMore);
}

TEST(Buffer, OneByteAtATime) {
  const auto frame = raw_frame("xyz");
  rl::Buffer buffer;
  std::vector<std::uint8_t> payload;
  for (std::size_t i = 0; i + 1 < frame.size(); ++i) {
    buffer.append(&frame[i], 1);
    EXPECT_EQ(buffer.next_frame(payload), rl::Buffer::Status::kNeedMore);
  }
  buffer.append(&frame.back(), 1);
  ASSERT_EQ(buffer.next_frame(payload), rl::Buffer::Status::kOk);
  EXPECT_EQ(std::string(payload.begin(), payload.end()), "xyz");
}

TEST(Buffer, StickyTwoFrames) {
  auto bytes = raw_frame("ab");
  const auto second = raw_frame("xyz");
  bytes.insert(bytes.end(), second.begin(), second.end());

  rl::Buffer buffer;
  buffer.append(bytes.data(), bytes.size());
  std::vector<std::uint8_t> payload;
  ASSERT_EQ(buffer.next_frame(payload), rl::Buffer::Status::kOk);
  EXPECT_EQ(std::string(payload.begin(), payload.end()), "ab");
  ASSERT_EQ(buffer.next_frame(payload), rl::Buffer::Status::kOk);
  EXPECT_EQ(std::string(payload.begin(), payload.end()), "xyz");
  EXPECT_EQ(buffer.next_frame(payload), rl::Buffer::Status::kNeedMore);
}

TEST(Buffer, StickyPlusPartialNextFrame) {
  auto bytes = raw_frame("Q");
  const std::uint8_t partial[] = {0x00, 0x00, 0x00, 0x02, 'Z'};
  bytes.insert(bytes.end(), partial, partial + sizeof(partial));

  rl::Buffer buffer;
  buffer.append(bytes.data(), bytes.size());
  std::vector<std::uint8_t> payload;
  ASSERT_EQ(buffer.next_frame(payload), rl::Buffer::Status::kOk);
  EXPECT_EQ(payload, std::vector<std::uint8_t>({'Q'}));
  EXPECT_EQ(buffer.next_frame(payload), rl::Buffer::Status::kNeedMore);

  const std::uint8_t rest[] = {'!'};
  buffer.append(rest, sizeof(rest));
  ASSERT_EQ(buffer.next_frame(payload), rl::Buffer::Status::kOk);
  EXPECT_EQ(std::string(payload.begin(), payload.end()), "Z!");
}

TEST(Buffer, ZeroLengthPayload) {
  const std::uint8_t bytes[] = {0, 0, 0, 0};
  rl::Buffer buffer;
  buffer.append(bytes, sizeof(bytes));
  std::vector<std::uint8_t> payload = {1, 2, 3};
  ASSERT_EQ(buffer.next_frame(payload), rl::Buffer::Status::kOk);
  EXPECT_TRUE(payload.empty());
}

TEST(Buffer, OversizedLengthIsStickyError) {
  // 16MiB，超过 1MiB 上限。不消费，避免错误地继续解析。
  const std::uint8_t bytes[] = {0x01, 0x00, 0x00, 0x00};
  rl::Buffer buffer;
  buffer.append(bytes, sizeof(bytes));
  std::vector<std::uint8_t> payload;
  EXPECT_EQ(buffer.next_frame(payload), rl::Buffer::Status::kError);
  EXPECT_EQ(buffer.next_frame(payload), rl::Buffer::Status::kError);
  EXPECT_EQ(buffer.size(), 4u);
}
