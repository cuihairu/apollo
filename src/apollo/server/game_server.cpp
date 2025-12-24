/**
 * @file game_server.cpp
 * @brief 游戏服务器启动器实现
 */

#include "apollo/server/game_server.h"
#include "apollo/core/log/logger.h"
#include "apollo/core/timer/timer_manager.h"
#include "apollo/utils/time.h"
#include <algorithm>

namespace apollo {
namespace server {

//==============================================================================
// GameServer 实现
//==============================================================================

GameServer::GameServer()
    : state_(ServerState::Stopped)
    , currentPhase_(StartupPhase::PreInit)
    , lifecycleListener_(nullptr)
    , running_(false) {
}

GameServer::~GameServer() {
    stop();
}

void GameServer::setConfig(const ServerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

void GameServer::addComponent(std::shared_ptr<IServerComponent> component) {
    std::lock_guard<std::mutex> lock(mutex_);
    components_.push_back(component);
}

void GameServer::setLifecycleListener(IServerLifecycleListener* listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    lifecycleListener_ = listener;
}

bool GameServer::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != ServerState::Stopped) {
        return false;
    }

    state_ = ServerState::Starting;

    // 预初始化阶段
    if (lifecycleListener_) {
        lifecycleListener_->onConfigLoaded(config_);
        lifecycleListener_->onInitializeStart();
    }

    notifyPhaseChanged(StartupPhase::PreInit);

    // 初始化组件
    notifyPhaseChanged(StartupPhase::InitComponents);
    if (!initializeComponents()) {
        state_ = ServerState::Error;
        if (lifecycleListener_) {
            lifecycleListener_->onInitializeComplete(false);
        }
        return false;
    }

    // 自定义初始化
    notifyPhaseChanged(StartupPhase::PostInit);
    if (!onInitialize()) {
        state_ = ServerState::Error;
        if (lifecycleListener_) {
            lifecycleListener_->onInitializeComplete(false);
        }
        return false;
    }

    notifyPhaseChanged(StartupPhase::Ready);

    if (lifecycleListener_) {
        lifecycleListener_->onInitializeComplete(true);
    }

    stats_.startTime = utils::Time::now();

    return true;
}

bool GameServer::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != ServerState::Starting) {
        return false;
    }

    if (lifecycleListener_) {
        lifecycleListener_->onStartupStart();
    }

    // 启动网络
    notifyPhaseChanged(StartupPhase::StartNetworking);
    if (!startComponents()) {
        state_ = ServerState::Error;
        if (lifecycleListener_) {
            lifecycleListener_->onStartupComplete(false);
        }
        return false;
    }

    // 自定义启动
    if (!onStart()) {
        state_ = ServerState::Error;
        if (lifecycleListener_) {
            lifecycleListener_->onStartupComplete(false);
        }
        return false;
    }

    state_ = ServerState::Running;
    running_ = true;

    // 启动主循环
    mainThread_ = std::thread(&GameServer::mainLoop, this);

    // 启动统计线程
    statsThread_ = std::thread([this]() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            updateStats();
        }
    });

    notifyPhaseChanged(StartupPhase::Ready);

    if (lifecycleListener_) {
        lifecycleListener_->onStartupComplete(true);
    }

    return true;
}

void GameServer::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == ServerState::Stopped || state_ == ServerState::Stopping) {
            return;
        }

        state_ = ServerState::Stopping;
        running_ = false;
    }

    if (lifecycleListener_) {
        lifecycleListener_->onShutdownStart();
    }

    // 自定义停止
    onStop();

    // 停止组件
    stopComponents();

    // 等待主线程结束
    if (mainThread_.joinable()) {
        mainThread_.join();
    }

    if (statsThread_.joinable()) {
        statsThread_.join();
    }

    state_ = ServerState::Stopped;

    if (lifecycleListener_) {
        lifecycleListener_->onShutdownComplete();
    }
}

void GameServer::run() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
    }

    // 主循环
    while (running_) {
        onUpdate();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));  // ~60 FPS
    }
}

ServerStats GameServer::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

bool GameServer::initializeComponents() {
    for (auto& component : components_) {
        if (!component->initialize(config_)) {
            std::cerr << "[Server] Failed to initialize component: "
                      << component->getName() << std::endl;
            return false;
        }
        std::cout << "[Server] Component initialized: "
                  << component->getName() << std::endl;
    }
    return true;
}

bool GameServer::startComponents() {
    for (auto& component : components_) {
        if (!component->start()) {
            std::cerr << "[Server] Failed to start component: "
                      << component->getName() << std::endl;
            return false;
        }
        std::cout << "[Server] Component started: "
                  << component->getName() << std::endl;
    }
    return true;
}

void GameServer::stopComponents() {
    for (auto& component : components_) {
        if (component->isRunning()) {
            component->stop();
            std::cout << "[Server] Component stopped: "
                      << component->getName() << std::endl;
        }
    }
}

void GameServer::updateStats() {
    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t now = utils::Time::now();
    stats_.uptime = now - stats_.startTime;

    // 更新其他统计信息
    // 实际实现中会从各个组件收集数据
}

void GameServer::notifyPhaseChanged(StartupPhase phase) {
    currentPhase_ = phase;
    if (lifecycleListener_) {
        lifecycleListener_->onPhaseChanged(currentPhase_, phase);
    }
}

void GameServer::mainLoop() {
    while (running_) {
        onUpdate();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

//==============================================================================
// ServerBuilder 实现
//==============================================================================

ServerBuilder::ServerBuilder()
    : lifecycleListener_(nullptr) {
    config_.name = "ApolloServer";
    config_.host = "0.0.0.0";
    config_.port = 0;
    config_.threadCount = 0;
    config_.maxConnections = 1000;
}

ServerBuilder& ServerBuilder::name(const std::string& name) {
    config_.name = name;
    return *this;
}

ServerBuilder& ServerBuilder::bind(const std::string& host, uint16_t port) {
    config_.host = host;
    config_.port = port;
    return *this;
}

ServerBuilder& ServerBuilder::threads(int count) {
    config_.threadCount = count;
    return *this;
}

ServerBuilder& ServerBuilder::maxConnections(int count) {
    config_.maxConnections = count;
    return *this;
}

ServerBuilder& ServerBuilder::configFile(const std::string& path) {
    config_.configFile = path;
    return *this;
}

ServerBuilder& ServerBuilder::component(std::shared_ptr<IServerComponent> component) {
    components_.push_back(component);
    return *this;
}

ServerBuilder& ServerBuilder::lifecycleListener(IServerLifecycleListener* listener) {
    lifecycleListener_ = listener;
    return *this;
}

std::unique_ptr<GameServer> ServerBuilder::build() {
    auto server = std::make_unique<GameServer>();
    server->setConfig(config_);

    for (auto& component : components_) {
        server->addComponent(component);
    }

    if (lifecycleListener_) {
        server->setLifecycleListener(lifecycleListener_);
    }

    return server;
}

//==============================================================================
// 预定义组件实现
//==============================================================================

LoggingComponent::LoggingComponent(const std::string& logPath)
    : logPath_(logPath) {
}

bool LoggingComponent::initialize(const ServerConfig& config) {
    (void)config;
    // 实际实现会初始化日志系统
    std::cout << "[LoggingComponent] Path: " << logPath_ << std::endl;
    running_ = true;
    return true;
}

void LoggingComponent::stop() {
    running_ = false;
}

TimerComponent::TimerComponent() {
}

bool TimerComponent::initialize(const ServerConfig& config) {
    (void)config;
    std::cout << "[TimerComponent] Initialized" << std::endl;
    return true;
}

bool TimerComponent::start() {
    running_ = true;
    std::cout << "[TimerComponent] Started" << std::endl;
    return true;
}

void TimerComponent::stop() {
    running_ = false;
}

NetworkComponent::NetworkComponent(uint16_t port)
    : port_(port) {
}

bool NetworkComponent::initialize(const ServerConfig& config) {
    if (port_ == 0 && config.port > 0) {
        port_ = config.port;
    }
    std::cout << "[NetworkComponent] Port: " << port_ << std::endl;
    return true;
}

bool NetworkComponent::start() {
    running_ = true;
    std::cout << "[NetworkComponent] Started on port " << port_ << std::endl;
    return true;
}

void NetworkComponent::stop() {
    running_ = false;
}

RpcComponent::RpcComponent() {
}

bool RpcComponent::initialize(const ServerConfig& config) {
    (void)config;
    std::cout << "[RpcComponent] Initialized" << std::endl;
    return true;
}

bool RpcComponent::start() {
    running_ = true;
    std::cout << "[RpcComponent] Started" << std::endl;
    return true;
}

void RpcComponent::stop() {
    running_ = false;
}

DatabaseComponent::DatabaseComponent() {
}

bool DatabaseComponent::initialize(const ServerConfig& config) {
    (void)config;
    std::cout << "[DatabaseComponent] Initialized" << std::endl;
    return true;
}

bool DatabaseComponent::start() {
    running_ = true;
    std::cout << "[DatabaseComponent] Connected" << std::endl;
    return true;
}

void DatabaseComponent::stop() {
    running_ = false;
}

} // namespace server
} // namespace apollo
