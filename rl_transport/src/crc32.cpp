#include "rl_transport/crc32.hpp"

namespace rl {

std::uint32_t crc32_ieee(const std::uint8_t* data, std::size_t len) {
  // 按位计算，方便对照多项式。消息很小，M1 不需要查表。
  std::uint32_t crc = 0xffffffffu;
  for (std::size_t i = 0; i < len; ++i) {
    crc ^= data == nullptr ? 0 : data[i];
    for (int bit = 0; bit < 8; ++bit) {
      const std::uint32_t mask = -(crc & 1u);
      crc = (crc >> 1) ^ (0xedb88320u & mask);
    }
  }
  return ~crc;
}

}  // namespace rl
