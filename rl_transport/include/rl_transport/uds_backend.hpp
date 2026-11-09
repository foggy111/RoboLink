#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "rl_transport/buffer.hpp"
#include "rl_transport/event_loop.hpp"
#include "rl_transport/publisher.hpp"
#include "rl_transport/subscriber.hpp"
#include "rl_transport/unique_fd.hpp"

namespace rl {

// 发布端 bind 一个 Unix 域流套接字，接受多个订阅者并把同一帧写给所有人。
// 没有订阅者时 publish 直接丢掉，后来连上的订阅者收不到更早的消息。
class UdsPublisher : public Publisher {
 public:
  explicit UdsPublisher(std::string path);
  ~UdsPublisher() override;

  UdsPublisher(const UdsPublisher&) = delete;
  UdsPublisher& operator=(const UdsPublisher&) = delete;

  void publish_raw(const std::uint8_t* data, std::size_t len) override;
  int subscriber_count() const override;

 private:
  struct Client {
    int fd{-1};
    std::vector<std::uint8_t> out;
    bool bad{false};
  };

  void thread_main();
  void on_accept();
  void on_wake();
  void on_client(int fd, std::uint32_t events);
  void flush_all();
  void drop_client(int fd);
  void wake();

  std::string path_;
  UniqueFd listen_fd_;
  UniqueFd wake_fd_;
  EventLoop loop_;
  mutable std::mutex mu_;
  std::vector<Client> clients_;
  std::atomic<bool> running_{false};
  std::thread io_;
};

// 订阅端连到发布端的路径，把读到的字节交给 Buffer，再按帧回调。
class UdsSubscriber : public Subscriber {
 public:
  explicit UdsSubscriber(std::string path);
  ~UdsSubscriber() override;

  UdsSubscriber(const UdsSubscriber&) = delete;
  UdsSubscriber& operator=(const UdsSubscriber&) = delete;

  void start(Callback callback) override;
  void stop() override;

 private:
  void thread_main();
  void on_events(std::uint32_t events);
  void drain_wake();

  std::string path_;
  Callback callback_;
  Buffer buffer_;
  UniqueFd conn_;
  UniqueFd wake_fd_;
  EventLoop loop_;
  std::mutex mu_;
  std::atomic<bool> running_{false};
  std::thread io_;
};

}  // namespace rl
