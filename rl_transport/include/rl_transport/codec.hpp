#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <stdexcept>
#include <string>

#include "rl_transport/crc32.hpp"

namespace rl {

// 域盐。proto3 会省略零值字段，一条全默认的消息序列化后是空字节，
// 而空输入的 CRC-32 恰好是 0，和「没填过 crc」分不开。盐把这种情况错开。
inline constexpr std::uint8_t kCrcDomain[] = {0x52, 0x4C, 0x01};  // 'R' 'L' 版本 1

inline std::uint64_t wall_time_ns() {
  timespec ts{};
  ::clock_gettime(CLOCK_REALTIME, &ts);
  return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ull +
         static_cast<std::uint64_t>(ts.tv_nsec);
}

// 填写发送时刻和 crc32。Msg 需要有 mutable_header()，三个示例消息都满足。
// 会序列化两次：第一次把 crc 清零用来计算，第二次才是带 crc 的线上字节。
template <typename Msg>
void seal_message(Msg& msg) {
  auto* header = msg.mutable_header();
  header->set_timestamp_ns(wall_time_ns());
  header->set_crc32(0);
  std::string raw;
  if (!msg.SerializeToString(&raw)) {
    throw std::runtime_error("Protobuf 序列化失败");
  }
  raw.append(reinterpret_cast<const char*>(kCrcDomain), sizeof(kCrcDomain));
  header->set_crc32(crc32_ieee(reinterpret_cast<const std::uint8_t*>(raw.data()), raw.size()));
}

template <typename Msg>
bool verify_message(const Msg& msg) {
  Msg copy = msg;
  const std::uint32_t expected = msg.header().crc32();
  copy.mutable_header()->set_crc32(0);
  std::string raw;
  if (!copy.SerializeToString(&raw)) {
    return false;
  }
  raw.append(reinterpret_cast<const char*>(kCrcDomain), sizeof(kCrcDomain));
  const std::uint32_t actual =
      crc32_ieee(reinterpret_cast<const std::uint8_t*>(raw.data()), raw.size());
  return actual == expected;
}

template <typename Msg>
bool decode_message(const std::uint8_t* data, std::size_t len, Msg& out) {
  if (len > static_cast<std::size_t>(0x7fffffff)) {
    return false;
  }
  if (len > 0 && data == nullptr) {
    return false;
  }
  if (!out.ParseFromArray(data, static_cast<int>(len))) {
    return false;
  }
  return verify_message(out);
}

}  // namespace rl
