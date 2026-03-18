#pragma once

#include "apollo/actor/actor_ref.h"
#include "apollo/actor/actor_manager.h"
#include <string>
#include <functional>
#include <memory>

namespace apollo {
namespace actor {

//==============================================================================
// HTTP 监控 API
//==============================================================================

class ActorHttpApi {
public:
    struct Config {
        std::string host = "0.0.0.0";
        uint16_t port = 8080;
        std::string basePath = "/api/actors";
        bool enableCors = true;
        bool enableAuth = false;
        std::string authToken = "";
    };

    explicit ActorHttpApi(ActorManager& manager, const Config& config = Config{});
    ~ActorHttpApi();

    // 启动 HTTP 服务器
    bool start();

    // 停止 HTTP 服务器
    void stop();

    // 是否运行中
    bool isRunning() const { return running_.load(); }

private:
    // HTTP 处理函数
    std::string handleRequest(const std::string& method,
                             const std::string& path,
                             const std::string& body);

    // 路由处理
    std::string handleGet(const std::string& path);
    std::string handlePost(const std::string& path, const std::string& body);
    std::string handleDelete(const std::string& path);

    ActorManager& manager_;
    Config config_;
    std::atomic<bool> running_{false};
    std::thread serverThread_;
};

//==============================================================================
// Prometheus 监控导出
//==============================================================================

class PrometheusExporter {
public:
    explicit PrometheusExporter(ActorManager& manager);

    // 导出 Prometheus 格式指标
    std::string exportMetrics() const;

    // 导出特定指标
    std::string exportActorMetrics() const;
    std::string exportSystemMetrics() const;
    std::string exportThreadPoolMetrics() const;

private:
    ActorManager& manager_;
};

//==============================================================================
// 日志观察者（将事件写入日志）
//==============================================================================

class LoggingObserver {
public:
    explicit LoggingObserver(ActorManager& manager);

    // 启动观察
    void start();

    // 停止观察
    void stop();

private:
    ActorManager& manager_;
    uint64_t observerId_ = 0;

    // 观察回调
    void onEvent(const Observation& event);

    // 事件转字符串
    std::string eventToString(const Observation& event) const;
};

//==============================================================================
// 指标收集器（用于性能分析）
//==============================================================================

class MetricsCollector {
public:
    struct ActorMetrics {
        std::string actorName;
        uint64_t messageCount = 0;
        uint64_t totalProcessingTimeUs = 0;
        uint64_t maxProcessingTimeUs = 0;
        uint64_t minProcessingTimeUs = UINT64_MAX;
        size_t peakMailboxSize = 0;
        uint64_t errorCount = 0;
        uint64_t restartCount = 0;

        // 计算平均处理时间
        uint64_t avgProcessingTimeUs() const {
            return messageCount > 0 ? totalProcessingTimeUs / messageCount : 0;
        }
    };

    explicit MetricsCollector(ActorManager& manager);

    // 启动收集
    void start();

    // 停止收集
    void stop();

    // 获取 Actor 指标
    ActorMetrics getActorMetrics(const std::string& name) const;

    // 获取所有 Actor 指标
    std::vector<ActorMetrics> getAllMetrics() const;

    // 重置指标
    void reset();

    // 导出为 JSON
    std::string exportAsJson() const;

    // 导出为 Prometheus 格式
    std::string exportAsPrometheus() const;

private:
    ActorManager& manager_;
    uint64_t observerId_ = 0;

    mutable std::shared_mutex metricsMutex_;
    std::unordered_map<std::string, ActorMetrics> metrics_;

    // 记录消息处理
    void recordMessageProcessed(const std::string& actorName,
                               uint64_t processingTimeUs);

    // 记录错误
    void recordError(const std::string& actorName);

    // 记录重启
    void recordRestart(const std::string& actorName);
};

//==============================================================================
// 健康检查端点
//==============================================================================

class HealthCheck {
public:
    struct CheckResult {
        bool healthy = true;
        std::string status = "healthy";  // healthy, degraded, unhealthy
        std::vector<std::string> checks;
        std::vector<std::string> failures;
        int64_t timestamp = 0;

        std::string toJson() const;
    };

    explicit HealthCheck(ActorManager& manager);

    // 执行健康检查
    CheckResult check() const;

    // 注册自定义检查
    using CheckFunction = std::function<std::pair<bool, std::string>()>;
    void registerCheck(const std::string& name, CheckFunction check);

    // 取消注册检查
    void unregisterCheck(const std::string& name);

    // HTTP 响应
    std::string handleRequest() const;

private:
    ActorManager& manager_;

    struct CustomCheck {
        std::string name;
        CheckFunction function;
    };
    std::vector<CustomCheck> customChecks_;
};

//==============================================================================
// 调试端点（用于开发和诊断）
//==============================================================================

class DebugEndpoint {
public:
    explicit DebugEndpoint(ActorManager& manager);

    // 获取 Actor 邮箱内容
    std::string getMailboxContent(const std::string& actorName) const;

    // 获取 Actor 堆栈（如果支持）
    std::string getActorStack(const std::string& actorName) const;

    // 获取线程状态
    std::string getThreadStates() const;

    // 获取内存使用
    std::string getMemoryUsage() const;

    // 触发 GC（如果支持）
    std::string triggerGC() const;

    // 获取死锁检测
    std::string detectDeadlocks() const;

private:
    ActorManager& manager_;
};

//==============================================================================
// 管理命令
//==============================================================================

class AdminCommand {
public:
    explicit AdminCommand(ActorManager& manager);

    // 执行命令
    struct CommandResult {
        bool success = false;
        std::string output;
        std::string error;
    };

    CommandResult execute(const std::string& command);

    // 命令处理器
    using CommandHandler = std::function<CommandResult(const std::vector<std::string>&)>;

    // 注册命令
    void registerCommand(const std::string& name,
                        const std::string& description,
                        CommandHandler handler);

    // 获取所有命令
    std::vector<std::string> listCommands() const;

    // 帮助信息
    std::string getHelp(const std::string& command = "") const;

private:
    ActorManager& manager_;

    struct CommandInfo {
        std::string name;
        std::string description;
        CommandHandler handler;
    };

    std::unordered_map<std::string, CommandInfo> commands_;

    // 内置命令
    void registerBuiltinCommands();
};

//==============================================================================
// 可观测性配置（统一入口）
//==============================================================================

struct ObservabilityConfig {
    // HTTP API
    ActorHttpApi::Config httpApi;
    bool enableHttpApi = true;

    // Prometheus 导出
    bool enablePrometheus = false;
    uint16_t prometheusPort = 9090;

    // 日志观察者
    bool enableLoggingObserver = true;

    // 指标收集
    bool enableMetricsCollector = true;
    uint64_t metricsExportIntervalMs = 60000;  // 1 分钟

    // 健康检查
    bool enableHealthCheck = true;
};

// 启动可观测性服务
class Observability {
public:
    explicit Observability(ActorManager& manager,
                          const ObservabilityConfig& config = ObservabilityConfig{});

    ~Observability();

    bool start();
    void stop();

    // 获取各个组件
    ActorHttpApi* httpApi() { return httpApi_.get(); }
    PrometheusExporter* prometheus() { return prometheus_.get(); }
    MetricsCollector* metrics() { return metrics_.get(); }
    HealthCheck* healthCheck() { return healthCheck_.get(); }

private:
    ActorManager& manager_;
    ObservabilityConfig config_;

    std::unique_ptr<ActorHttpApi> httpApi_;
    std::unique_ptr<PrometheusExporter> prometheus_;
    std::unique_ptr<LoggingObserver> loggingObserver_;
    std::unique_ptr<MetricsCollector> metrics_;
    std::unique_ptr<HealthCheck> healthCheck_;
    std::unique_ptr<DebugEndpoint> debugEndpoint_;
    std::unique_ptr<AdminCommand> adminCommand_;

    std::atomic<bool> running_{false};
    std::thread metricsExportThread_;
};

} // namespace actor
} // namespace apollo
