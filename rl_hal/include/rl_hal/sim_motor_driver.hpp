#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace rl::hal {

// 内存里的电机驱动。能组出合法的轮速帧，但没有串口。
// TODO(M3): 用 posix_openpt 或真实设备把 last_tx() 写出去，并解析编码器回包。
class SimMotorDriver {
 public:
  explicit SimMotorDriver(std::string device);

  bool open();
  void close();
  bool is_open() const;
  const std::string& device() const;

  void set_wheel_speed(std::int16_t left_mm_s, std::int16_t right_mm_s);
  const std::vector<std::uint8_t>& last_tx() const;

 private:
  std::string device_;
  bool open_{false};
  std::vector<std::uint8_t> last_tx_;
};

}  // namespace rl::hal
