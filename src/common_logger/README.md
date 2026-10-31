# common_logger

ROS2 通用工具包，含两部分：

- **Logger** —— 多进程安全日志（基于 spdlog，`flock` 文件锁，单文件不滚动）
- **Recorder** —— 单进程数据记录器（CSV 表格，后台批量写盘，软上限滚动）

## 依赖

`spdlog`、`fmt`

## 接入其它 ROS2 项目

### 1. `package.xml` 声明依赖

```xml
<depend>common_logger</depend>
```

### 2. `CMakeLists.txt` 链接

```cmake
find_package(common_logger REQUIRED)

add_executable(your_node src/your_node.cpp)
ament_target_dependencies(your_node rclcpp)
target_link_libraries(your_node common_logger::common_logger)
```

---

## Logger（日志）

### 代码中使用

```cpp
#include "common_logger/common_logger.hpp"

// 方式一：只传名字，目录与文件名都用默认
//   目录 ./my_logs/<name>/，文件名 <yyyy-MM-dd-hh-mm-ss>.txt
auto logger = common_logger::Logger::getInstance("log1");

// 方式二：显式指定目录与文件名（file_name 原样使用，不自动补扩展名）
// auto logger = common_logger::Logger::getInstance("log1", "./my_logs/log1", "run1.log");

// 模板接口：fmt 格式化
logger->info("pose x={} y={}", 1.5, 2.0);
logger->warn("low battery: {}%", 20);

// 普通接口：字符串作为普通文本，含 {} 也不会崩溃
logger->info(std::string("literal { } safe"));

// 级别过滤 + 显式落盘（昂贵，退出前调用）
logger->setLevel(spdlog::level::warn);
logger->flush();
```

### API 摘要

| 方法                                      | 说明                                                                         |
| ----------------------------------------- | ---------------------------------------------------------------------------- |
| `getInstance(name, log_dir, file_name)` | 取单例；`log_dir` 缺省 `./my_logs/<name>`，`file_name` 缺省时间戳+.txt |
| `debug/info/warn/error/critical(...)`   | 5 级日志，支持 `{}` 格式化与普通文本两种重载                               |
| `setLevel(level)`                       | 过滤低于该级别的日志                                                         |
| `flush()`                               | 显式 `fsync` 落盘（昂贵）                                                  |

### 日志文件位置

- 默认目录：`./my_logs/<name>/`
- 文件名：默认 `yyyy-MM-dd-hh-mm-ss.txt`（精确到秒）

### Logger 注意事项

- 跨进程写同一文件：各进程用相同 `log_dir` + 相同 `name`（同一秒）写同一文件，`flock` 保证行不撕裂。
- 同名 `getInstance` 第二次调用会忽略 `log_dir`（一个 name 只绑定一个文件）。
- 日志写入内核页缓存，进程正常退出即落盘；仅掉电场景才需手动 `flush()`（fsync）。

---

## Recorder（数据记录器）

把每一拍的关键数值拼成一行 CSV 写盘，事后用 Excel/Python 画曲线复盘。

### 代码中使用

```cpp
#include "common_logger/common_recorder.hpp"

// 定义列名（有序）；首列自动为 time_ms（epoch 毫秒）
auto r = common_logger::Recorder::getInstance("debug", {"pos_x", "cnt"});

// 每拍：写列值 + push() 拼一行入队
r->data["pos_x"] = 1.5;
r->data["cnt"]   = 3;
r->push();
```

### 输出示例（CSV）

```csv
time_ms,pos_x,cnt
1793435824698,1.5,3
1793435824698,1.6,4
```

- 统一 double + 默认格式：`1` → `"1"`、`3.14` → `"3.14"`（不补零，整数浮点自动区分）

### API 摘要

| 方法                                                                | 说明                                                                                                                                            |
| ------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| `getInstance(name, columns, file_path, max_file_size, max_files)` | 取单例；`columns` 为 `vector<string>` 列名；`file_path` 缺省 `./my_records/<name>/`；`max_file_size` 缺省 100MB；`max_files` 缺省 2 |
| `data`                                                            | `std::map<string,double>`，直接 `data["A"] = 3.14`                                                                                          |
| `push()`                                                          | 记录 `time_ms` + 拼一行 CSV 入队                                                                                                              |

### 文件位置

- 默认目录：`./my_records/<name>/`
- 文件名：`yyyy-MM-dd-hh-mm-ss.csv`（秒级）；滚动用新时间戳，同秒撞名加 `_1`/`_2` 后缀

### Recorder 注意事项

- **单写线程**：`data` map 不加锁，只在单线程里写值 + 调 `push()`。
- `push()` 后 `data` 不清空，未更新的列沿用上一拍的值。
- **跨进程不安全**：多进程同名同目录同秒撞名是竞态（可能覆盖，不报错）。不同 `name` 天然隔离。

---

## 通用注意事项

- 依赖的 spdlog 若为系统 apt 版，CMake 里需用 `spdlog::spdlog` 现代 target（本包已处理）。
- 如有问题，联系渡渡鸟/QuincyFrancis进行修改
