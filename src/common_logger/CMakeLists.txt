cmake_minimum_required(VERSION 3.8)
project(common_logger)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(spdlog REQUIRED)

add_library(common_logger SHARED
  src/common_logger.cpp
  src/common_recorder.cpp)

target_include_directories(common_logger PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(common_logger PUBLIC cxx_std_17)

# spdlog 使用现代 CMake target（apt 的 spdlog 不提供 *_INCLUDE_DIRS / *_LIBRARIES
# 变量，不能用 ament_target_dependencies 去链接），会传递带出 fmt。
target_link_libraries(common_logger
  spdlog::spdlog
)

install(
  TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}Targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

install(
  DIRECTORY include/
  DESTINATION include
)

ament_export_targets(${PROJECT_NAME}Targets HAS_LIBRARY_TARGET)
ament_export_dependencies(spdlog)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
