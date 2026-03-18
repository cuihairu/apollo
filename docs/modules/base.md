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

## 组件

### Time

高精度时间工具。

```cpp
#include <apollo/base/time.hpp>

using namespace apollo::base;

// 获取当前时间戳（毫秒）
uint64_t now = Time::nowMillis();

// 获取当前时间戳（微秒）
uint64_t nowUs = Time::nowMicros();

// 高精度计时器
Timer timer;
// ... 执行操作 ...
double elapsed = timer.elapsed();  // 秒
```

### ThreadPool

线程池，支持任务调度。

```cpp
#include <apollo/base/thread_pool.hpp>

ThreadPool pool(4);  // 4个线程

// 提交任务
auto future = pool.submit([]() {
    // 执行耗时操作
    return 42;
});

// 获取结果
int result = future.get();
```

### IdPool

唯一ID生成器。

```cpp
#include <apollo/base/id_pool.hpp>

IdPool pool(1000, 9999);  // 范围 [1000, 9999]

uint64_t id = pool.acquire();  // 获取ID
bool valid = pool.is_valid(id); // 检查ID是否有效
pool.release(id);              // 释放ID
```

### ObjectPool

对象池，内存复用。

```cpp
#include <apollo/base/object_pool.hpp>

class MyObject {
public:
    void reset() { /* 重置状态 */ }
};

ObjectPool<MyObject> pool(100);  // 池大小100

auto obj = pool.acquire();  // 获取对象
pool.release(obj);          // 归还对象
```

### String

字符串处理工具。

```cpp
#include <apollo/base/string.hpp>

using namespace apollo::base;

std::string s = "  Hello World  ";
std::string trimmed = String::trim(s);  // "Hello World"

std::vector<std::string> parts = String::split("a,b,c", ',');
// ["a", "b", "c"]

bool match = String::iequals("hello", "HELLO");  // true
```

### Memory

内存池、Buffer管理。

```cpp
#include <apollo/base/memory.hpp>

// 固定大小内存池
FixedMemoryPool<1024> pool(100);  // 100个1KB的块

void* ptr = pool.allocate();
pool.deallocate(ptr);

// Buffer
Buffer buf(1024);
buf.write("Hello", 5);
buf.read(dest, 5);
```

## 依赖

无。

## 链接

```cmake
find_package(apollo-base REQUIRED)
target_link_libraries(my_app apollo::base)
```
