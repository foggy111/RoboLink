#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>

#include "rl_transport/unique_fd.hpp"

namespace rl {

// 水平触发的 epoll 循环。
// 水平触发会在条件还成立时反复唤醒。写端缓冲区空了必须去掉 EPOLLOUT，
// 否则线程会空转。边沿触发更省唤醒，但也更容易漏掉「没读完」的数据，
// 所以 M1 先用水平触发。
//
// poll 只应在一个线程里调用。回调里可以 add/del，内部会先把回调拷出来再执行。
class EventLoop {
 public:
  using Handler = std::function<void(std::uint32_t events)>;

  EventLoop();
  ~EventLoop();

  EventLoop(const EventLoop&) = delete;
  EventLoop& operator=(const EventLoop&) = delete;

  void add(int fd, std::uint32_t events, Handler handler);
  bool mod(int fd, std::uint32_t events);
  void del(int fd);

  // timeout_ms < 0 表示一直等。被信号打断时返回 0。
  int poll(int timeout_ms);

 private:
  UniqueFd epoll_fd_;
  std::mutex mu_;
  std::unordered_map<int, Handler> handlers_;
};

}  // namespace rl
