#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <iostream>

namespace apollo {
namespace server {

/// 服务器配置
struct ServerConfig {
    std::string name;              // 服务器名称
    std::string host;              // 绑定地址
    uint16_t port;                 // 绑定端口
    int threadCount;               // 工作线程数
    int maxConnections;            // 最大连接数

    // 配置文件路径
    std::string configFile;

    // 日志配置
    std::string logPath;
    uint32_t logMaxSize;           // 日志文件最大大小（字节）
    uint32_t logMaxFiles;          // 日志文件最大数量

    ServerConfig()
        : port(0), threadCount(0), maxConnections(1000)
        , logMaxSize(100 * 1024 * 1024)
        , logMaxFiles(10) {}
};

/// 服务器状态
enum class ServerState : uint8_t {
    Stopped = 0,
    Starting = 1,
    Running = 2,
    Stopping = 3,
    Error = 4
};

/// 启动阶段
enum class StartupPhase : uint8_t {
    PreInit = 0,        // 预初始化（加载配置等）
    InitComponents = 1, // 初始化组件
    PostInit = 2,       // 后初始化（组件依赖注入）
    StartNetworking = 3, // 启动网络
    Ready = 4           // 就绪
};

/// 组件接口
class IServerComponent {
public:
    virtual ~IServerComponent() = default;

    /// 获取组件名称
    virtual std::string getName() const = 0;

    /// 初始化
    virtual bool initialize(const ServerConfig& config) = 0;

    /// 启动
    virtual bool start() = 0;

    /// 停止
    virtual void stop() = 0;

    /// 获取组件状态
    virtual bool isRunning() const = 0;
};

/// 生命周期监听器
class IServerLifecycleListener {
public:
    virtual ~IServerLifecycleListener() = default;

    /// 配置加载后
    virtual void onConfigLoaded(const ServerConfig& config) {}

    /// 初始化开始
    virtual void onInitializeStart() {}

    /// 初始化完成
    virtual void onInitializeComplete(bool success) {}

    /// 启动开始
    virtual void onStartupStart() {}

    /// 启动完成
    virtual void onStartupComplete(bool success) {}

    /// 关闭开始
    virtual void onShutdownStart() {}

    /// 关闭完成
    virtual void onShutdownComplete() {}

    /// 阶段变化
    virtual void onPhaseChanged(StartupPhase from, StartupPhase to) {}
};

/// 默认生命周期监听器
class DefaultLifecycleListener : public IServerLifecycleListener {
public:
    void onConfigLoaded(const ServerConfig& config) override {
        std::cout << "[Server] Config loaded: " << config.name << std::endl;
    }

    void onInitializeStart() override {
        std::cout << "[Server] Initializing..." << std::endl;
    }

    void onInitializeComplete(bool success) override {
        std::cout << "[Server] Initialization "
                  << (success ? "succeeded" : "failed") << std::endl;
    }

    void onStartupStart() override {
        std::cout << "[Server] Starting..." << std::endl;
    }

    void onStartupComplete(bool success) override {
        std::cout << "[Server] Startup "
                  << (success ? "succeeded" : "failed") << std::endl;
    }

    void onShutdownStart() override {
        std::cout << "[Server] Shutting down..." << std::endl;
    }

    void onShutdownComplete() override {
        std::cout << "[Server] Shutdown complete" << std::endl;
    }

    void onPhaseChanged(StartupPhase from, StartupPhase to) override {
        const char* phaseNames[] = {
            "PreInit", "InitComponents", "PostInit", "StartNetworking", "Ready"
        };
        std::cout << "[Server] Phase: " << phaseNames[static_cast<int>(from)]
                  << " -> " << phaseNames[static_cast<int>(to)] << std::endl;
    }
};

/// 服务器统计信息
struct ServerStats {
    uint64_t startTime;           // 启动时间（毫秒时间戳）
    uint64_t uptime;              // 运行时间（毫秒）
    uint32_t currentConnections;  // 当前连接数
    uint32_t totalConnections;    // 总连接数
    uint64_t totalBytesReceived;  // 总接收字节
    uint64_t totalBytesSent;      // 总发送字节
    uint32_t requestsPerSecond;   // 每秒请求数
    double cpuUsage;              // CPU 使用率
    double memoryUsage;           // 内存使用量（MB）

    ServerStats()
        : startTime(0), uptime(0)
        , currentConnections(0), totalConnections(0)
        , totalBytesReceived(0), totalBytesSent(0)
        , requestsPerSecond(0), cpuUsage(0.0), memoryUsage(0.0) {}
};

/// 游戏服务器基类
class GameServer {
public:
    GameServer();
    virtual ~GameServer();

    /// 禁止拷贝和移动
    GameServer(const GameServer&) = delete;
    GameServer& operator=(const GameServer&) = delete;

    /// 设置配置
    void setConfig(const ServerConfig& config);

    /// 添加组件
    void addComponent(std::shared_ptr<IServerComponent> component);

    /// 设置生命周期监听器
    void setLifecycleListener(IServerLifecycleListener* listener);

    /// 初始化服务器
    bool initialize();

    /// 启动服务器
    bool start();

    /// 停止服务器
    void stop();

    /// 阻塞运行（直到服务器停止）
    void run();

    /// 获取服务器状态
    ServerState getState() const { return state_; }

    /// 获取统计信息
    ServerStats getStats() const;

    /// 获取配置
    const ServerConfig& getConfig() const { return config_; }

    /// 是否运行中
    bool isRunning() const { return state_ == ServerState::Running; }

protected:
    /// 子类重写：自定义初始化逻辑
    virtual bool onInitialize() { return true; }

    /// 子类重写：自定义启动逻辑
    virtual bool onStart() { return true; }

    /// 子类重写：自定义停止逻辑
    virtual void onStop() {}

    /// 子类重写：主循环（每帧调用）
    virtual void onUpdate() {}

    /// 通知阶段变化
    void notifyPhaseChanged(StartupPhase phase);

private:
    bool initializeComponents();
    bool startComponents();
    void stopComponents();
    void updateStats();
    void mainLoop();

    ServerConfig config_;
    ServerState state_;
    StartupPhase currentPhase_;
    ServerStats stats_;

    std::vector<std::shared_ptr<IServerComponent>> components_;
    IServerLifecycleListener* lifecycleListener_;

    std::atomic<bool> running_;
    std::thread mainThread_;
    std::thread statsThread_;

    mutable std::mutex mutex_;
};

/// 服务器构建器
class ServerBuilder {
public:
    ServerBuilder();

    /// 设置服务器名称
    ServerBuilder& name(const std::string& name);

    /// 设置绑定地址
    ServerBuilder& bind(const std::string& host, uint16_t port);

    /// 设置线程数
    ServerBuilder& threads(int count);

    /// 设置最大连接数
    ServerBuilder& maxConnections(int count);

    /// 设置配置文件
    ServerBuilder& configFile(const std::string& path);

    /// 添加组件
    ServerBuilder& component(std::shared_ptr<IServerComponent> component);

    /// 设置生命周期监听器
    ServerBuilder& lifecycleListener(IServerLifecycleListener* listener);

    /// 构建服务器
    std::unique_ptr<GameServer> build();

private:
    ServerConfig config_;
    std::vector<std::shared_ptr<IServerComponent>> components_;
    IServerLifecycleListener* lifecycleListener_;
};

/// 便捷宏定义
#define SERVER_NAME(name) \
    std::string getServerName() const override { return name; }

#define CONFIG_PARAM(type, name, defaultValue) \
    type get##name() const { return config_.name; }

//==============================================================================
// 预定义组件
//==============================================================================

/// 日志组件
class LoggingComponent : public IServerComponent {
public:
    explicit LoggingComponent(const std::string& logPath);

    std::string getName() const override { return "Logging"; }
    bool initialize(const ServerConfig& config) override;
    bool start() override { return true; }
    void stop() override;
    bool isRunning() const override { return running_; }

private:
    std::string logPath_;
    std::atomic<bool> running_{false};
};

/// 定时器组件
class TimerComponent : public IServerComponent {
public:
    TimerComponent();

    std::string getName() const override { return "Timer"; }
    bool initialize(const ServerConfig& config) override;
    bool start() override;
    void stop() override;
    bool isRunning() const override { return running_; }

private:
    std::atomic<bool> running_{false};
};

/// 网络组件
class NetworkComponent : public IServerComponent {
public:
    explicit NetworkComponent(uint16_t port);

    std::string getName() const override { return "Network"; }
    bool initialize(const ServerConfig& config) override;
    bool start() override;
    void stop() override;
    bool isRunning() const override { return running_; }

    uint16_t getPort() const { return port_; }

private:
    uint16_t port_;
    std::atomic<bool> running_{false};
};

/// RPC 组件
class RpcComponent : public IServerComponent {
public:
    RpcComponent();

    std::string getName() const override { return "RPC"; }
    bool initialize(const ServerConfig& config) override;
    bool start() override;
    void stop() override;
    bool isRunning() const override { return running_; }

private:
    std::atomic<bool> running_{false};
};

/// 数据库组件
class DatabaseComponent : public IServerComponent {
public:
    DatabaseComponent();

    std::string getName() const override { return "Database"; }
    bool initialize(const ServerConfig& config) override;
    bool start() override;
    void stop() override;
    bool isRunning() const override { return running_; }

private:
    std::atomic<bool> running_{false};
};

} // namespace server
} // namespace apollo
