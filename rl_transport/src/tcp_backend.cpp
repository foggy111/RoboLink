#include "rl_transport/tcp_backend.hpp"

namespace rl {

TcpPublisher::TcpPublisher(std::string endpoint) : endpoint_(std::move(endpoint)) {}

TcpPublisher::~TcpPublisher() = default;

void TcpPublisher::publish_raw(const std::uint8_t*, std::size_t) {
  throw std::logic_error("TODO(M2): TCP 后端尚未实现，终点 " + endpoint_);
}

TcpSubscriber::TcpSubscriber(std::string endpoint) : endpoint_(std::move(endpoint)) {}

TcpSubscriber::~TcpSubscriber() = default;

void TcpSubscriber::start(Callback) {
  throw std::logic_error("TODO(M2): TCP 后端尚未实现，终点 " + endpoint_);
}

void TcpSubscriber::stop() {}

}  // namespace rl
