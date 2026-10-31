#include "common_logger/common_logger.hpp"

#include <rclcpp/rclcpp.hpp>

// 演示 common_logger::Logger 的用法：
//   5 级日志 + fmt 格式化 + 普通文本 + 级别过滤 + 显式落盘。
//
// 运行：  ros2 run demo logger_demo
// 输出：  终端彩色日志 + 文件 ./my_logs/logger_demo/<yyyy-MM-dd-hh-mm-ss>.txt
//
// 说明：
//   - getInstance(name) 取单例，同名只建一次
//   - {} 是 fmt 占位符；普通接口（单字符串）按普通文本输出，含 {} 不崩溃
//   - setLevel 过滤低于该级别的日志
int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("logger_demo_node");

  // 取 Logger 实例（默认写到 ./my_logs/logger_demo/）
  auto logger = common_logger::Logger::getInstance("logger_demo");

  // 5 级日志 + 模板格式化
  logger->debug("debug level, value={}", 1);
  logger->info("info level, value={}", 2);
  logger->warn("warn level, value={}", 3);
  logger->error("error level, value={}", 4);
  logger->critical("critical level, value={}", 5);

  // 普通接口：字符串作为普通文本，含 {} 不会崩溃
  logger->info("literal braces { } safe");

  // 级别过滤：调成 warn 后，debug/info 不再输出
  logger->setLevel(spdlog::level::warn);
  logger->info("this won't print");
  logger->warn("this will print");

  logger->flush();   // 显式 fsync 落盘（可选）
  rclcpp::shutdown();
  return 0;
}
