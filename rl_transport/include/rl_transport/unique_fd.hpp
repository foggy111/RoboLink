#pragma once

#include <unistd.h>

namespace rl {

// 关闭文件描述符的 RAII 包装。拷贝会让两个对象关闭同一个 fd，因此禁止拷贝。
class UniqueFd {
 public:
  UniqueFd() = default;
  explicit UniqueFd(int fd) : fd_(fd) {}

  ~UniqueFd() { reset(); }

  UniqueFd(const UniqueFd&) = delete;
  UniqueFd& operator=(const UniqueFd&) = delete;

  UniqueFd(UniqueFd&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

  UniqueFd& operator=(UniqueFd&& other) noexcept {
    if (this != &other) {
      reset(other.fd_);
      other.fd_ = -1;
    }
    return *this;
  }

  int get() const { return fd_; }
  explicit operator bool() const { return fd_ >= 0; }

  void reset(int fd = -1) {
    if (fd_ >= 0) {
      ::close(fd_);
    }
    fd_ = fd;
  }

  int release() {
    const int fd = fd_;
    fd_ = -1;
    return fd;
  }

 private:
  int fd_{-1};
};

}  // namespace rl
