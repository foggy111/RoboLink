#pragma once

#include <stdexcept>
#include <string>

#include "rl_transport/publisher.hpp"
#include "rl_transport/subscriber.hpp"

namespace rl {

// TODO(M2): POSIX 共享内存环形缓冲。
// 设想的布局（还没写）：
//   shm_open + mmap 一块固定大小的区域。
//   头部放 write_seq（原子 uint64）、槽位数和槽大小。
//   每个槽是 length + payload，写完后再发布序号，读者用自己的 read_seq 追。
//   M1 先做单写多读；慢读者被覆盖时要能发现序号跳跃，而不是读到半帧。
// 现在构造只保存名字，真正收发会抛 logic_error。
class ShmPublisher : public Publisher {
 public:
  explicit ShmPublisher(std::string name);
  ~ShmPublisher() override;

  void publish_raw(const std::uint8_t* data, std::size_t len) override;

 private:
  std::string name_;
};

class ShmSubscriber : public Subscriber {
 public:
  explicit ShmSubscriber(std::string name);
  ~ShmSubscriber() override;

  void start(Callback callback) override;
  void stop() override;

 private:
  std::string name_;
};

}  // namespace rl
