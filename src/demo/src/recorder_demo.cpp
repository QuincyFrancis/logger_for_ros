#include "common_logger/common_recorder.hpp"

#include <rclcpp/rclcpp.hpp>

#include <chrono>
#include <cmath>   // sin / cos

// 演示 common_logger::Recorder 的用法：
//   用 100Hz 定时器模拟业务回调，每拍写列值 + push() 拼一行 CSV，后台线程写盘。
//
// 运行：  ros2 run demo recorder_demo
// 输出：  ./my_records/demo/<yyyy-MM-dd-hh-mm-ss>.csv
// 验证：  head -5 ./my_records/demo/*.csv
//           time_ms,t,sin_t,cos_t,idx
//           <毫秒>,0,0,1,0
//           ...
//
// 说明：
//   - 首列 time_ms 自动为 epoch 毫秒，之后按列名顺序
//   - data 是 std::map<string,double>，未写的列 push() 时输出空字段
//   - push() 后 data 不清空，未更新列沿用上一拍的值
int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("recorder_demo");

  // 1. 建记录器：列名有序（首列自动 time_ms）
  auto rec = common_logger::Recorder::getInstance(
      "demo", {"t", "sin_t", "cos_t", "idx"});

  // 2. 每拍写值 + push（模拟 100Hz 业务回调）
  int i = 0;
  auto timer = node->create_wall_timer(std::chrono::milliseconds(10), [rec, &i]() {
    const double t = i * 0.01;
    rec->data["t"]     = t;
    rec->data["sin_t"] = std::sin(t);
    rec->data["cos_t"] = std::cos(t);
    rec->data["idx"]   = i;
    rec->push();

    if (++i >= 200) {   // 200 拍（2 秒）后退出
      rclcpp::shutdown();
    }
  });

  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
