#include <stdexcept>

#include "rl_transport/publisher.hpp"
#include "rl_transport/shm_backend.hpp"
#include "rl_transport/subscriber.hpp"
#include "rl_transport/tcp_backend.hpp"
#include "rl_transport/uds_backend.hpp"

namespace rl {

std::unique_ptr<Publisher> make_publisher(Backend backend, const std::string& endpoint) {
  switch (backend) {
    case Backend::kUds:
      return std::make_unique<UdsPublisher>(endpoint);
    case Backend::kTcp:
      return std::make_unique<TcpPublisher>(endpoint);
    case Backend::kShm:
      return std::make_unique<ShmPublisher>(endpoint);
  }
  throw std::invalid_argument("未知的发布后端");
}

std::unique_ptr<Subscriber> make_subscriber(Backend backend, const std::string& endpoint) {
  switch (backend) {
    case Backend::kUds:
      return std::make_unique<UdsSubscriber>(endpoint);
    case Backend::kTcp:
      return std::make_unique<TcpSubscriber>(endpoint);
    case Backend::kShm:
      return std::make_unique<ShmSubscriber>(endpoint);
  }
  throw std::invalid_argument("未知的订阅后端");
}

}  // namespace rl
