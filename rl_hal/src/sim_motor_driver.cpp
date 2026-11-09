#include "rl_hal/sim_motor_driver.hpp"

#include "rl_hal/serial_frame.hpp"

namespace rl::hal {

SimMotorDriver::SimMotorDriver(std::string device) : device_(std::move(device)) {}

bool SimMotorDriver::open() {
  // 没有打开文件描述符。返回 true 只表示内存仿真可以开始组帧。
  open_ = true;
  return true;
}

void SimMotorDriver::close() {
  open_ = false;
}

bool SimMotorDriver::is_open() const {
  return open_;
}

const std::string& SimMotorDriver::device() const {
  return device_;
}

void SimMotorDriver::set_wheel_speed(std::int16_t left_mm_s, std::int16_t right_mm_s) {
  if (!open_) {
    throw std::logic_error("模拟驱动还没 open");
  }
  last_tx_ = encode_velocity(left_mm_s, right_mm_s);
}

const std::vector<std::uint8_t>& SimMotorDriver::last_tx() const {
  return last_tx_;
}

}  // namespace rl::hal
