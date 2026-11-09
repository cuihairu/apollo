#pragma once

#include "apollo/net/adapters/sdnet_sscp.h"
#include "apollo/net/net.h"
#include <memory>
#include <string>

namespace apollo {
namespace net {
namespace adapters {

/**
 * @brief SDNet 监听器实现
 */
class SDNetListener : public Listener {
public:
    SDNetListener() : SDNetListener(nullptr) {}

    explicit SDNetListener(SSCP::ISSListener* nativeListener)
        : nativeListener_(nativeListener)
        , maxConnections_(1000)
        , noDelay_(true)
        , recvBufferSize_(8192)
        , sendBufferSize_(8192)
        , running_(false) {}

    ~SDNetListener() override {
        stop();
    }

    // Listener 接口实现
    bool start(const std::string& ip, uint16_t port) override {
        if (!nativeListener_) return false;

        if (recvBufferSize_ > 0 || sendBufferSize_ > 0) {
            nativeListener_->SetBufferSize(recvBufferSize_, sendBufferSize_);
        }

        running_ = nativeListener_->Start(ip.c_str(), port);
        return running_;
    }

    bool stop() override {
        if (!nativeListener_ || !running_) return true;
        running_ = nativeListener_->Stop();
        return running_;
    }

    bool isRunning() const override { return running_; }

    const std::string& getListenAddress() const override { return listenAddress_; }
    uint16_t getListenPort() const override { return listenPort_; }

    uint32_t getConnectionCount() const override {
        // 需要维护连接计数
        return 0;
    }

    uint32_t getMaxConnections() const override { return maxConnections_; }

    void setMaxConnections(uint32_t maxConn) override {
        maxConnections_ = maxConn;
    }

    void setSessionFactory(SessionFactoryPtr factory) override {
        sessionFactory_ = std::move(factory);
        if (nativeListener_ && sessionFactory_) {
            // 创建适配的 SDNet 工厂
            auto* sdnetFactory = createSDNetFactory(sessionFactory_);
            nativeListener_->SetSessionFactory(sdnetFactory);
        }
    }

    void setPacketParser(PacketParserPtr parser) override {
        packetParser_ = std::move(parser);
        if (nativeListener_ && packetParser_) {
            // 创建适配的 SDNet 解析器
            auto* sdnetParser = createSDNetParser(packetParser_);
            nativeListener_->SetPacketParser(sdnetParser);
        }
    }

    void setCallbacks(const ListenerCallbacks& callbacks) override {
        callbacks_ = callbacks;
    }

    bool disconnectConnection(uint64_t connectionId, const char* reason) override {
        (void)connectionId;
        (void)reason;
        // SDNet 通过 ISSConnection::Disconnect() 断开连接
        return false;
    }

    uint32_t disconnectAll(const char* reason) override {
        (void)reason;
        return 0;
    }

    ConnectionPtr getConnection(uint64_t connectionId) const override {
        (void)connectionId;
        return nullptr;
    }

    void setNoDelay(bool enable) override { noDelay_ = enable; }

    void setRecvBufferSize(int32_t size) override {
        recvBufferSize_ = size;
    }

    void setSendBufferSize(int32_t size) override {
        sendBufferSize_ = size;
    }

    // SDNet 特定方法
    SSCP::ISSListener* getNativeListener() const { return nativeListener_; }
    void setNativeListener(SSCP::ISSListener* listener) {
        nativeListener_ = listener;
    }

private:
    virtual SSCP::ISSSessionFactory* createSDNetFactory(const SessionFactoryPtr&) {
        // 子类需要重写此方法创建具体的 SDNet 工厂
        return nullptr;
    }

    virtual SSCP::ISSPacketParser* createSDNetParser(const PacketParserPtr&) {
        // 子类需要重写此方法创建具体的 SDNet 解析器
        return nullptr;
    }

    SSCP::ISSListener* nativeListener_;
    SessionFactoryPtr sessionFactory_;
    PacketParserPtr packetParser_;
    ListenerCallbacks callbacks_;
    uint32_t maxConnections_;
    bool noDelay_;
    int32_t recvBufferSize_;
    int32_t sendBufferSize_;
    std::string listenAddress_;
    uint16_t listenPort_ = 0;
    bool running_;
};

/**
 * @brief SDNet 连接器实现
 */
class SDNetConnector : public Connector {
public:
    SDNetConnector() : SDNetConnector(nullptr) {}

    explicit SDNetConnector(SSCP::ISSConnector* nativeConnector)
        : nativeConnector_(nativeConnector)
        , state_(ConnectorState::Stopped) {}

    ~SDNetConnector() override {
        stop();
    }

    // Connector 接口实现
    int32_t connect(const ConnectConfig& config) override {
        config_ = config;
        return connect(config.host, config.port);
    }

    int32_t connect(const std::string& host, uint16_t port) override {
        if (!nativeConnector_) return -1;

        state_ = ConnectorState::Connecting;

        int result = nativeConnector_->Connect(host.c_str(), port);

        if (result >= 0) {
            state_ = ConnectorState::Connected;
        } else {
            state_ = ConnectorState::Failed;
            if (callbacks_.onConnectFailed) {
                callbacks_.onConnectFailed(result, "Connection failed");
            }
        }

        return result;
    }

    int32_t reconnect() override {
        if (!config_.host.empty()) {
            state_ = ConnectorState::Reconnecting;
            return nativeConnector_ ? nativeConnector_->ReConnect() : -1;
        }
        return -1;
    }

    void disconnect() override {
        if (nativeConnector_) {
            SSCP::ISSSession* session = nativeConnector_->GetSession();
            if (session) {
                session->ActiveClose();
            }
        }
        state_ = ConnectorState::Stopped;
        if (connection_) {
            connection_.reset();
        }
    }

    void stop() override {
        disconnect();
    }

    ConnectorState getState() const override { return state_; }

    ConnectionPtr getConnection() const override { return connection_; }

    const ConnectConfig& getConfig() const override { return config_; }

    void updateConfig(const ConnectConfig& config) override {
        config_ = config;
    }

    void setCallbacks(const ConnectorCallbacks& callbacks) override {
        callbacks_ = callbacks;
    }

    void setPacketParser(PacketParserPtr parser) override {
        packetParser_ = std::move(parser);
        if (nativeConnector_ && packetParser_) {
            // 创建适配的 SDNet 解析器
            auto* sdnetParser = createSDNetParser(packetParser_);
            nativeConnector_->SetPacketParser(sdnetParser);
        }
    }

    bool isConnected() const {
        return state_ == ConnectorState::Connected;
    }

    bool isConnecting() const {
        return state_ == ConnectorState::Connecting ||
               state_ == ConnectorState::Reconnecting;
    }

    // SDNet 特定方法
    SSCP::ISSConnector* getNativeConnector() const { return nativeConnector_; }
    void setNativeConnector(SSCP::ISSConnector* connector) {
        nativeConnector_ = connector;
    }

    void setConnection(ConnectionPtr conn) {
        connection_ = std::move(conn);
    }

    void onConnected() {
        state_ = ConnectorState::Connected;
        if (callbacks_.onConnected && connection_) {
            callbacks_.onConnected(connection_);
        }
    }

    void onDisconnected(const char* reason) {
        state_ = ConnectorState::Stopped;
        if (callbacks_.onDisconnected) {
            callbacks_.onDisconnected(reason);
        }
    }

private:
    virtual SSCP::ISSPacketParser* createSDNetParser(const PacketParserPtr&) {
        return nullptr;
    }

    SSCP::ISSConnector* nativeConnector_;
    ConnectionPtr connection_;
    PacketParserPtr packetParser_;
    ConnectorCallbacks callbacks_;
    ConnectConfig config_;
    ConnectorState state_;
};

} // namespace adapters
} // namespace net
} // namespace apollo
