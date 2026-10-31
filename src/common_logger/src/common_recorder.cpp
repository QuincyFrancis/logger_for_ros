#include "common_logger/common_recorder.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace common_logger {

// ---------------------------------------------------------------------------
// 工具函数
// ---------------------------------------------------------------------------

/// 当前本地时间戳 yyyy-MM-dd-hh-mm-ss（用作文件名）
static std::string Timestamp() {
  auto now = std::chrono::system_clock::now();
  auto t = std::chrono::system_clock::to_time_t(now);
  std::tm tm_now;
  localtime_r(&t, &tm_now);   // 线程安全版 localtime
  std::ostringstream ss;
  ss << std::setfill('0')
     << std::setw(4) << (tm_now.tm_year + 1900) << "-"
     << std::setw(2) << (tm_now.tm_mon + 1) << "-"
     << std::setw(2) << tm_now.tm_mday << "-"
     << std::setw(2) << tm_now.tm_hour << "-"
     << std::setw(2) << tm_now.tm_min << "-"
     << std::setw(2) << tm_now.tm_sec;
  return ss.str();
}

/// 当前时刻距 Unix 纪元的毫秒数（作为首列 time_ms）
static int64_t NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// ---------------------------------------------------------------------------
// Recorder
// ---------------------------------------------------------------------------

std::shared_mutex Recorder::mutex_;
std::unordered_map<std::string, std::shared_ptr<Recorder>> Recorder::instances_;

// 单例入口：读路径用共享锁（读者并发），未命中再走独占锁 + 二次检查，
// 避免两个线程同时「没查到 → 都新建」。与 Logger::getInstance 相同的双检锁写法。
std::shared_ptr<Recorder> Recorder::getInstance(
    const std::string& name,
    const std::vector<std::string>& columns,
    const std::string& file_path,
    size_t max_file_size,
    size_t max_files)
{
  {   // 读路径：共享锁
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = instances_.find(name);
    if (it != instances_.end()) return it->second;
  }
  // 写路径：独占锁 + 二次检查
  std::unique_lock<std::shared_mutex> lock(mutex_);
  auto it = instances_.find(name);
  if (it != instances_.end()) return it->second;
  auto inst = std::shared_ptr<Recorder>(
      new Recorder(name, columns, file_path, max_file_size, max_files));
  instances_[name] = inst;
  return inst;
}

// 构造：建目录 -> 开首个文件并写表头 -> 起后台写盘线程。
// OpenNewFile 必须在起线程之前完成，保证 file_ 单线程初始化、无并发。
Recorder::Recorder(
    const std::string& name,
    const std::vector<std::string>& columns,
    const std::string& file_path,
    size_t max_file_size,
    size_t max_files)
    : columns_(columns),
      max_file_size_(max_file_size),
      max_files_(max_files)
{
  dir_ = file_path.empty() ? "./my_records/" + name : file_path;
  std::filesystem::create_directories(dir_);

  OpenNewFile();

  writer_running_.store(true, std::memory_order_release);
  writer_thread_ = std::thread(&Recorder::WriterLoop, this);
}

// 析构顺序关键：先置停止标志并唤醒写线程，再 join 等它写完残余队列，最后关文件。
Recorder::~Recorder() {
  writer_running_.store(false, std::memory_order_release);
  write_cv_.notify_one();
  if (writer_thread_.joinable()) writer_thread_.join();
  if (file_.is_open()) file_.close();   // close 会自动 flush
}

// 开新时间戳文件：撞名加 _N 后缀 -> 写表头 -> 按创建顺序删超量旧文件。
void Recorder::OpenNewFile() {
  const std::string base = dir_ + "/" + Timestamp();
  std::string fname = base + ".csv";
  for (int i = 1; std::filesystem::exists(fname); ++i) {
    fname = base + "_" + std::to_string(i) + ".csv";
  }

  file_.open(fname, std::ios::out | std::ios::trunc);

  // 表头：time_ms,列1,列2,...
  std::ostringstream header;
  header << "time_ms";
  for (const auto& col : columns_) {
    header << "," << col;
  }
  header << "\n";
  file_ << header.str();
  current_size_ = header.str().size();

  // 用内存队列按创建顺序删最旧（不依赖文件名/mtime 排序，同秒撞名也正确）
  files_.push_back(fname);
  while (max_files_ > 0 && files_.size() > max_files_) {
    std::filesystem::remove(files_.front());
    files_.pop_front();
  }
}

// 滚动：关旧文件，再开新时间戳文件。
void Recorder::Rotate() {
  file_.flush();
  file_.close();
  OpenNewFile();
}

// 拼一行 CSV：首列 time_ms，按列名顺序取 data；未写列输出空字段。
// 统一 double + defaultfloat（12 位有效数字）：整数值显示 "1"，浮点显示 "3.14"，不补零。
void Recorder::push() {
  std::ostringstream line;
  line << NowMs();
  for (const auto& col : columns_) {
    line << ",";
    auto it = data.find(col);
    if (it != data.end()) {
      line << std::setprecision(12) << it->second;
    }
  }
  line << "\n";
  EnqueueLine(line.str());
}

// 入队：加锁 push，锁外 notify（减少被唤醒线程又立刻撞锁的开销）。
void Recorder::EnqueueLine(std::string line) {
  {
    std::lock_guard<std::mutex> lock(write_mutex_);
    write_queue_.push_back(std::move(line));
  }
  write_cv_.notify_one();
}

// 后台写盘线程：wait 到队列非空 -> swap 整批换出（O(1)，少持锁）-> 锁外写盘 -> 软上限滚动。
// 退出时再换出残余队列写掉，避免最后一拍数据丢失。
void Recorder::WriterLoop() {
  std::deque<std::string> local;
  while (writer_running_.load(std::memory_order_acquire)) {
    {
      std::unique_lock<std::mutex> lock(write_mutex_);
      write_cv_.wait(lock, [this]() {
        return !writer_running_.load(std::memory_order_acquire) || !write_queue_.empty();
      });
      local.swap(write_queue_);
    }
    while (!local.empty()) {
      const std::string& line = local.front();
      file_ << line;
      current_size_ += line.size();
      local.pop_front();
    }
    file_.flush();
    if (current_size_ > max_file_size_) {
      Rotate();
    }
  }
  // 退出前写残余队列
  {
    std::lock_guard<std::mutex> lock(write_mutex_);
    local.swap(write_queue_);
  }
  while (!local.empty()) {
    const std::string& line = local.front();
    file_ << line;
    current_size_ += line.size();
    local.pop_front();
  }
  file_.flush();
}

}  // namespace common_logger
