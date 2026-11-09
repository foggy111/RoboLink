#include "rl_transport/event_loop.hpp"

#include <sys/epoll.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace rl {

EventLoop::EventLoop() {
  epoll_fd_.reset(::epoll_create1(EPOLL_CLOEXEC));
  if (!epoll_fd_) {
    throw std::runtime_error(std::string("epoll_create1 失败: ") + std::strerror(errno));
  }
}

EventLoop::~EventLoop() = default;

void EventLoop::add(int fd, std::uint32_t events, Handler handler) {
  epoll_event ev{};
  ev.events = events;
  ev.data.fd = fd;
  if (::epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, fd, &ev) < 0) {
    throw std::runtime_error(std::string("epoll_ctl ADD 失败: ") + std::strerror(errno));
  }
  std::lock_guard<std::mutex> lock(mu_);
  handlers_[fd] = std::move(handler);
}

bool EventLoop::mod(int fd, std::uint32_t events) {
  epoll_event ev{};
  ev.events = events;
  ev.data.fd = fd;
  return ::epoll_ctl(epoll_fd_.get(), EPOLL_CTL_MOD, fd, &ev) == 0;
}

void EventLoop::del(int fd) {
  // 删除时内核不看 event 指针，传空即可。fd 已经关掉也不算错误。
  ::epoll_ctl(epoll_fd_.get(), EPOLL_CTL_DEL, fd, nullptr);
  std::lock_guard<std::mutex> lock(mu_);
  handlers_.erase(fd);
}

int EventLoop::poll(int timeout_ms) {
  epoll_event events[32];
  const int n = ::epoll_wait(epoll_fd_.get(), events, 32, timeout_ms);
  if (n < 0) {
    if (errno == EINTR) {
      return 0;
    }
    throw std::runtime_error(std::string("epoll_wait 失败: ") + std::strerror(errno));
  }
  for (int i = 0; i < n; ++i) {
    Handler handler;
    {
      std::lock_guard<std::mutex> lock(mu_);
      const auto it = handlers_.find(events[i].data.fd);
      if (it != handlers_.end()) {
        handler = it->second;
      }
    }
    if (handler) {
      handler(events[i].events);
    }
  }
  return n;
}

}  // namespace rl
