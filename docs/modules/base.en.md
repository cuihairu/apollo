---
title: Base Module
icon: cube
order: 1
category:
  - Modules
tag:
  - base
  - infrastructure
---

[English](base.en.md) | [中文](base.md)

# Base Module

The Base module provides pure infrastructure with no framework semantics of its own; it can be used standalone in any C++ project.
For the full API list see [Base API](/api/base).

## Components

Eight headers (`include/apollo/base/`):

### Time

High-precision time utilities (time.hpp).

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

Thread pool (thread_pool.hpp).

```cpp
ThreadPool pool(4);

auto future = pool.submit([]() { return 42; });
int result = future.get();

pool.wait_for_all();                    // 等全部任务完成
pool.stop();                            // 停止（之后 submit 抛 std::runtime_error）
```

### IdPool / ObjectPool

Unique ID generator and slot-based object pool (both in id_pool.hpp).

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

String utilities (string.hpp).

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

Memory pool and byte buffer (memory.hpp).

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

Deterministic random numbers (rng.hpp, PCG-XSH-RR 64/32 — same lineage as the battle RNG substream).

```cpp
#include <apollo/base/rng.hpp>

Pcg32 rng;
rng.seed(12345, 7);                     // initstate + 子流号
uint32_t v = rng.next_uint32();
double d = rng.next_double();           // [0, 1)
```

### Terminal

Static terminal utilities (terminal.hpp): `is_tty()` / `supports_color()` / `enable_color()`
/ `get_size()` / `clear()` / cursor control (`move_cursor` / `cursor_up/down/left/right`).

### TimerWheel（G-4）

Hierarchical hash timing wheel (timer_wheel.hpp): single-writer lock-free, driven by feeding
the clock with `advance(now_ms)`, O(1) intrusive-list schedule/cancel, page-boundary cascading,
catch-up semantics, generation counters guarding against stale cancels; deliberately not a
skynet-style dedicated timer thread — driving stays with a fixed stage of the game loop.

```cpp
#include <apollo/base/timer_wheel.hpp>

TimerWheel wheel(/*tick_ms=*/10);
auto id = wheel.schedule_after(5000, [](TimerId) { /* 一次性 */ });
auto pid = wheel.schedule_interval(100, [](TimerId) { /* 周期 */ });
wheel.cancel(id);
wheel.advance(Time::now());             // 主循环固定阶段喂钟
```

## Dependencies

None.

## Linking

```cmake
find_package(apollo-base REQUIRED)
target_link_libraries(my_app apollo::base)
```
