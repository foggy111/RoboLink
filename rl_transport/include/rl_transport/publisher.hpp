#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include "rl_transport/backend.hpp"
#include "rl_transport/codec.hpp"

namespace rl {

class Publisher {
 public:
  virtual ~Publisher() = default;

  virtual void publish_raw(const std::uint8_t* data, std::size_t len) = 0;

  // 当前已连接的订阅者数量。没有这个概念的后端返回 0。
  virtual int subscriber_count() const { return 0; }

  // 按值接收，这样调用方还能继续改自己的那份消息。
  // seal 会覆盖 timestamp_ns 和 crc32，seq 由调用方填写。
  template <typename Msg>
  void publish(Msg msg) {
    seal_message(msg);
    std::string bytes;
    if (!msg.SerializeToString(&bytes)) {
      throw std::runtime_error("Protobuf 序列化失败");
    }
    publish_raw(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
  }
};

// endpoint：UDS 是套接字文件路径，TCP 预留 host:port，共享内存预留名字。
std::unique_ptr<Publisher> make_publisher(Backend backend, const std::string& endpoint);

}  // namespace rl
