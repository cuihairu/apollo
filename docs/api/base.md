---
title: Base API
icon: cube
prev: /api/README.md
---

# Base API

## apollo::base::Time

```cpp
namespace apollo::base {
class Time {
public:
    // 获取当前时间戳（毫秒）
    static uint64_t nowMillis();

    // 获取当前时间戳（微秒）
    static uint64_t nowMicros();

    // 获取当前时间戳（纳秒）
    static uint64_t nowNanos();

    // 格式化时间
    static std::string format(const char* fmt = "%Y-%m-%d %H:%M:%S");

    // 休眠
    static void sleep(uint64_t millis);
};
}
```

**线程安全**: ✅

---

## apollo::base::ThreadPool

```cpp
namespace apollo::base {
class ThreadPool {
public:
    explicit ThreadPool(size_t threads);
    ~ThreadPool();

    // 提交任务
    template<class F, class... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;

    // 获取线程数
    size_t size() const;

    // 等待所有任务完成
    void wait();

    // 停止线程池
    void stop();
};
}
```

**线程安全**: ✅

---

## apollo::base::IdPool

```cpp
namespace apollo::base {
class IdPool {
public:
    // 构造函数 [minId, maxId]
    IdPool(uint64_t minId, uint64_t maxId);

    // 获取ID
    uint64_t acquire();

    // 释放ID
    void release(uint64_t id);

    // 检查ID是否有效
    bool is_valid(uint64_t id) const;

    // 获取可用数量
    size_t available() const;
};
}
```

**线程安全**: ⚠️ 需要外部同步

---

## apollo::base::ObjectPool

```cpp
namespace apollo::base {
template<typename T>
class ObjectPool {
public:
    explicit ObjectPool(size_t capacity);

    // 获取对象
    std::shared_ptr<T> acquire();

    // 归还对象
    void release(std::shared_ptr<T> obj);

    // 池大小
    size_t capacity() const;

    // 已分配数量
    size_t allocated_count() const;
};
}
```

**线程安全**: ⚠️ 需要外部同步

---

## apollo::base::String

```cpp
namespace apollo::base {
class String {
public:
    // 去除空格
    static std::string trim(std::string_view str);
    static void trim_in_place(std::string& str);

    // 分割
    static std::vector<std::string> split(std::string_view str, char delim);

    // 连接
    static std::string join(const std::vector<std::string>& strs, std::string_view sep);

    // 大小写
    static std::string to_upper(std::string_view str);
    static std::string to_lower(std::string_view str);

    // 比较
    static bool iequals(std::string_view a, std::string_view b);
    static bool starts_with(std::string_view str, std::string_view prefix);
    static bool iends_with(std::string_view str, std::string_view suffix);

    // 格式化
    template<typename... Args>
    static std::string format(std::string_view fmt, Args&&... args);
};
}
```

**线程安全**: ✅

---

## apollo::base::FixedMemoryPool

```cpp
namespace apollo::base {
template<size_t BlockSize>
class FixedMemoryPool {
public:
    explicit FixedMemoryPool(size_t capacity);

    // 分配内存
    void* allocate();

    // 释放内存
    void deallocate(void* ptr);

    // 池信息
    size_t block_size() const;
    size_t capacity() const;
};
}
```

**线程安全**: ⚠️ 需要外部同步
