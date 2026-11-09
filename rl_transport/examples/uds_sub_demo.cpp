// 订阅端示例：连上 UDS 发布端，校验 CRC 后把 Twist 打出来。

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "rl_transport/codec.hpp"
#include "rl_transport/subscriber.hpp"
#include "twist.pb.h"

namespace {

volatile std::sig_atomic_t g_stop = 0;

void on_signal(int) {
  g_stop = 1;
}

}  // namespace

int main(int argc, char** argv) {
  std::string path = "/tmp/robolink.sock";
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      std::cerr << "用法: " << argv[0] << " [--path PATH]\n"
                << "  --path  Unix 套接字路径，默认 /tmp/robolink.sock\n";
      return 0;
    }
    if (arg == "--path") {
      if (i + 1 >= argc) {
        std::cerr << "缺少参数值: --path\n";
        return 2;
      }
      path = argv[++i];
    } else {
      std::cerr << "未知参数: " << arg << '\n';
      return 2;
    }
  }

  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  std::mutex mu;
  try {
    auto subscriber = rl::make_subscriber(rl::Backend::kUds, path);
    subscriber->start([&](const std::uint8_t* data, std::size_t len) {
      robolink::Twist twist;
      if (!rl::decode_message(data, len, twist)) {
        std::lock_guard<std::mutex> lock(mu);
        std::cerr << "[sub] 丢弃一帧：解析或 CRC 失败\n";
        return;
      }
      std::lock_guard<std::mutex> lock(mu);
      std::cout << "[sub] seq=" << twist.header().seq() << " linear_x=" << twist.linear_x()
                << " angular_z=" << twist.angular_z() << " t_ns=" << twist.header().timestamp_ns()
                << '\n';
    });
    std::cout << "[sub] 连接 " << path << " （发布端还没起来的话会重试约 30 秒）\n";
    while (g_stop == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    subscriber->stop();
  } catch (const std::exception& ex) {
    std::cerr << "[sub] 失败: " << ex.what() << '\n';
    return 1;
  }
  std::cout << "[sub] 退出\n";
  return 0;
}
