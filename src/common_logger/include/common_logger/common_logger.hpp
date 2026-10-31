#ifndef COMMON_LOGGER_HPP
#define COMMON_LOGGER_HPP

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <fcntl.h>        // O_WRONLY / O_APPEND / O_CREAT / O_TRUNC
#include <unistd.h>       // write / close / fsync
#include <sys/file.h>     // flock
#include <filesystem>     // create_directories
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>        // std::forward

namespace common_logger {

/// 多进程安全的文件 sink。
/// 进程内线程安全由 base_sink<std::mutex> 保证；跨进程安全由 flock 保证。
/// 用 raw fd 直接 write()（不用 stdio 缓冲），确保写入发生在锁内。
class locked_file_sink : public spdlog::sinks::base_sink<std::mutex> {
 public:
  /// @param path     日志文件路径，父目录不存在时自动创建
  /// @param truncate true 清空已有文件，false 追加（默认）
  explicit locked_file_sink(const std::string& path, bool truncate = false);

  ~locked_file_sink() override;

 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override;
  void flush_() override;

 private:
  void open_file(const std::string& path, bool truncate);
  void close_file();

  int fd_ = -1;   ///< 日志文件描述符，同时作为 flock 的对象
};

/// 日志单例：按名字区分，同名只创建一次，不同名可共存。
/// 一个 Logger 在创建时绑定一个文件，之后不可更改路径。
class Logger {
 public:
  /// 获取指定名字的日志实例（首次调用创建，之后返回同一实例）
  /// @param name      实例名，同时决定默认日志子目录名
  /// @param log_dir   日志目录；空则用 ./my_logs/<name>（相对工作区根）
  /// @param file_name 日志文件名；空则用 <yyyy-MM-dd-hh-mm-ss>.txt
  /// @return shared_ptr，多线程可安全共享
  static std::shared_ptr<Logger> getInstance(
    const std::string& name = "default",
    const std::string& log_dir = "",
    const std::string& file_name = "");

  /// @name 普通文本接口
  /// msg 作为普通文本输出，不做格式串解析，含 {} 不会抛异常
  /// @{
  void debug(const std::string& msg);
  void info(const std::string& msg);
  void warn(const std::string& msg);
  void error(const std::string& msg);
  void critical(const std::string& msg);
  /// @}

  /// @name 格式化接口
  /// 用 fmt 占位符 {}，如 logger->info("x={} y={}", x, y)
  /// @{
  template <typename... Args>
  void debug(const std::string& fmt, Args&&... args) {
    logger_->debug(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  void info(const std::string& fmt, Args&&... args) {
    logger_->info(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  void warn(const std::string& fmt, Args&&... args) {
    logger_->warn(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  void error(const std::string& fmt, Args&&... args) {
    logger_->error(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  void critical(const std::string& fmt, Args&&... args) {
    logger_->critical(fmt, std::forward<Args>(args)...);
  }
  /// @}

  /// 设置日志级别，低于该级别的日志被丢弃
  void setLevel(spdlog::level::level_enum level);

  /// 显式 fsync 落盘（昂贵，仅退出/关键节点手动调用）
  void flush();

 private:
  Logger(const std::string& name, const std::string& log_dir, const std::string& file_name);

  std::shared_ptr<spdlog::logger> logger_;          ///< spdlog 日志对象

  static std::shared_mutex mutex_;                  ///< 保护 instances_（读共享/写独占）
  static std::unordered_map<std::string, std::shared_ptr<Logger>> instances_;  ///< 名字 -> 实例
};

}  // namespace common_logger

#endif  // COMMON_LOGGER_HPP
