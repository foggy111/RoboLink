#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "rl_transport/codec.hpp"
#include "rl_transport/publisher.hpp"
#include "rl_transport/subscriber.hpp"
#include "twist.pb.h"

namespace {

std::string socket_path(const char* name) {
  return "/tmp/rl_" + std::to_string(::getpid()) + "_" + name + ".sock";
}

class Inbox {
 public:
  void push(robolink::Twist twist) {
    std::lock_guard<std::mutex> lock(mu_);
    items_.push_back(std::move(twist));
    cv_.notify_all();
  }

  bool wait_for(std::size_t n, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mu_);
    return cv_.wait_for(lock, timeout, [&] { return items_.size() >= n; });
  }

  std::vector<robolink::Twist> snapshot() const {
    std::lock_guard<std::mutex> lock(mu_);
    return items_;
  }

 private:
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::vector<robolink::Twist> items_;
};

rl::Subscriber::Callback decode_into(Inbox& inbox) {
  return [&inbox](const std::uint8_t* data, std::size_t len) {
    robolink::Twist twist;
    if (!rl::decode_message(data, len, twist)) {
      return;
    }
    inbox.push(std::move(twist));
  };
}

bool wait_subscribers(rl::Publisher& publisher, int n) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    if (publisher.subscriber_count() >= n) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return publisher.subscriber_count() >= n;
}

}  // namespace

TEST(UdsPubSub, RoundTripInProcess) {
  const std::string path = socket_path("roundtrip");
  auto publisher = rl::make_publisher(rl::Backend::kUds, path);
  auto subscriber = rl::make_subscriber(rl::Backend::kUds, path);

  Inbox inbox;
  subscriber->start(decode_into(inbox));
  ASSERT_TRUE(wait_subscribers(*publisher, 1));

  robolink::Twist twist;
  twist.mutable_header()->set_seq(42);
  twist.set_linear_x(0.25);
  twist.set_angular_z(-0.5);
  publisher->publish(twist);

  ASSERT_TRUE(inbox.wait_for(1, std::chrono::seconds(3)));
  const auto got = inbox.snapshot();
  ASSERT_EQ(got.size(), 1u);
  EXPECT_EQ(got[0].header().seq(), 42u);
  EXPECT_DOUBLE_EQ(got[0].linear_x(), 0.25);
  EXPECT_DOUBLE_EQ(got[0].angular_z(), -0.5);
  EXPECT_GT(got[0].header().timestamp_ns(), 0u);
  EXPECT_TRUE(rl::verify_message(got[0]));
}

TEST(UdsPubSub, BurstSurvivesStickyReads) {
  const std::string path = socket_path("burst");
  auto publisher = rl::make_publisher(rl::Backend::kUds, path);
  auto subscriber = rl::make_subscriber(rl::Backend::kUds, path);

  Inbox inbox;
  subscriber->start(decode_into(inbox));
  ASSERT_TRUE(wait_subscribers(*publisher, 1));

  constexpr int kCount = 30;
  for (int i = 0; i < kCount; ++i) {
    robolink::Twist twist;
    twist.mutable_header()->set_seq(static_cast<std::uint64_t>(i));
    twist.set_linear_x(0.01 * i);
    publisher->publish(twist);
  }

  ASSERT_TRUE(inbox.wait_for(kCount, std::chrono::seconds(3)));
  const auto got = inbox.snapshot();
  ASSERT_EQ(got.size(), static_cast<std::size_t>(kCount));
  for (int i = 0; i < kCount; ++i) {
    EXPECT_EQ(got[static_cast<std::size_t>(i)].header().seq(), static_cast<std::uint64_t>(i));
    EXPECT_DOUBLE_EQ(got[static_cast<std::size_t>(i)].linear_x(), 0.01 * i);
  }
}

TEST(UdsPubSub, LateSubscriberMissesEarlierMessage) {
  const std::string path = socket_path("late");
  auto publisher = rl::make_publisher(rl::Backend::kUds, path);

  robolink::Twist early;
  early.mutable_header()->set_seq(1);
  early.set_linear_x(1.0);
  publisher->publish(early);

  auto subscriber = rl::make_subscriber(rl::Backend::kUds, path);
  Inbox inbox;
  subscriber->start(decode_into(inbox));
  ASSERT_TRUE(wait_subscribers(*publisher, 1));

  robolink::Twist late;
  late.mutable_header()->set_seq(2);
  late.set_linear_x(2.0);
  publisher->publish(late);

  ASSERT_TRUE(inbox.wait_for(1, std::chrono::seconds(3)));
  const auto got = inbox.snapshot();
  ASSERT_EQ(got.size(), 1u);
  EXPECT_EQ(got[0].header().seq(), 2u);
}

TEST(UdsPubSub, TwoSubscribersFanout) {
  const std::string path = socket_path("fanout");
  auto publisher = rl::make_publisher(rl::Backend::kUds, path);
  auto first = rl::make_subscriber(rl::Backend::kUds, path);
  auto second = rl::make_subscriber(rl::Backend::kUds, path);

  Inbox inbox_a;
  Inbox inbox_b;
  first->start(decode_into(inbox_a));
  second->start(decode_into(inbox_b));
  ASSERT_TRUE(wait_subscribers(*publisher, 2));

  robolink::Twist twist;
  twist.mutable_header()->set_seq(8);
  twist.set_linear_y(0.75);
  publisher->publish(twist);

  ASSERT_TRUE(inbox_a.wait_for(1, std::chrono::seconds(3)));
  ASSERT_TRUE(inbox_b.wait_for(1, std::chrono::seconds(3)));
  EXPECT_DOUBLE_EQ(inbox_a.snapshot()[0].linear_y(), 0.75);
  EXPECT_DOUBLE_EQ(inbox_b.snapshot()[0].linear_y(), 0.75);
}
