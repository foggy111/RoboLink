#pragma once

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

namespace rl {

inline void set_nonblocking(int fd) {
  const int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    throw std::runtime_error(std::string("设置非阻塞失败: ") + std::strerror(errno));
  }
}

inline sockaddr_un make_uds_addr(const std::string& path) {
  sockaddr_un addr{};
  if (path.empty() || path.size() >= sizeof(addr.sun_path)) {
    throw std::invalid_argument("UDS 路径为空或过长: " + path);
  }
  addr.sun_family = AF_UNIX;
  std::memcpy(addr.sun_path, path.data(), path.size());
  return addr;
}

}  // namespace rl
