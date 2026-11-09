#include "rl_transport/shm_backend.hpp"

namespace rl {

ShmPublisher::ShmPublisher(std::string name) : name_(std::move(name)) {}

ShmPublisher::~ShmPublisher() = default;

void ShmPublisher::publish_raw(const std::uint8_t*, std::size_t) {
  throw std::logic_error("TODO(M2): 共享内存环形缓冲尚未实现，名字 " + name_);
}

ShmSubscriber::ShmSubscriber(std::string name) : name_(std::move(name)) {}

ShmSubscriber::~ShmSubscriber() = default;

void ShmSubscriber::start(Callback) {
  throw std::logic_error("TODO(M2): 共享内存环形缓冲尚未实现，名字 " + name_);
}

void ShmSubscriber::stop() {}

}  // namespace rl
