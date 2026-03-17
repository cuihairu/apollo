/**
 * @file monitoring.cpp
 * @brief Actor 监控和可观测性实现
 */

#include "apollo/actor/monitoring.h"
#include "apollo/actor/actor_utils.h"
#include <sstream>
#include <iomanip>
#include <chrono>

namespace apollo {
namespace actor {

//==============================================================================
// ActorHttpApi 实现
//==============================================================================

ActorHttpApi::ActorHttpApi(ActorManager& manager, const Config& config)
    : manager_(manager), config_(config) {
}

ActorHttpApi::~ActorHttpApi() {
    stop();
}

bool ActorHttpApi::start() {
    if (running_.exchange(true)) {
        return true;
    }

    // 启动 HTTP 服务器线程
    serverThread_ = std::thread([this]() {
        // TODO: 实现完整的 HTTP 服务器
        // 这里简化为每 10 秒打印一次统计信息
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(10));

            ActorMonitor monitor(manager_);
            auto stats = monitor.stats();
            std::cout << "[HTTP API] Stats: "
                      << "total=" << stats.totalActors
                      << ", running=" << stats.runningActors
                      << ", messages=" << stats.processedMessages
                      << std::endl;
        }
    });

    return true;
}

void ActorHttpApi::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (serverThread_.joinable()) {
        serverThread_.join();
    }
}

std::string ActorHttpApi::handleRequest(const std::string& method,
                                       const std::string& path,
                                       const std::string& body) {
    if (method == "GET") {
        return handleGet(path);
    } else if (method == "POST") {
        return handlePost(path, body);
    } else if (method == "DELETE") {
        return handleDelete(path);
    }

    return "{\"error\":\"Unsupported method\"}";
}

std::string ActorHttpApi::handleGet(const std::string& path) {
    ActorMonitor monitor(manager_);

    if (path == "/api/actors/stats") {
        return monitor.exportStatsAsJson();
    } else if (path.find("/api/actors/") == 0) {
        std::string name = path.substr(12);  // 去掉 "/api/actors/"
        return monitor.exportActorInfoAsJson(name);
    } else if (path == "/api/actors") {
        return monitor.exportAllActorsAsJson();
    } else if (path == "/api/health") {
        auto health = monitor.health();
        std::ostringstream oss;
        oss << "{\"healthy\":" << (health.healthy ? "true" : "false")
            << ",\"unhealthy\":[";
        for (size_t i = 0; i < health.unhealthyActors.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << health.unhealthyActors[i] << "\"";
        }
        oss << "],\"warnings\":[";
        for (size_t i = 0; i < health.warnings.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << health.warnings[i] << "\"";
        }
        oss << "]}";
        return oss.str();
    }

    return "{\"error\":\"Not found\"}";
}

std::string ActorHttpApi::handlePost(const std::string& path, const std::string& body) {
    // POST 请求处理（如重启 Actor）
    return "{\"error\":\"Not implemented\"}";
}

std::string ActorHttpApi::handleDelete(const std::string& path) {
    // DELETE 请求处理（如删除 Actor）
    return "{\"error\":\"Not implemented\"}";
}

//==============================================================================
// PrometheusExporter 实现
//==============================================================================

PrometheusExporter::PrometheusExporter(ActorManager& manager)
    : manager_(manager) {
}

std::string PrometheusExporter::exportMetrics() const {
    std::ostringstream oss;

    oss << "# TYPE actor_total gauge\n";
    oss << "# TYPE actor_running gauge\n";
    oss << "# TYPE actor_messages_total counter\n";
    oss << "# TYPE actor_dropped_messages_total counter\n";
    oss << "# TYPE thread_pool_active gauge\n";
    oss << "# TYPE queue_size gauge\n";

    oss << "\n";
    oss << exportActorMetrics();
    oss << "\n";
    oss << exportSystemMetrics();
    oss << "\n";
    oss << exportThreadPoolMetrics();

    return oss.str();
}

std::string PrometheusExporter::exportActorMetrics() const {
    std::ostringstream oss;
    auto stats = manager_.getStats();

    oss << "# Actor metrics\n";
    oss << "actor_total " << stats.totalActors << "\n";
    oss << "actor_running " << stats.runningActors << "\n";
    oss << "actor_suspended " << stats.suspendedActors << "\n";
    oss << "actor_stopped " << stats.stoppedActors << "\n";
    oss << "actor_messages_total " << stats.totalMessages << "\n";
    oss << "actor_processed_messages_total " << stats.processedMessages << "\n";
    oss << "actor_dropped_messages_total " << stats.droppedMessages << "\n";

    return oss.str();
}

std::string PrometheusExporter::exportSystemMetrics() const {
    std::ostringstream oss;

    oss << "# System metrics\n";
    oss << "system_uptime_seconds " << (currentTimeMs() / 1000) << "\n";

    return oss.str();
}

std::string PrometheusExporter::exportThreadPoolMetrics() const {
    std::ostringstream oss;
    auto stats = manager_.getStats();

    oss << "# Thread pool metrics\n";
    oss << "thread_pool_active{name=\"dispatcher\"} " << stats.activeDispatcherThreads << "\n";
    oss << "thread_pool_active{name=\"io\"} " << stats.activeIoThreads << "\n";
    oss << "thread_pool_active{name=\"background\"} " << stats.activeBackgroundThreads << "\n";
    oss << "queue_size{name=\"dispatcher\"} " << stats.dispatcherQueueSize << "\n";
    oss << "queue_size{name=\"io\"} " << stats.ioQueueSize << "\n";
    oss << "queue_size{name=\"background\"} " << stats.backgroundQueueSize << "\n";

    return oss.str();
}

//==============================================================================
// LoggingObserver 实现
//==============================================================================

LoggingObserver::LoggingObserver(ActorManager& manager)
    : manager_(manager) {
}

void LoggingObserver::start() {
    observerId_ = manager_.addObserver("logging-observer",
        [this](const Observation& event) {
            onEvent(event);
        });
}

void LoggingObserver::stop() {
    if (observerId_ != 0) {
        manager_.removeObserver(observerId_);
        observerId_ = 0;
    }
}

void LoggingObserver::onEvent(const Observation& event) {
    std::string eventStr = eventToString(event);
    std::cout << "[ActorEvent] " << eventStr << std::endl;
}

std::string LoggingObserver::eventToString(const Observation& event) const {
    std::ostringstream oss;

    oss << "[" << event.timestamp << "] "
        << event.actorPath.toString() << " - ";

    switch (event.event) {
        case ObservationEvent::ActorCreated:
            oss << "Created";
            break;
        case ObservationEvent::ActorStarted:
            oss << "Started";
            break;
        case ObservationEvent::ActorStopped:
            oss << "Stopped";
            break;
        case ObservationEvent::ActorTerminated:
            oss << "Terminated";
            break;
        case ObservationEvent::ActorRestarted:
            oss << "Restarted";
            break;
        case ObservationEvent::MessageReceived:
            oss << "MessageReceived";
            break;
        case ObservationEvent::MessageProcessed:
            oss << "MessageProcessed";
            break;
        case ObservationEvent::MessageDropped:
            oss << "MessageDropped";
            break;
        case ObservationEvent::ActorException:
            oss << "Exception: " << event.details;
            break;
        case ObservationEvent::MailboxOverflow:
            oss << "MailboxOverflow";
            break;
        case ObservationEvent::DeadlockDetected:
            oss << "DeadlockDetected";
            break;
        default:
            oss << "Unknown";
    }

    if (!event.details.empty()) {
        oss << " - " << event.details;
    }

    return oss.str();
}

//==============================================================================
// MetricsCollector 实现
//==============================================================================

MetricsCollector::MetricsCollector(ActorManager& manager)
    : manager_(manager) {
}

void MetricsCollector::start() {
    observerId_ = manager_.addObserver("metrics-collector",
        [this](const Observation& event) {
            if (event.event == ObservationEvent::MessageProcessed) {
                // 从 details 解析处理时间
                // 简化处理
            } else if (event.event == ObservationEvent::ActorException) {
                recordError(event.actorPath.name);
            }
        });
}

void MetricsCollector::stop() {
    if (observerId_ != 0) {
        manager_.removeObserver(observerId_);
        observerId_ = 0;
    }
}

MetricsCollector::ActorMetrics MetricsCollector::getActorMetrics(const std::string& name) const {
    std::shared_lock lock(metricsMutex_);
    auto it = metrics_.find(name);
    return it != metrics_.end() ? it->second : ActorMetrics{};
}

std::vector<MetricsCollector::ActorMetrics> MetricsCollector::getAllMetrics() const {
    std::shared_lock lock(metricsMutex_);
    std::vector<ActorMetrics> result;
    result.reserve(metrics_.size());

    for (const auto& [_, metrics] : metrics_) {
        result.push_back(metrics);
    }

    return result;
}

void MetricsCollector::reset() {
    std::unique_lock lock(metricsMutex_);
    metrics_.clear();
}

std::string MetricsCollector::exportAsJson() const {
    std::ostringstream oss;
    auto allMetrics = getAllMetrics();

    oss << "[";
    for (size_t i = 0; i < allMetrics.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& m = allMetrics[i];

        oss << "{"
            << "\"actor\":\"" << m.actorName << "\","
            << "\"messageCount\":" << m.messageCount << ","
            << "\"totalProcessingTimeUs\":" << m.totalProcessingTimeUs << ","
            << "\"avgProcessingTimeUs\":" << m.avgProcessingTimeUs() << ","
            << "\"maxProcessingTimeUs\":" << m.maxProcessingTimeUs << ","
            << "\"minProcessingTimeUs\":"
                << (m.minProcessingTimeUs == UINT64_MAX ? 0 : m.minProcessingTimeUs) << ","
            << "\"peakMailboxSize\":" << m.peakMailboxSize << ","
            << "\"errorCount\":" << m.errorCount << ","
            << "\"restartCount\":" << m.restartCount
            << "}";
    }
    oss << "]";

    return oss.str();
}

std::string MetricsCollector::exportAsPrometheus() const {
    std::ostringstream oss;
    auto allMetrics = getAllMetrics();

    for (const auto& m : allMetrics) {
        std::string escapedName = m.actorName;
        // 替换不合法字符
        std::replace(escapedName.begin(), escapedName.end(), '-', '_');
        std::replace(escapedName.begin(), escapedName.end(), '/', '_');

        oss << "actor_messages_total{actor=\"" << m.actorName << "\"} "
            << m.messageCount << "\n";
        oss << "actor_processing_time_us_total{actor=\"" << m.actorName << "\"} "
            << m.totalProcessingTimeUs << "\n";
        oss << "actor_errors_total{actor=\"" << m.actorName << "\"} "
            << m.errorCount << "\n";
        oss << "actor_restarts_total{actor=\"" << m.actorName << "\"} "
            << m.restartCount << "\n";
    }

    return oss.str();
}

void MetricsCollector::recordMessageProcessed(const std::string& actorName,
                                              uint64_t processingTimeUs) {
    std::unique_lock lock(metricsMutex_);

    auto& metrics = metrics_[actorName];
    metrics.actorName = actorName;
    metrics.messageCount++;
    metrics.totalProcessingTimeUs += processingTimeUs;

    if (processingTimeUs > metrics.maxProcessingTimeUs) {
        metrics.maxProcessingTimeUs = processingTimeUs;
    }
    if (processingTimeUs < metrics.minProcessingTimeUs) {
        metrics.minProcessingTimeUs = processingTimeUs;
    }
}

void MetricsCollector::recordError(const std::string& actorName) {
    std::unique_lock lock(metricsMutex_);

    auto& metrics = metrics_[actorName];
    metrics.actorName = actorName;
    metrics.errorCount++;
}

void MetricsCollector::recordRestart(const std::string& actorName) {
    std::unique_lock lock(metricsMutex_);

    auto& metrics = metrics_[actorName];
    metrics.actorName = actorName;
    metrics.restartCount++;
}

//==============================================================================
// HealthCheck 实现
//==============================================================================

HealthCheck::HealthCheck(ActorManager& manager)
    : manager_(manager) {
}

HealthCheck::CheckResult HealthCheck::check() const {
    CheckResult result;
    result.timestamp = currentTimeMs();

    // 基础健康检查
    auto managerHealth = manager.checkHealth();
    result.healthy = managerHealth.healthy;

    if (!managerHealth.healthy) {
        result.status = "unhealthy";
        result.failures.insert(result.failures.end(),
                              managerHealth.unhealthyActors.begin(),
                              managerHealth.unhealthyActors.end());
    }

    result.checks.insert(result.checks.end(),
                        managerHealth.warnings.begin(),
                        managerHealth.warnings.end());

    // 自定义检查
    for (const auto& customCheck : customChecks_) {
        auto [success, message] = customCheck.function();
        result.checks.push_back(customCheck.name + ": " + message);

        if (!success) {
            result.healthy = false;
            result.status = "unhealthy";
            result.failures.push_back(customCheck.name + ": " + message);
        }
    }

    if (!result.healthy && result.status != "unhealthy") {
        result.status = "degraded";
    }

    return result;
}

void HealthCheck::registerCheck(const std::string& name, CheckFunction check) {
    customCheck customCheck;
    customCheck.name = name;
    customCheck.function = std::move(check);
    customChecks_.push_back(std::move(customCheck));
}

void HealthCheck::unregisterCheck(const std::string& name) {
    customChecks_.erase(
        std::remove_if(customChecks_.begin(), customChecks_.end(),
            [&name](const auto& check) { return check.name == name; }),
        customChecks_.end());
}

std::string HealthCheck::handleRequest() const {
    return check().toJson();
}

std::string HealthCheck::CheckResult::toJson() const {
    std::ostringstream oss;

    oss << "{"
        << "\"healthy\":" << (healthy ? "true" : "false") << ","
        << "\"status\":\"" << status << "\","
        << "\"timestamp\":" << timestamp << ","
        << "\"checks\":[";

    for (size_t i = 0; i < checks.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << checks[i] << "\"";
    }

    oss << "],\"failures\":[";

    for (size_t i = 0; i < failures.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << failures[i] << "\"";
    }

    oss << "]}";

    return oss.str();
}

//==============================================================================
// DebugEndpoint 实现
//==============================================================================

DebugEndpoint::DebugEndpoint(ActorManager& manager)
    : manager_(manager) {
}

std::string DebugEndpoint::getMailboxContent(const std::string& actorName) const {
    auto cell = manager.getActor(actorName);
    if (!cell) {
        return "{\"error\":\"Actor not found\"}";
    }

    std::ostringstream oss;
    oss << "{"
        << "\"actor\":\"" << actorName << "\","
        << "\"mailboxSize\":" << cell->mailbox().size()
        << "}";

    return oss.str();
}

std::string DebugEndpoint::getActorStack(const std::string& actorName) const {
    // TODO: 实现堆栈跟踪
    return "{\"error\":\"Not implemented\"}";
}

std::string DebugEndpoint::getThreadStates() const {
    std::ostringstream oss;
    oss << "{"
        << "\"status\":\"running\""
        << "}";

    return oss.str();
}

std::string DebugEndpoint::getMemoryUsage() const {
    // TODO: 实现内存统计
    return "{\"error\":\"Not implemented\"}";
}

std::string DebugEndpoint::triggerGC() const {
    return "{\"error\":\"Not implemented\"}";
}

std::string DebugEndpoint::detectDeadlocks() const {
    auto deadlocked = manager.detectDeadlock();

    std::ostringstream oss;
    oss << "{"
        << "\"deadlocked\":[";

    for (size_t i = 0; i < deadlocked.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << deadlocked[i] << "\"";
    }

    oss << "]}";

    return oss.str();
}

//==============================================================================
// AdminCommand 实现
//==============================================================================

AdminCommand::AdminCommand(ActorManager& manager)
    : manager_(manager) {
    registerBuiltinCommands();
}

AdminCommand::CommandResult AdminCommand::execute(const std::string& command) {
    // 简单解析
    std::vector<std::string> args;
    std::istringstream iss(command);
    std::string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }

    if (args.empty()) {
        return {false, "", "Empty command"};
    }

    std::string cmdName = args[0];
    auto it = commands_.find(cmdName);

    if (it == commands_.end()) {
        return {false, "", "Unknown command: " + cmdName};
    }

    try {
        return it->second.handler(args);
    } catch (const std::exception& e) {
        return {false, "", e.what()};
    }
}

void AdminCommand::registerCommand(const std::string& name,
                                  const std::string& description,
                                  CommandHandler handler) {
    CommandInfo info;
    info.name = name;
    info.description = description;
    info.handler = std::move(handler);
    commands_[name] = std::move(info);
}

std::vector<std::string> AdminCommand::listCommands() const {
    std::vector<std::string> result;
    for (const auto& [name, info] : commands_) {
        result.push_back(name + " - " + info.description);
    }
    return result;
}

std::string AdminCommand::getHelp(const std::string& command) const {
    std::ostringstream oss;

    if (command.empty()) {
        oss << "Available commands:\n";
        for (const auto& [name, info] : commands_) {
            oss << "  " << name << " - " << info.description << "\n";
        }
    } else {
        auto it = commands_.find(command);
        if (it != commands_.end()) {
            oss << it->first << " - " << it->second.description;
        } else {
            oss << "Unknown command: " << command;
        }
    }

    return oss.str();
}

void AdminCommand::registerBuiltinCommands() {
    // list - 列出所有 Actor
    registerCommand("list", "List all actors",
        [this](const std::vector<std::string>&) -> CommandResult {
            auto names = manager.getAllActorNames();
            std::ostringstream oss;
            for (const auto& name : names) {
                oss << name << "\n";
            }
            return {true, oss.str(), ""};
        });

    // stop - 停止 Actor
    registerCommand("stop", "Stop an actor: stop <actor_name>",
        [this](const std::vector<std::string>& args) -> CommandResult {
            if (args.size() < 2) {
                return {false, "", "Usage: stop <actor_name>"};
            }
            bool success = manager.stopActor(args[1]);
            return {success, success ? "Actor stopped" : "Failed to stop actor", ""};
        });

    // stats - 显示统计信息
    registerCommand("stats", "Show statistics",
        [this](const std::vector<std::string>&) -> CommandResult {
            ActorMonitor monitor(manager_);
            return {true, monitor.exportStatsAsJson(), ""};
        });

    // health - 健康检查
    registerCommand("health", "Check system health",
        [this](const std::vector<std::string>&) -> CommandResult {
            ActorMonitor monitor(manager_);
            auto health = monitor.health();
            std::ostringstream oss;
            oss << "healthy: " << (health.healthy ? "yes" : "no") << "\n";
            for (const auto& warning : health.warnings) {
                oss << "warning: " << warning << "\n";
            }
            return {true, oss.str(), ""};
        });

    // help - 帮助
    registerCommand("help", "Show help: help [command]",
        [this](const std::vector<std::string>& args) -> CommandResult {
            std::string cmd = args.size() > 1 ? args[1] : "";
            return {true, getHelp(cmd), ""};
        });
}

//==============================================================================
// Observability 实现
//==============================================================================

Observability::Observability(ActorManager& manager, const ObservabilityConfig& config)
    : manager_(manager)
    , config_(config)
    , prometheus_(std::make_unique<PrometheusExporter>(manager))
    , loggingObserver_(std::make_unique<LoggingObserver>(manager))
    , metrics_(std::make_unique<MetricsCollector>(manager))
    , healthCheck_(std::make_unique<HealthCheck>(manager))
    , debugEndpoint_(std::make_unique<DebugEndpoint>(manager))
    , adminCommand_(std::make_unique<AdminCommand>(manager)) {
}

Observability::~Observability() {
    stop();
}

bool Observability::start() {
    if (running_.exchange(true)) {
        return true;
    }

    // 启动 HTTP API
    if (config_.enableHttpApi) {
        httpApi_ = std::make_unique<ActorHttpApi>(manager_, config_.httpApi);
        httpApi_->start();
    }

    // 启动日志观察者
    if (config_.enableLoggingObserver) {
        loggingObserver_->start();
    }

    // 启动指标收集
    if (config_.enableMetricsCollector) {
        metrics_->start();
    }

    // 启动指标导出线程
    if (config_.enablePrometheus) {
        metricsExportThread_ = std::thread([this]() {
            while (running_.load()) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(config_.metricsExportIntervalMs));

                // 导出指标到文件或网络
                std::cout << "[Prometheus]\n" << prometheus_->exportMetrics() << std::endl;
            }
        });
    }

    return true;
}

void Observability::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    // 停止 HTTP API
    if (httpApi_) {
        httpApi_->stop();
    }

    // 停止观察者
    loggingObserver_->stop();
    metrics_->stop();

    // 停止导出线程
    if (metricsExportThread_.joinable()) {
        metricsExportThread_.join();
    }
}

} // namespace actor
} // namespace apollo
