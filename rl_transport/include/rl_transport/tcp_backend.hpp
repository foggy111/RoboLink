#pragma once

#include <stdexcept>
#include <string>

#include "rl_transport/publisher.hpp"
#include "rl_transport/subscriber.hpp"

namespace rl {

// TODO(M2): TCP 后端。
// 终点格式 host:port。非阻塞 connect、长度前缀和 Buffer 都可以复用 UDS 这条路径，
// 差别主要是地址族、端口复用，以及订阅端要能重连。
class TcpPublisher : public Publisher {
 public:
  explicit TcpPublisher(std::string endpoint);
  ~TcpPublisher() override;

  void publish_raw(const std::uint8_t* data, std::size_t len) override;

 private:
  std::string endpoint_;
};

class TcpSubscriber : public Subscriber {
 public:
  explicit TcpSubscriber(std::string endpoint);
  ~TcpSubscriber() override;

  void start(Callback callback) override;
  void stop() override;

 private:
  std::string endpoint_;
};

}  // namespace rl
