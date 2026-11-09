#pragma once

#include <cstddef>
#include <cstdint>

namespace rl {

// IEEE CRC-32，多项式 0xEDB88320，和 zlib / PNG 使用的是同一种。
// "123456789" 的结果是 0xCBF43926，空输入的结果是 0。
std::uint32_t crc32_ieee(const std::uint8_t* data, std::size_t len);

}  // namespace rl
