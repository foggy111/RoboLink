// 发布端示例：往一个 UDS 路径上周期发送 robolink.Twist。
// 先开订阅端或后开都可以。订阅端会重试大约 30 秒。

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "rl_transport/publisher.hpp"
#include "twist.pb.h"

namespace {

volatile std::sig_atomic_t g_stop = 0;

void on_signal(int) {
  g_stop = 1;
}

void print_usage(const char* argv0) {
  std::cerr << "用法: " << argv0 << " [--path PATH] [--hz HZ] [--count N]\n"
            << "  --path   Unix 套接字路径，默认 /tmp/robolink.sock\n"
            << "  --hz     发送频率，默认 10\n"
            << "  --count  发送条数，0 表示一直发到 Ctrl-C，默认 0\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string path = "/tmp/robolink.sock";
  double hz = 10.0;
  int count = 0;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto next = [&](const char* flag) -> const char* {
      if (i + 1 >= argc) {
        std::cerr << "缺少参数值: " << flag << '\n';
        return nullptr;
      }
      return argv[++i];
    };
    if (arg == "--help" || arg == "-h") {
      print_usage(argv[0]);
      return 0;
    }
    if (arg == "--path") {
      const char* value = next("--path");
      if (value == nullptr) {
        return 2;
      }
      path = value;
    } else if (arg == "--hz") {
      const char* value = next("--hz");
      if (value == nullptr) {
        return 2;
      }
      hz = std::strtod(value, nullptr);
    } else if (arg == "--count") {
      const char* value = next("--count");
      if (value == nullptr) {
        return 2;
      }
      count = std::atoi(value);
    } else {
      std::cerr << "未知参数: " << arg << '\n';
      print_usage(argv[0]);
      return 2;
    }
  }
  if (hz <= 0.0) {
    std::cerr << "--hz 必须大于 0\n";
    return 2;
  }

  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  try {
    auto publisher = rl::make_publisher(rl::Backend::kUds, path);
    std::cout << "[pub] 监听 " << path << " ，频率 " << hz << " Hz\n";
    const auto interval = std::chrono::duration<double>(1.0 / hz);
    std::uint64_t seq = 0;
    int sent = 0;
    while (g_stop == 0 && (count == 0 || sent < count)) {
      robolink::Twist twist;
      twist.mutable_header()->set_seq(seq);
      twist.set_linear_x(0.1 * static_cast<double>(seq % 10));
      twist.set_angular_z(0.05);
      publisher->publish(twist);
      std::cout << "[pub] seq=" << seq << " linear_x=" << twist.linear_x()
                << " angular_z=" << twist.angular_z() << " 订阅者=" << publisher->subscriber_count()
                << '\n';
      ++seq;
      ++sent;
      std::this_thread::sleep_for(interval);
    }
  } catch (const std::exception& ex) {
    std::cerr << "[pub] 失败: " << ex.what() << '\n';
    return 1;
  }
  std::cout << "[pub] 退出\n";
  return 0;
}
