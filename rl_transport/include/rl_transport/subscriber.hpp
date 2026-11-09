#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "rl_transport/backend.hpp"

namespace rl {

class Subscriber {
 public:
  // 在该后端的 IO 线程里调用。不要在回调里 stop() 或析构订阅端，否则会自己等自己。
  using Callback = std::function<void(const std::uint8_t* data, std::size_t len)>;

  virtual ~Subscriber() = default;
  virtual void start(Callback callback) = 0;
  virtual void stop() = 0;
};

std::unique_ptr<Subscriber> make_subscriber(Backend backend, const std::string& endpoint);

}  // namespace rl
