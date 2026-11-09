#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rl {

// 单帧载荷上限。长度字段如果比这更大，直接判为坏帧，避免把内存撑爆。
inline constexpr std::size_t kMaxFramePayload = 1024 * 1024;

// 字节流缓冲。TCP/UDS 的一次 read 可能切开一帧（半包），也可能带上几帧（粘包）。
// 线格式：uint32 大端长度 + 载荷。长度不含这 4 个字节。
class Buffer {
 public:
  enum class Status {
    kNeedMore,  // 长度或载荷还没收齐
    kOk,        // 取出一帧
    kError,     // 长度非法。调用方应关掉这条连接，不要继续 next_frame
  };

  void append(const std::uint8_t* data, std::size_t len);

  // 成功时 payload 被替换成这一帧的载荷（不含长度头）。
  // kError 时不消费字节，再次调用仍会得到 kError。
  Status next_frame(std::vector<std::uint8_t>& payload);

  std::size_t size() const;

 private:
  void compact();
  std::uint32_t peek_u32() const;

  std::vector<std::uint8_t> buf_;
  std::size_t read_pos_{0};
};

}  // namespace rl
