// accept4 / pthread_setname_np 在严格的 -std=c++17 下需要这个宏。
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "rl_transport/uds_backend.hpp"

#include <pthread.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <utility>

#include "rl_transport/frame.hpp"
#include "socket_utils.hpp"

namespace rl {
namespace {

constexpr std::size_t kMaxQueuedBytes = 4 * 1024 * 1024;

void drain_fd(int fd) {
  std::uint64_t value = 0;
  while (true) {
    const ssize_t n = ::read(fd, &value, sizeof(value));
    if (n < 0 && errno == EINTR) {
      continue;
    }
    break;
  }
}

// 连接阶段单独用一次 epoll 等「可写」，这样 connect 不会把 IO 线程堵死。
// 套接字文件还不存在时会立刻失败，由调用方隔一小段时间再试。
int connect_uds_once(const std::string& path, int timeout_ms) {
  UniqueFd fd(::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
  if (!fd) {
    return -1;
  }
  set_nonblocking(fd.get());
  const sockaddr_un addr = make_uds_addr(path);
  const int rc = ::connect(fd.get(), reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
  if (rc == 0) {
    return fd.release();
  }
  if (errno != EINPROGRESS && errno != EAGAIN) {
    return -1;
  }

  UniqueFd ep(::epoll_create1(EPOLL_CLOEXEC));
  if (!ep) {
    return -1;
  }
  epoll_event ev{};
  ev.events = EPOLLOUT;
  ev.data.fd = fd.get();
  if (::epoll_ctl(ep.get(), EPOLL_CTL_ADD, fd.get(), &ev) < 0) {
    return -1;
  }
  epoll_event out{};
  if (::epoll_wait(ep.get(), &out, 1, timeout_ms) <= 0) {
    return -1;
  }
  int so_error = 0;
  socklen_t len = sizeof(so_error);
  if (::getsockopt(fd.get(), SOL_SOCKET, SO_ERROR, &so_error, &len) < 0 || so_error != 0) {
    return -1;
  }
  return fd.release();
}

}  // namespace

UdsPublisher::UdsPublisher(std::string path) : path_(std::move(path)) {
  ::unlink(path_.c_str());

  listen_fd_.reset(::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
  if (!listen_fd_) {
    throw std::runtime_error(std::string("创建 UDS 失败: ") + std::strerror(errno));
  }
  const sockaddr_un addr = make_uds_addr(path_);
  if (::bind(listen_fd_.get(), reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
    throw std::runtime_error(std::string("bind 失败: ") + std::strerror(errno) + " path=" + path_);
  }
  if (::listen(listen_fd_.get(), 16) < 0) {
    throw std::runtime_error(std::string("listen 失败: ") + std::strerror(errno));
  }
  set_nonblocking(listen_fd_.get());

  wake_fd_.reset(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
  if (!wake_fd_) {
    throw std::runtime_error(std::string("eventfd 失败: ") + std::strerror(errno));
  }

  // 先标成运行中再启动线程。eventfd 用水平触发：哪怕线程还没把 fd 加进 epoll，
  // 计数也会留着，加进去之后立刻可读，不会丢唤醒。
  running_.store(true);
  io_ = std::thread([this] { thread_main(); });
}

UdsPublisher::~UdsPublisher() {
  running_.store(false);
  wake();
  if (io_.joinable()) {
    io_.join();
  }
  {
    std::lock_guard<std::mutex> lock(mu_);
    for (const Client& client : clients_) {
      if (client.fd >= 0) {
        ::close(client.fd);
      }
    }
    clients_.clear();
  }
  if (!path_.empty()) {
    ::unlink(path_.c_str());
  }
}

void UdsPublisher::publish_raw(const std::uint8_t* data, std::size_t len) {
  const std::vector<std::uint8_t> frame = encode_frame(data, len);
  {
    std::lock_guard<std::mutex> lock(mu_);
    for (Client& client : clients_) {
      if (client.bad) {
        continue;
      }
      if (client.out.size() + frame.size() > kMaxQueuedBytes) {
        // 订阅端读得太慢。断开它，避免发布端内存跟着涨。
        client.bad = true;
        continue;
      }
      client.out.insert(client.out.end(), frame.begin(), frame.end());
    }
  }
  wake();
}

int UdsPublisher::subscriber_count() const {
  std::lock_guard<std::mutex> lock(mu_);
  int count = 0;
  for (const Client& client : clients_) {
    if (!client.bad) {
      ++count;
    }
  }
  return count;
}

void UdsPublisher::wake() {
  if (!wake_fd_) {
    return;
  }
  const std::uint64_t one = 1;
  const ssize_t n = ::write(wake_fd_.get(), &one, sizeof(one));
  (void)n;
}

void UdsPublisher::thread_main() {
  ::pthread_setname_np(::pthread_self(), "rl_uds_pub");
  try {
    loop_.add(listen_fd_.get(), EPOLLIN, [this](std::uint32_t) { on_accept(); });
    loop_.add(wake_fd_.get(), EPOLLIN, [this](std::uint32_t) { on_wake(); });
    while (running_.load()) {
      loop_.poll(200);
    }
  } catch (const std::exception&) {
    running_.store(false);
  }
}

void UdsPublisher::on_accept() {
  while (running_.load()) {
    const int fd = ::accept4(listen_fd_.get(), nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (fd < 0) {
      if (errno == EINTR) {
        continue;
      }
      // EAGAIN：这一轮的连接已经接完。其它错误再 epoll 会立刻再次可读，容易空转。
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        running_.store(false);
      }
      return;
    }
    try {
      loop_.add(fd, EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR,
                [this, fd](std::uint32_t events) { on_client(fd, events); });
    } catch (...) {
      ::close(fd);
      return;
    }
    std::lock_guard<std::mutex> lock(mu_);
    Client client;
    client.fd = fd;
    clients_.push_back(std::move(client));
  }
}

void UdsPublisher::on_wake() {
  drain_fd(wake_fd_.get());
  flush_all();
}

void UdsPublisher::on_client(int fd, std::uint32_t events) {
  if (events & EPOLLIN) {
    std::uint8_t tmp[256];
    while (true) {
      const ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
      if (n > 0) {
        continue;
      }
      if (n < 0 && errno == EINTR) {
        continue;
      }
      if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        break;
      }
      drop_client(fd);
      return;
    }
  }
  if (events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
    drop_client(fd);
    return;
  }
  if (events & EPOLLOUT) {
    flush_all();
  }
}

void UdsPublisher::flush_all() {
  std::vector<int> dead;
  {
    std::lock_guard<std::mutex> lock(mu_);
    for (Client& client : clients_) {
      if (client.bad) {
        dead.push_back(client.fd);
        continue;
      }
      while (!client.out.empty()) {
        const ssize_t n = ::send(client.fd, client.out.data(), client.out.size(), MSG_NOSIGNAL);
        if (n < 0) {
          if (errno == EINTR) {
            continue;
          }
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
          }
          client.bad = true;
          break;
        }
        if (n == 0) {
          break;
        }
        client.out.erase(client.out.begin(), client.out.begin() + n);
      }
      if (client.bad) {
        dead.push_back(client.fd);
      }
    }
  }
  for (const int fd : dead) {
    drop_client(fd);
  }

  std::vector<std::pair<int, bool>> interest;
  {
    std::lock_guard<std::mutex> lock(mu_);
    interest.reserve(clients_.size());
    for (const Client& client : clients_) {
      interest.emplace_back(client.fd, !client.out.empty());
    }
  }
  for (const auto& item : interest) {
    std::uint32_t events = EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
    if (item.second) {
      // 只在还有数据写不出去时关心 EPOLLOUT。水平触发下一直挂着会空转。
      events |= EPOLLOUT;
    }
    loop_.mod(item.first, events);
  }
}

void UdsPublisher::drop_client(int fd) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    const auto it = std::find_if(clients_.begin(), clients_.end(),
                                 [fd](const Client& client) { return client.fd == fd; });
    if (it == clients_.end()) {
      return;
    }
    clients_.erase(it);
  }
  loop_.del(fd);
  ::close(fd);
}

UdsSubscriber::UdsSubscriber(std::string path) : path_(std::move(path)) {
  wake_fd_.reset(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
  if (!wake_fd_) {
    throw std::runtime_error(std::string("eventfd 失败: ") + std::strerror(errno));
  }
}

UdsSubscriber::~UdsSubscriber() {
  stop();
}

void UdsSubscriber::start(Callback callback) {
  stop();
  {
    std::lock_guard<std::mutex> lock(mu_);
    callback_ = std::move(callback);
    buffer_ = Buffer();
  }
  running_.store(true);
  io_ = std::thread([this] { thread_main(); });
}

void UdsSubscriber::stop() {
  running_.store(false);
  if (wake_fd_) {
    const std::uint64_t one = 1;
    const ssize_t n = ::write(wake_fd_.get(), &one, sizeof(one));
    (void)n;
  }
  if (io_.joinable()) {
    io_.join();
  }
  // 线程已经退出。把残留的 fd 从 epoll 拿掉，避免下次 start 重复 ADD。
  if (conn_) {
    loop_.del(conn_.get());
    conn_.reset();
  }
  if (wake_fd_) {
    loop_.del(wake_fd_.get());
  }
}

void UdsSubscriber::thread_main() {
  ::pthread_setname_np(::pthread_self(), "rl_uds_sub");
  try {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    int raw = -1;
    while (running_.load() && std::chrono::steady_clock::now() < deadline) {
      raw = connect_uds_once(path_, 200);
      if (raw >= 0) {
        break;
      }
    }
    if (raw < 0 || !running_.load()) {
      if (raw >= 0) {
        ::close(raw);
      }
      running_.store(false);
      return;
    }
    conn_.reset(raw);
    loop_.add(conn_.get(), EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR,
              [this](std::uint32_t events) { on_events(events); });
    loop_.add(wake_fd_.get(), EPOLLIN, [this](std::uint32_t) { drain_wake(); });
    while (running_.load()) {
      loop_.poll(200);
    }
  } catch (const std::exception&) {
    running_.store(false);
  }
}

void UdsSubscriber::drain_wake() {
  drain_fd(wake_fd_.get());
}

void UdsSubscriber::on_events(std::uint32_t events) {
  if (!conn_) {
    return;
  }
  bool peer_closed = false;
  if (events & (EPOLLIN | EPOLLHUP | EPOLLRDHUP)) {
    std::uint8_t tmp[8192];
    while (running_.load()) {
      const ssize_t n = ::recv(conn_.get(), tmp, sizeof(tmp), 0);
      if (n > 0) {
        buffer_.append(tmp, static_cast<std::size_t>(n));
        continue;
      }
      if (n < 0 && errno == EINTR) {
        continue;
      }
      if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        break;
      }
      peer_closed = true;
      break;
    }
  }

  std::vector<std::uint8_t> payload;
  while (running_.load()) {
    const Buffer::Status status = buffer_.next_frame(payload);
    if (status == Buffer::Status::kNeedMore) {
      break;
    }
    if (status == Buffer::Status::kError) {
      running_.store(false);
      return;
    }
    Callback callback;
    {
      std::lock_guard<std::mutex> lock(mu_);
      callback = callback_;
    }
    if (callback) {
      callback(payload.empty() ? nullptr : payload.data(), payload.size());
    }
  }
  if (peer_closed || (events & EPOLLERR)) {
    running_.store(false);
  }
}

}  // namespace rl
