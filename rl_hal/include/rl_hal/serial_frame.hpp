#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rl::hal {

inline constexpr std::uint8_t kMagic0 = 0xAA;
inline constexpr std::uint8_t kMagic1 = 0x55;
inline constexpr std::size_t kMaxPayload = 255;

inline constexpr std::uint8_t kCmdSetVelocity = 0x01;
inline constexpr std::uint8_t kCmdHeartbeat = 0x02;
inline constexpr std::uint8_t kCmdGetEncoder = 0x03;

struct Frame {
  std::uint8_t cmd{0};
  std::vector<std::uint8_t> payload;
};

struct WheelVelocity {
  std::int16_t left_mm_s{0};
  std::int16_t right_mm_s{0};
};

// CRC-16/MODBUS。覆盖魔数到 payload 末尾，不含 CRC 自身。详见 docs/hal_frame.md。
std::uint16_t crc16_modbus(const std::uint8_t* data, std::size_t len);

std::vector<std::uint8_t> encode_frame(std::uint8_t cmd, const std::uint8_t* payload,
                                       std::size_t len);

// data 必须刚好是一帧，多一个字节也算失败。
bool decode_frame(const std::uint8_t* data, std::size_t len, Frame& out,
                  std::string* error = nullptr);

std::vector<std::uint8_t> encode_velocity(std::int16_t left_mm_s, std::int16_t right_mm_s);
bool decode_velocity(const Frame& frame, WheelVelocity& out);

// 串口字节流上的半包、粘包和前面的噪声。CRC 不对就丢掉一个字节继续找魔数。
class FrameParser {
 public:
  enum class Status { kNeedMore, kOk };

  void append(const std::uint8_t* data, std::size_t len);
  Status next(Frame& out);

 private:
  void compact();
  std::size_t size() const { return buf_.size() - read_pos_; }

  std::vector<std::uint8_t> buf_;
  std::size_t read_pos_{0};
};

}  // namespace rl::hal
