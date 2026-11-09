#pragma once

#include "apollo/net/adapters/sdnet_sscp.h"
#include "apollo/net/adapters/sdnet_endpoint.h"
#include "apollo/net/net.h"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace apollo {
namespace net {
namespace adapters {

/**
 * @brief SDNet 网络管理器实现
 */
class SDNetNetworkManager : public NetworkManager {
public:
    SDNetNetworkManager()
        : nativeNetwork_(nullptr)
        , running_(false)
        , ioType_(0) {

        // 根据平台选择默认 I/O 类型
#ifdef _WIN32
        ioType_ = SSCP::NETIO_COMPLETIONPORT;
#else
        ioType_ = SSCP::NETIO_EPOLL;
#endif
    }

    ~SDNetNetworkManager() override {
        stop();
    }

    // NetworkManager 接口实现
    bool initialize(const NetworkConfig& config) override {
        config_ = config;

        // 获取 SDNet 模块
        // nativeNetwork_ = SSCP::SSNetGetModule(&SSCP::SDNET_VERSION);
        // if (!nativeNetwork_) {
        //     return false;
        // }

        return true;
    }

    bool start() override {
        if (running_) return true;
        running_ = true;
        return true;
    }

    void stop() override {
        if (!running_) return;

        // 停止所有监听器
        for (auto& kv : listeners_) {
            kv.second->stop();
        }
        listeners_.clear();

        // 停止所有连接器
        for (auto& kv : connectors_) {
            kv.second->stop();
        }
        connectors_.clear();

        // 释放 SDNet 模块
        // if (nativeNetwork_) {
        //     nativeNetwork_->Release();
        //     nativeNetwork_ = nullptr;
        // }

        running_ = false;
    }

    bool isRunning() const override { return running_; }

    void update() override {
        if (!running_ || !nativeNetwork_) return;

        // 处理网络事件 (非阻塞)
        // nativeNetwork_->Run(0);
    }

    ListenerPtr createListener(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (listeners_.find(name) != listeners_.end()) {
            return nullptr;
        }

        // 创建 SDNet 监听器
        // SSCP::ISSListener* nativeListener = nativeNetwork_->CreateListener(ioType_);
        // if (!nativeListener) {
        //     return nullptr;
        // }

        auto listener = std::make_shared<SDNetListener>(nullptr);
        listeners_[name] = listener;
        return listener;
    }

    ListenerPtr getListener(const std::string& name) const override {
        auto it = listeners_.find(name);
        return it != listeners_.end() ? it->second : nullptr;
    }

    bool removeListener(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = listeners_.find(name);
        if (it != listeners_.end()) {
            it->second->stop();
            listeners_.erase(it);
            return true;
        }
        return false;
    }

    void removeAllListeners() override {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& kv : listeners_) {
            kv.second->stop();
        }
        listeners_.clear();
    }

    ConnectorPtr createConnector(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (connectors_.find(name) != connectors_.end()) {
            return nullptr;
        }

        // 创建 SDNet 连接器
        // SSCP::ISSConnector* nativeConnector = nativeNetwork_->CreateConnector(ioType_);
        // if (!nativeConnector) {
        //     return nullptr;
        // }

        auto connector = std::make_shared<SDNetConnector>(nullptr);
        connectors_[name] = connector;
        return connector;
    }

    ConnectorPtr getConnector(const std::string& name) const override {
        auto it = connectors_.find(name);
        return it != connectors_.end() ? it->second : nullptr;
    }

    bool removeConnector(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = connectors_.find(name);
        if (it != connectors_.end()) {
            it->second->stop();
            connectors_.erase(it);
            return true;
        }
        return false;
    }

    void removeAllConnectors() override {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& kv : connectors_) {
            kv.second->stop();
        }
        connectors_.clear();
    }

    ConnectionPtr getConnection(uint64_t connectionId) const override {
        (void)connectionId;
        return nullptr;
    }

    bool disconnectConnection(uint64_t connectionId, const char* reason) override {
        (void)connectionId;
        (void)reason;
        return false;
    }

    uint32_t broadcast(const char* data, uint32_t length,
                       const std::vector<uint64_t>& excludeIds) override {
        (void)excludeIds;
        uint32_t count = 0;
        for (const auto& kv : listeners_) {
            (void)kv;
            // TODO: 向监听器的所有连接广播
        }
        return count;
    }

    NetworkStats getStats() const override { return stats_; }

    void resetStats() override {
        stats_ = NetworkStats{};
    }

    const NetworkConfig& getConfig() const override { return config_; }

    // SDNet 特定方法
    SSCP::ISSNet* getNativeNetwork() const { return nativeNetwork_; }
    void setNativeNetwork(SSCP::ISSNet* network) { nativeNetwork_ = network; }

    void setIOType(uint32_t ioType) { ioType_ = ioType; }

private:
    SSCP::ISSNet* nativeNetwork_;
    NetworkConfig config_;
    mutable NetworkStats stats_;
    std::unordered_map<std::string, ListenerPtr> listeners_;
    std::unordered_map<std::string, ConnectorPtr> connectors_;
    mutable std::mutex mutex_;
    bool running_;
    uint32_t ioType_;
};

} // namespace adapters
} // namespace net
} // namespace apollo
