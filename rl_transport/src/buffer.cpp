#include "rl_transport/buffer.hpp"

#include <stdexcept>

namespace rl {

void Buffer::append(const std::uint8_t* data, std::size_t len) {
  if (len == 0) {
    return;
  }
  if (data == nullptr) {
    throw std::invalid_argument("Buffer::append 数据指针为空");
  }
  compact();
  buf_.insert(buf_.end(), data, data + len);
}

Buffer::Status Buffer::next_frame(std::vector<std::uint8_t>& payload) {
  if (size() < 4) {
    return Status::kNeedMore;
  }
  const std::uint32_t len = peek_u32();
  if (len > kMaxFramePayload) {
    return Status::kError;
  }
  if (size() < static_cast<std::size_t>(4) + len) {
    return Status::kNeedMore;
  }
  const std::size_t begin = read_pos_ + 4;
  payload.assign(buf_.begin() + static_cast<std::ptrdiff_t>(begin),
                 buf_.begin() + static_cast<std::ptrdiff_t>(begin + len));
  read_pos_ += static_cast<std::size_t>(4) + len;
  if (read_pos_ == buf_.size()) {
    buf_.clear();
    read_pos_ = 0;
  }
  return Status::kOk;
}

std::size_t Buffer::size() const {
  return buf_.size() - read_pos_;
}

void Buffer::compact() {
  if (read_pos_ == 0) {
    return;
  }
  // 读指针走得比较远时把已消费的字节丢掉，避免向量只增不减。
  if (read_pos_ > 4096 || read_pos_ * 2 >= buf_.size()) {
    buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(read_pos_));
    read_pos_ = 0;
  }
}

std::uint32_t Buffer::peek_u32() const {
  const std::size_t i = read_pos_;
  return (static_cast<std::uint32_t>(buf_[i]) << 24) |
         (static_cast<std::uint32_t>(buf_[i + 1]) << 16) |
         (static_cast<std::uint32_t>(buf_[i + 2]) << 8) | static_cast<std::uint32_t>(buf_[i + 3]);
}

}  // namespace rl
