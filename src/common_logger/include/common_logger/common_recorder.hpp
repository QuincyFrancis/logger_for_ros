#ifndef COMMON_RECORDER_HPP
#define COMMON_RECORDER_HPP

#include <atomic>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace common_logger {

/// 数据记录器：把每一拍的关键数值拼成一行 CSV 写盘，事后用 Excel/Python 画曲线复盘。
///
/// 典型用法（业务线程，单写线程）：
///   auto r = Recorder::getInstance("debug", {"x", "cnt"});
///   r->data["x"] = 1.5;    // 存列值（统一 double，默认格式自动区分整数/浮点）
///   r->data["cnt"] = 3;
///   r->push();             // 记录当前时间 + 拼一行入队
///
/// 线程模型：
///   - data map 不加锁（单写线程假设）
///   - push() 只入队（加锁），磁盘 I/O 交给后台写盘线程
///
/// 输出为标准 CSV：首列自动为 time_ms（epoch 毫秒），之后按列名顺序排列。
class Recorder {
 public:
  /// 获取指定名字的记录器实例（同名只建一次；同名二次调用忽略其余参数）
  /// @param name          实例名，同时决定默认子目录名
  /// @param columns       列名（有序，决定 CSV 列顺序）
  /// @param file_path     文件目录；空则用 ./my_records/<name>/
  /// @param max_file_size 单文件大小上限（字节）；软上限，整批写完才检查，超出一批后滚动
  /// @param max_files     最多保留文件数，超出删最旧；0 表示不限制
  /// @return shared_ptr，可多线程安全共享
  static std::shared_ptr<Recorder> getInstance(
      const std::string& name,
      const std::vector<std::string>& columns,
      const std::string& file_path = "",
      size_t max_file_size = 100 * 1024 * 1024,
      size_t max_files = 2);

  ~Recorder();

  /// 列值存储：data["A"] = 3.14; data["B"] = 42;
  /// 未写入的列在 push() 时输出空字段；本 map 不加锁，仅单写线程使用。
  std::map<std::string, double> data;

  /// 记录当前时间（time_ms），按列名顺序把 data 各列拼成一行 CSV 入队。
  /// push() 后 data 不清空，未更新的列沿用上一拍的值。
  void push();

 private:
  Recorder(const std::string& name,
           const std::vector<std::string>& columns,
           const std::string& file_path,
           size_t max_file_size,
           size_t max_files);

  /// 开新时间戳文件（撞名加 _N 后缀）+ 写表头 + 按创建顺序删超量旧文件
  void OpenNewFile();
  /// 关闭当前文件并开新文件
  void Rotate();
  /// 一行文本入队（加锁 push + 锁外 notify）
  void EnqueueLine(std::string line);
  /// 后台写盘线程：swap 换出队列，锁外写盘，软上限滚动
  void WriterLoop();

  std::vector<std::string> columns_;                          ///< 列名（有序）
  std::string dir_;                                           ///< 记录目录
  size_t max_file_size_;                                      ///< 单文件字节上限
  size_t max_files_;                                          ///< 最多保留文件数
  std::ofstream file_;                                        ///< 当前文件流
  size_t current_size_ = 0;                                   ///< 当前文件已写字节数
  std::deque<std::string> files_;                             ///< 存活文件（创建顺序，旧→新）

  std::mutex write_mutex_;                                    ///< 保护 write_queue_
  std::condition_variable write_cv_;                          ///< 队列非空/退出通知
  std::deque<std::string> write_queue_;                       ///< 待写行队列
  std::thread writer_thread_;                                 ///< 写盘线程
  std::atomic<bool> writer_running_{false};                   ///< 写线程运行标志

  static std::shared_mutex mutex_;                            ///< 保护 instances_
  static std::unordered_map<std::string, std::shared_ptr<Recorder>> instances_;  ///< 名字 -> 实例
};

}  // namespace common_logger

#endif  // COMMON_RECORDER_HPP
