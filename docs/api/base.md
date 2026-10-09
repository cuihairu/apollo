---
title: Base API
icon: cube
prev: /api/README.md
---

# Base API

> 2026-10-10 对账：本页按 HEAD 实况重写（modules/base 八头：time/thread_pool/
> id_pool/memory/string/rng/terminal/timer_wheel）。旧稿的 `Time::nowMillis()`、
> `IdPool::acquire()`、`<apollo/base/object_pool.hpp>` 独立头等在仓库中不存在。

## apollo::base::Time（time.hpp）

```cpp
namespace apollo::base {
class Time {
public:
    using Timestamp = int64_t;
    static Timestamp now();                 // steady_clock 毫秒
    static int64_t now_microseconds();
    static int64_t now_nanoseconds();
    static time_t unix_time();
    static std::string format(time_t timestamp,
                              const char* pattern = "%Y-%m-%d %H:%M:%S");
};
}
```

同文件还有：

- `Timer`——`reset()` / `elapsed()`（秒，`double`）。
- `ScopeTimer`——RAII 计时（构造起走，析构打点）。
- `FpsCalculator`——`update()` 逐帧喂、`fps()` 读滑动帧率（构造注入刷新间隔，默认 500ms）。

**线程安全**: 安全（全静态无状态）

---

## apollo::base::ThreadPool（thread_pool.hpp）

```cpp
namespace apollo::base {
class ThreadPool {
public:
    explicit ThreadPool(size_t threads);
    ~ThreadPool();                          // 析构即 stop

    template<class F, class... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;
        // 已 stop 再 submit 抛 std::runtime_error

    size_t size() const;                    // 线程数
    size_t tasks() const;                   // 排队任务数
    void wait_for_all();                    // 等待全部任务完成
    void stop();                            // 停止（未执行任务不再执行）
};
}
```

**线程安全**: 安全（内部锁）

---

## apollo::base::IdPool（id_pool.hpp）

```cpp
namespace apollo::base {
class IdPool {
public:
    IdPool() = default;
    IdPool(uint32_t min_id, uint32_t max_id);   // 范围 [min_id, max_id]
    explicit IdPool(uint32_t size);             // [0, size)

    uint32_t allocate();                    // 分配（耗尽返回 UINT32_MAX）
    void release(uint32_t id);
    bool reserve(uint32_t id);              // 预占指定 id
    void reset();

    bool is_valid(uint32_t id) const;
    bool is_allocated(uint32_t id) const;
    bool is_full() const;
    bool is_empty() const;
    uint32_t allocated_count() const;
    uint32_t capacity() const;
    uint32_t usage_percent() const;
};
}
```

**线程安全**: 不安全（归宿主线程；无内锁）

---

## apollo::base::ObjectPool（id_pool.hpp，槽位复用 IdPool 机制）

```cpp
namespace apollo::base {
template <typename T>
class ObjectPool {
public:
    explicit ObjectPool(uint32_t capacity);

    uint32_t allocate(T object);            // 槽位化持有，返回 id（耗尽 UINT32_MAX）
    T* get(uint32_t id);                    // 越界/空闲返回 nullptr
    std::unique_ptr<T> release(uint32_t id);// 取出并释放槽位
};
}
```

注意：与旧稿不同，非 `acquire()/release(shared_ptr)` 语义——对象由调用方构造后
`allocate(std::move(obj))` 入池，经 `get(id)` 访问、`release(id)` 取出。

**线程安全**: 不安全（无内锁）

---

## apollo::base::String（string.hpp）

```cpp
namespace apollo::base {
class String {
public:
    // 修剪（含 ltrim/rtrim 与 *_in_place 变体）
    static std::string trim(std::string_view str);
    static void trim_in_place(std::string& str);

    // 大小写
    static std::string to_lower(std::string str);
    static std::string to_upper(std::string str);

    // 比较
    static bool iequals(std::string_view a, std::string_view b);
    static bool starts_with(std::string_view str, std::string_view prefix);
    static bool ends_with(std::string_view str, std::string_view suffix);
    static bool istarts_with(std::string_view str, std::string_view prefix);
    static bool iends_with(std::string_view str, std::string_view suffix);

    // 分割（string_view 分隔符重载 + out 参数重载）
    static std::vector<std::string> split(std::string_view str, char delimiter);

    // 连接（vector<string> 与 vector<string_view> 两重载）
    static std::string join(const std::vector<std::string>& strs, std::string_view delimiter);

    // 格式化
    template <typename... Args>
    static std::string format(const char* fmt, Args... args);
    static std::string format_bytes(uint64_t bytes);   // 字节数人读化
};
}
```

**线程安全**: 安全（全静态无状态）

---

## apollo::base::FixedMemoryPool（memory.hpp）

```cpp
namespace apollo::base {
template <size_t BlockSize, size_t BlockCount = 1024>
class FixedMemoryPool {
public:
    FixedMemoryPool();                      // 容量由 BlockCount 编译期给定

    void* allocate();                       // 耗尽返回 nullptr
    void deallocate(void* ptr);

    size_t allocated_count() const;
    size_t capacity() const;                // = BlockCount
    size_t block_size() const;              // = BlockSize（≥ sizeof(void*)）
};
}
```

**线程安全**: 安全（内部互斥）

同文件还有：

- `ByteBuffer`——`ByteBuffer(capacity = 1024)`，`write(const void*, len)` /
  读指针面 / `data()` / `size()` / `capacity()` / `empty()`（字节缓冲）。
- `ArenaAllocator`——`ArenaAllocator(initial_capacity = 4096)`，区域分配 +
  `construct<T>(args...)` 定点构造（批量小对象、统一释放）。

---

## apollo::base::Pcg32（rng.hpp）

PCG-XSH-RR 64/32 最小实现（确定性子流——战斗随机流 battle-determinism 同源）：

```cpp
namespace apollo::base {
class Pcg32 {
public:
    Pcg32() = default;
    void seed(std::uint64_t initstate, std::uint64_t stream);  // 子流奇数化内含
    std::uint32_t next_uint32() noexcept;
    double next_double() noexcept;          // [0, 1)
};
}
```

**线程安全**: 不安全（单流单写者；跨流隔离走 stream 参数）

---

## apollo::base::Terminal（terminal.hpp）

静态终端工具面：`is_tty()` / `supports_color()` / `enable_color()` /
`get_size()`（`Size{rows, cols}`）/ `clear()` / `clear_line()` /
`move_cursor(row, col)` / `cursor_up/down/left/right(n)` / `save_cursor()` 等。

**线程安全**: 不安全（全局终端状态）

---

## apollo::base::TimerWheel（timer_wheel.hpp，G-4 定时器轮库件）

分层哈希时间轮（单写者无锁，驱动权归 game loop——`advance(now_ms)` 喂钟）：

```cpp
namespace apollo::base {
class TimerWheel {
public:
    using TimerId = std::uint64_t;          // (generation << 32) | 槽位号；0 = 无效
    using Callback = std::function<void(TimerId)>;

    explicit TimerWheel(std::uint32_t tick_ms = 10, std::uint32_t slots = 64,
                        std::uint32_t levels = 5, std::uint64_t start_ms = 0);

    TimerId schedule_after(std::uint64_t delay_ms, Callback cb);      // 一次性
    TimerId schedule_interval(std::uint64_t interval_ms, Callback cb); // 周期
    bool cancel(TimerId id);                // 已触发/陈旧 id = false

    void advance(std::uint64_t now_ms);     // 喂钟驱动：按拍触发到期回调
};
}
```

**线程安全**: 不安全（单写者纪律——只允许 game loop 线程调用）

---

**base 模块不含**：日志、配置、网络、框架语义——归属 core 及以上模块
（base 是纯基础设施，可被任何 C++ 项目独立使用）。
