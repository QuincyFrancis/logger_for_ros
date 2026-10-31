cmake_minimum_required(VERSION 3.8)
project(demo)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(common_logger REQUIRED)

add_executable(demo_node
  src/demo_node.cpp)

ament_target_dependencies(demo_node
  rclcpp
)

# common_logger 导出的现代 CMake target 带命名空间（common_logger::common_logger），
# 直接链接即可（传递带出 spdlog/fmt）。
target_link_libraries(demo_node
  common_logger::common_logger
)

install(TARGETS demo_node
  DESTINATION lib/${PROJECT_NAME})

ament_package()
