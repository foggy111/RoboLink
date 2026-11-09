#include "rl_hal/serial_frame.hpp"

#include <cstring>
#include <stdexcept>

namespace rl::hal {
namespace {

void set_error(std::string* error, const char* text) {
  if (error != nullptr) {
    *error = text;
  }
}

}  // namespace

std::uint16_t crc16_modbus(const std::uint8_t* data, std::size_t len) {
  std::uint16_t crc = 0xffff;
  for (std::size_t i = 0; i < len; ++i) {
    crc = static_cast<std::uint16_t>(crc ^ (data == nullptr ? 0 : data[i]));
    for (int bit = 0; bit < 8; ++bit) {
      if ((crc & 0x0001) != 0) {
        crc = static_cast<std::uint16_t>((crc >> 1) ^ 0xa001);
      } else {
        crc = static_cast<std::uint16_t>(crc >> 1);
      }
    }
  }
  return crc;
}

std::vector<std::uint8_t> encode_frame(std::uint8_t cmd, const std::uint8_t* payload,
                                       std::size_t len) {
  if (len > kMaxPayload) {
    throw std::invalid_argument("串口帧载荷超过 255 字节");
  }
  if (len > 0 && payload == nullptr) {
    throw std::invalid_argument("encode_frame 载荷指针为空");
  }
  std::vector<std::uint8_t> frame(6 + len);
  frame[0] = kMagic0;
  frame[1] = kMagic1;
  frame[2] = static_cast<std::uint8_t>(len);
  frame[3] = cmd;
  if (len > 0) {
    std::memcpy(frame.data() + 4, payload, len);
  }
  const std::uint16_t crc = crc16_modbus(frame.data(), 4 + len);
  frame[4 + len] = static_cast<std::uint8_t>(crc & 0xff);
  frame[5 + len] = static_cast<std::uint8_t>((crc >> 8) & 0xff);
  return frame;
}

bool decode_frame(const std::uint8_t* data, std::size_t len, Frame& out, std::string* error) {
  if (data == nullptr || len < 6) {
    set_error(error, "帧太短");
    return false;
  }
  if (data[0] != kMagic0 || data[1] != kMagic1) {
    set_error(error, "魔数不匹配");
    return false;
  }
  const std::size_t payload_len = data[2];
  if (len != 6 + payload_len) {
    set_error(error, "长度字段和缓冲区不一致");
    return false;
  }
  const std::uint16_t expect = crc16_modbus(data, 4 + payload_len);
  const std::uint16_t actual = static_cast<std::uint16_t>(data[4 + payload_len]) |
                               static_cast<std::uint16_t>(data[5 + payload_len] << 8);
  if (expect != actual) {
    set_error(error, "CRC 不匹配");
    return false;
  }
  out.cmd = data[3];
  out.payload.assign(data + 4, data + 4 + payload_len);
  return true;
}

std::vector<std::uint8_t> encode_velocity(std::int16_t left_mm_s, std::int16_t right_mm_s) {
  const std::uint8_t payload[4] = {
      static_cast<std::uint8_t>(left_mm_s & 0xff),
      static_cast<std::uint8_t>((left_mm_s >> 8) & 0xff),
      static_cast<std::uint8_t>(right_mm_s & 0xff),
      static_cast<std::uint8_t>((right_mm_s >> 8) & 0xff),
  };
  return encode_frame(kCmdSetVelocity, payload, sizeof(payload));
}

bool decode_velocity(const Frame& frame, WheelVelocity& out) {
  if (frame.cmd != kCmdSetVelocity || frame.payload.size() != 4) {
    return false;
  }
  const auto left = static_cast<std::int16_t>(frame.payload[0] |
                                              (static_cast<std::uint16_t>(frame.payload[1]) << 8));
  const auto right = static_cast<std::int16_t>(frame.payload[2] |
                                               (static_cast<std::uint16_t>(frame.payload[3]) << 8));
  out.left_mm_s = left;
  out.right_mm_s = right;
  return true;
}

void FrameParser::append(const std::uint8_t* data, std::size_t len) {
  if (len == 0) {
    return;
  }
  if (data == nullptr) {
    throw std::invalid_argument("FrameParser::append 数据指针为空");
  }
  compact();
  buf_.insert(buf_.end(), data, data + len);
}

FrameParser::Status FrameParser::next(Frame& out) {
  while (true) {
    if (size() < 4) {
      return Status::kNeedMore;
    }
    const std::uint8_t* p = buf_.data() + read_pos_;
    if (p[0] != kMagic0 || p[1] != kMagic1) {
      ++read_pos_;
      continue;
    }
    const std::size_t payload_len = p[2];
    const std::size_t total = 6 + payload_len;
    if (size() < total) {
      return Status::kNeedMore;
    }
    const std::uint16_t expect = crc16_modbus(p, total - 2);
    const std::uint16_t actual =
        static_cast<std::uint16_t>(p[total - 2]) | static_cast<std::uint16_t>(p[total - 1] << 8);
    if (expect != actual) {
      // 魔数撞上了噪声。丢掉一个字节再找，而不是把后面的流全部作废。
      ++read_pos_;
      continue;
    }
    out.cmd = p[3];
    out.payload.assign(p + 4, p + 4 + payload_len);
    read_pos_ += total;
    if (read_pos_ == buf_.size()) {
      buf_.clear();
      read_pos_ = 0;
    }
    return Status::kOk;
  }
}

void FrameParser::compact() {
  if (read_pos_ == 0) {
    return;
  }
  if (read_pos_ > 256 || read_pos_ * 2 >= buf_.size()) {
    buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(read_pos_));
    read_pos_ = 0;
  }
}

}  // namespace rl::hal
