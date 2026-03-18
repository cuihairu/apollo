---
title: Core API
icon: core
prev: /api/README.md
---

# Core API

## apollo::core::Application

```cpp
namespace apollo::core {
class Application {
public:
    virtual ~Application() = default;

    // 启动应用
    virtual void start() = 0;

    // 停止应用
    virtual void stop() = 0;

    // 获取状态
    virtual State getState() const = 0;

    // 获取名称
    virtual std::string name() const = 0;
};

enum class State {
    STOPPED,
    STARTING,
    RUNNING,
    STOPPING
};
}
```

**线程安全**: ⚠️ stop() 可以在任何线程调用

---

## apollo::core::LogManager

```cpp
namespace apollo::core {
class LogManager {
public:
    static LogManager& instance();

    // 配置
    void configure(const Config& config);

    // 写入日志
    void write(LogLevel level, std::string logger, std::string message);

    // 获取日志器
    LoggerPtr getLogger(const std::string& name);
};

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};
}
```

**线程安全**: ✅

---

## 日志宏

```cpp
#define LOG_TRACE(logger, ...)
#define LOG_DEBUG(logger, ...)
#define LOG_INFO(logger, ...)
#define LOG_WARN(logger, ...)
#define LOG_ERROR(logger, ...)
#define LOG_FATAL(logger, ...)
```

**线程安全**: ✅

---

## apollo::core::ConfigManager

```cpp
namespace apollo::core {
class ConfigManager {
public:
    static ConfigManager& instance();

    // 加载配置
    void load(const std::string& path);
    void loadString(const std::string& content);

    // 获取配置值
    template<typename T>
    T get(const std::string& key, const T& defaultValue = T{}) const;

    // 设置配置值
    template<typename T>
    void set(const std::string& key, const T& value);

    // 保存配置
    void save(const std::string& path);
};
}
```

**线程安全**: ⚠️ 需要外部同步

---

## apollo::core::ApplicationContext

```cpp
namespace apollo::core::di {
class ApplicationContext {
public:
    static ApplicationContext& instance();

    // 注册服务
    template<typename T>
    void registerSingleton();
    template<typename T>
    void registerTransient();

    // 解析服务
    template<typename T>
    std::shared_ptr<T> resolve();

    // 检查服务
    template<typename T>
    bool isRegistered() const;
};
}
```

**线程安全**: ⚠️ 需要外部同步

---

## apollo::core::EventBus

```cpp
namespace apollo::core {
class EventBus {
public:
    static EventBus& instance();

    // 订阅事件
    template<typename E>
    void subscribe(std::function<void(const E&)> handler);

    // 取消订阅
    template<typename E>
    void unsubscribe();

    // 发布事件
    template<typename E>
    void publish(const E& event);
};
}
```

**线程安全**: ✅
