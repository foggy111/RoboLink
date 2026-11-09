#include "rclcpp/rclcpp.hpp"
#include "rl_bridge/bridge_node.hpp"

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rl_bridge::BridgeNode>();
  // 生命周期节点用 base interface 旋转，状态切换交给外部 lifecycle 客户端。
  rclcpp::spin(node->get_node_base_interface());
  rclcpp::shutdown();
  return 0;
}
