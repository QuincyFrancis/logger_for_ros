#include "common_logger/common_logger.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>

namespace common_logger {

// ---------------------------------------------------------------------------
// locked_file_sink
// ---------------------------------------------------------------------------

locked_file_sink::locked_file_sink(const std::string& path, bool truncate) {
  open_file(path, truncate);
}

locked_file_sink::~locked_file_sink() {
  close_file();
}

void locked_file_sink::sink_it_(const spdlog::details::log_msg& msg) {
  spdlog::memory_buf_t formatted;
  formatter_->format(msg, formatted);
  flock(fd_, LOCK_EX);   // 跨进程排它锁
  const ssize_t n = ::write(fd_, formatted.data(), formatted.size());
  if (n != static_cast<ssize_t>(formatted.size())) {   // 出错或部分写（如磁盘满）
    flock(fd_, LOCK_UN);
    throw spdlog::spdlog_ex("common_logger: write failed");
  }
  flock(fd_, LOCK_UN);
}

void locked_file_sink::flush_() {
  if (fd_ >= 0) ::fsync(fd_);   // 显式落盘，仅 Logger::flush() 触发
}

void locked_file_sink::open_file(const std::string& path, bool truncate) {
  std::filesystem::path p(path);
  if (p.has_parent_path()) {
    std::filesystem::create_directories(p.parent_path());
  }
  int flags = O_WRONLY | O_CREAT | O_APPEND;
  if (truncate) flags |= O_TRUNC;
  fd_ = ::open(path.c_str(), flags, 0644);
  if (fd_ < 0) {
    throw spdlog::spdlog_ex("common_logger: open failed: " + path);
  }
}

void locked_file_sink::close_file() {
  if (fd_ >= 0) {
    flock(fd_, LOCK_UN);   // close 也会释放锁，显式释放更清晰
    ::close(fd_);
    fd_ = -1;
  }
}

// ---------------------------------------------------------------------------
// Logger
// ---------------------------------------------------------------------------

std::shared_mutex Logger::mutex_;
std::unordered_map<std::string, std::shared_ptr<Logger>> Logger::instances_;

std::shared_ptr<Logger> Logger::getInstance(
  const std::string& name, const std::string& log_dir, const std::string& file_name)
{
  {   // 读路径：共享锁，读者并发
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = instances_.find(name);
    if (it != instances_.end()) return it->second;
  }
  // 写路径：独占锁 + 二次检查
  std::unique_lock<std::shared_mutex> lock(mutex_);
  auto it = instances_.find(name);
  if (it != instances_.end()) return it->second;
  auto inst = std::shared_ptr<Logger>(new Logger(name, log_dir, file_name));
  instances_[name] = inst;
  return inst;
}

Logger::Logger(const std::string& name, const std::string& log_dir, const std::string& file_name) {
  // 文件名：显式指定，或用时间戳 <yyyy-MM-dd-hh-mm-ss>.txt 作为默认
  std::string fname = file_name;
  if (fname.empty()) {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    localtime_r(&t, &tm_now);
    std::ostringstream ss;
    ss << std::setfill('0')
       << std::setw(4) << (tm_now.tm_year + 1900) << "-"
       << std::setw(2) << (tm_now.tm_mon + 1) << "-"
       << std::setw(2) << tm_now.tm_mday << "-"
       << std::setw(2) << tm_now.tm_hour << "-"
       << std::setw(2) << tm_now.tm_min << "-"
       << std::setw(2) << tm_now.tm_sec;
    fname = ss.str() + ".txt";
  }

  std::string dir = log_dir.empty() ? "./my_logs/" + name : log_dir;
  std::filesystem::create_directories(dir);
  std::string log_file = dir + "/" + fname;

  std::vector<spdlog::sink_ptr> sinks;

  auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console->set_level(spdlog::level::debug);
  sinks.push_back(console);

  auto file_sink = std::make_shared<locked_file_sink>(log_file);
  file_sink->set_level(spdlog::level::trace);
  sinks.push_back(file_sink);

  logger_ = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
  logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%f] [%n] [%^%l%$] %v");
  logger_->set_level(spdlog::level::debug);

  logger_->info("Logger initialized, file: {}", log_file);
}

void Logger::debug(const std::string& msg)    { logger_->debug("{}", msg); }
void Logger::info(const std::string& msg)     { logger_->info("{}", msg); }
void Logger::warn(const std::string& msg)     { logger_->warn("{}", msg); }
void Logger::error(const std::string& msg)    { logger_->error("{}", msg); }
void Logger::critical(const std::string& msg) { logger_->critical("{}", msg); }

void Logger::setLevel(spdlog::level::level_enum level) { logger_->set_level(level); }

void Logger::flush() { logger_->flush(); }

}  // namespace common_logger
