#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "rl_transport/buffer.hpp"

namespace rl {

// 把一块载荷包成线上的一帧：4 字节大端长度 + 载荷。
inline std::vector<std::uint8_t> encode_frame(const std::uint8_t* data, std::size_t len) {
  if (len > kMaxFramePayload) {
    throw std::invalid_argument("帧载荷超过 1MiB 上限");
  }
  if (len > 0 && data == nullptr) {
    throw std::invalid_argument("encode_frame 数据指针为空");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(4 + len);
  frame.push_back(static_cast<std::uint8_t>((len >> 24) & 0xff));
  frame.push_back(static_cast<std::uint8_t>((len >> 16) & 0xff));
  frame.push_back(static_cast<std::uint8_t>((len >> 8) & 0xff));
  frame.push_back(static_cast<std::uint8_t>(len & 0xff));
  frame.insert(frame.end(), data, data + len);
  return frame;
}

}  // namespace rl
