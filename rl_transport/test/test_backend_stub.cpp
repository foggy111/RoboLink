#include <stdexcept>

#include "gtest/gtest.h"
#include "rl_transport/publisher.hpp"
#include "rl_transport/subscriber.hpp"

TEST(BackendStub, TcpAndShmAreNotImplemented) {
  auto tcp_pub = rl::make_publisher(rl::Backend::kTcp, "127.0.0.1:9000");
  EXPECT_THROW(tcp_pub->publish_raw(nullptr, 0), std::logic_error);

  auto shm_sub = rl::make_subscriber(rl::Backend::kShm, "/rl_ring");
  EXPECT_THROW(shm_sub->start([](const std::uint8_t*, std::size_t) {}), std::logic_error);
}
