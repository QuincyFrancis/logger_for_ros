cmake_minimum_required(VERSION 3.8)
project(demo)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(common_logger REQUIRED)

add_executable(logger_demo
  src/logger_demo.cpp)

ament_target_dependencies(logger_demo
  rclcpp
)

add_executable(recorder_demo
  src/recorder_demo.cpp)

ament_target_dependencies(recorder_demo
  rclcpp
)

# common_logger 导出的现代 CMake target 带命名空间（common_logger::common_logger），
# 直接链接即可（传递带出 spdlog/fmt）。
target_link_libraries(logger_demo
  common_logger::common_logger
)

target_link_libraries(recorder_demo
  common_logger::common_logger
)

install(TARGETS
  logger_demo
  recorder_demo
  DESTINATION lib/${PROJECT_NAME})

ament_package()
