#include "common_logger/common_logger.hpp"

#include <rclcpp/rclcpp.hpp>

#include <unistd.h>   // getpid

#include <string>

// 验证 common_logger 的 demo 节点：
//   1. 各级别日志 + 模板格式化
//   2. 普通接口安全路径（字符串含花括号不崩溃）
//   3. 跨进程：多个进程用相同 log_dir + 相同 file_name 写同一文件，flock 保证行不撕裂
//
// 用法：
//   单进程:  ros2 run demo demo_node
//   跨进程:  两个终端分别
//     ros2 run demo demo_node --ros-args -p log_dir:=/tmp/demo_logs -p file_name:=shared.log -p lines:=1000
int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("demo_node");

  node->declare_parameter<std::string>("log_dir", "");
  node->declare_parameter<std::string>("file_name", "");
  node->declare_parameter<int>("lines", 1000);

  const std::string log_dir = node->get_parameter("log_dir").as_string();
  const std::string file_name = node->get_parameter("file_name").as_string();
  const int lines = node->get_parameter("lines").as_int();

  auto logger = common_logger::Logger::getInstance("demo", log_dir, file_name);

  logger->info("=== demo start, pid={} ===", getpid());

  // 各级别 + 模板格式化
  logger->debug("debug level, value={}", 1);
  logger->info("info level, value={}", 2);
  logger->warn("warn level, value={}", 3);
  logger->error("error level, value={}", 4);
  logger->critical("critical level, value={}", 5);

  // 普通接口安全路径：命中非模板重载，msg 作为普通文本，含 {} 不抛异常
  logger->info("literal braces { } should not crash");

  // 循环写 lines 行，每行带 pid + 序号 + 固定长度 payload，用于跨进程串行化校验
  for (int i = 0; i < lines; ++i) {
    logger->info("pid={} seq={} payload=XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", getpid(), i);
  }

  logger->info("=== demo end, pid={} ===", getpid());
  logger->flush();   // 显式 fsync 落盘

  rclcpp::shutdown();
  return 0;
}
