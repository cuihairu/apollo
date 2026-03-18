---
title: Runtime API
icon: play
prev: /api/README.md
---

# Runtime API

## apollo::runtime::ApplicationHost

```cpp
namespace apollo::runtime {
class ApplicationHost {
public:
    ApplicationHost();
    ~ApplicationHost();

    // 注册应用
    template<typename T, typename... Args>
    void registerApplication(Args&&... args);

    // 运行
    int run();

    // 停止
    void stop();

    // 是否运行中
    bool is_running() const;

    // 添加关闭钩子
    void addShutdownHook(std::function<void()> hook);
};
}
```

**线程安全**: ⚠️ stop() 可以在任何线程调用

---

## apollo::runtime::Console

```cpp
namespace apollo::runtime {
class Console {
public:
    static Console& instance();

    // 启动控制台监听
    void start();

    // 停止控制台监听
    void stop();

    // 添加命令
    void addCommand(const std::string& name, std::function<void()> handler);

    // 移除命令
    void removeCommand(const std::string& name);
};
}
```

**线程安全**: ✅

---

## apollo::runtime::Signal

```cpp
namespace apollo::runtime {
class Signal {
public:
    static Signal& instance();

    // 注册信号处理器
    void on(int signal, std::function<void()> handler);

    // 忽略信号
    void ignore(int signal);

    // 默认处理
    void default_(int signal);
};
}
```

**线程安全**: ⚠️ 信号处理器在信号处理线程执行

---

## apollo::runtime::HealthCheck

```cpp
namespace apollo::runtime {
class HealthCheck {
public:
    static HealthCheck& instance();

    // 添加检查项
    void add(const std::string& name, std::function<HealthStatus()> checker);

    // 移除检查项
    void remove(const std::string& name);

    // 检查所有
    HealthStatus check();

    // 获取状态
    HealthStatus getStatus(const std::string& name) const;
};

enum class HealthStatus {
    HEALTHY,
    DEGRADED,
    UNHEALTHY
};
}
```

**线程安全**: ✅
