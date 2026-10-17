---
title: Base 模块
icon: cube
order: 1
category:
  - 模块
tag:
  - base
  - 基础
---

# Base 模块

Base 模块提供纯基础设施，无任何框架语义，可以被任何 C++ 项目独立使用。
API 明细见 [Base API](/api/base)。

## 组件

八个头（`include/apollo/base/`）：

### Time

高精度时间工具（time.hpp）。

```cpp
#include <apollo/base/time.hpp>

using namespace apollo::base;

int64_t now = Time::now();              // steady_clock 毫秒
int64_t us  = Time::now_microseconds();

Timer timer;                            // 计时器
// ... 执行操作 ...
double elapsed = timer.elapsed();       // 秒

FpsCalculator fps;                      // 帧率统计
fps.update();
float rate = fps.fps();
```

### ThreadPool

线程池（thread_pool.hpp）。

```cpp
ThreadPool pool(4);

auto future = pool.submit([]() { return 42; });
int result = future.get();

pool.wait_for_all();                    // 等全部任务完成
pool.stop();                            // 停止（之后 submit 抛 std::runtime_error）
```

### IdPool / ObjectPool

唯一 ID 生成器与槽位对象池（同 id_pool.hpp）。

```cpp
#include <apollo/base/id_pool.hpp>

IdPool pool(1000, 9999);                // 范围 [1000, 9999]
uint32_t id = pool.allocate();          // 获取（耗尽 UINT32_MAX）
bool valid = pool.is_valid(id);
pool.release(id);

ObjectPool<MyObject> opool(100);        // 槽位池：allocate(T)->id / get(id) / release(id)
uint32_t oid = opool.allocate(MyObject{});
```

### String

字符串处理工具（string.hpp）。

```cpp
#include <apollo/base/string.hpp>

using namespace apollo::base;

std::string s = String::trim("  Hello World  ");   // "Hello World"
auto parts = String::split("a,b,c", ',');
bool match = String::iequals("hello", "HELLO");
std::string msg = String::format("%d items", n);   // printf 风格
std::string h = String::format_bytes(1536);        // "1.50 KB"
```

### Memory

内存池与字节缓冲（memory.hpp）。

```cpp
#include <apollo/base/memory.hpp>

FixedMemoryPool<1024> pool;             // <BlockSize, BlockCount=1024>
void* ptr = pool.allocate();            // 耗尽返回 nullptr（内部互斥）
pool.deallocate(ptr);

ByteBuffer buf(1024);                   // 字节缓冲
buf.write("Hello", 5);

ArenaAllocator arena(4096);             // 区域分配 + 定点构造
auto* obj = arena.construct<MyObject>(a, b);
```

### Rng（Pcg32）

确定性随机（rng.hpp，PCG-XSH-RR 64/32——战斗随机子流同源）。

```cpp
#include <apollo/base/rng.hpp>

Pcg32 rng;
rng.seed(12345, 7);                     // initstate + 子流号
uint32_t v = rng.next_uint32();
double d = rng.next_double();           // [0, 1)
```

### Terminal

静态终端工具（terminal.hpp）：`is_tty()` / `supports_color()` / `enable_color()`
/ `get_size()` / `clear()` / 光标控制（`move_cursor` / `cursor_up/down/left/right`）。

### TimerWheel（G-4）

分层哈希时间轮（timer_wheel.hpp）：单写者无锁、`advance(now_ms)` 喂钟驱动、
O(1) 侵入链调度撤销、页界级联、补拍语义、代数防陈旧撤销；不学 skynet
独立 timer 线程——驱动权留 game loop 固定阶段。

```cpp
#include <apollo/base/timer_wheel.hpp>

TimerWheel wheel(/*tick_ms=*/10);
auto id = wheel.schedule_after(5000, [](TimerId) { /* 一次性 */ });
auto pid = wheel.schedule_interval(100, [](TimerId) { /* 周期 */ });
wheel.cancel(id);
wheel.advance(Time::now());             // 主循环固定阶段喂钟
```

## 依赖

无。

## 链接

```cmake
find_package(apollo-base REQUIRED)
target_link_libraries(my_app apollo::base)
```
