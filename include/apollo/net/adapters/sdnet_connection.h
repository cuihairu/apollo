#pragma once

#include "apollo/net/adapters/sdnet_sscp.h"
#include "apollo/net/adapters/sdnet_util.h"
#include "apollo/net/net.h"
#include <cstdint>
#include <functional>
#include <string>

namespace apollo {
namespace net {
namespace adapters {

// ========== SDNet 具体实现类 ==========

/**
 * @brief SDNet 连接实现
 *
 * 包装 ISSConnection 接口，实现 Apollo 的 Connection 抽象
 */
class SDNetConnection : public Connection {
public:
    explicit SDNetConnection(SSCP::ISSConnection* nativeConn = nullptr)
        : nativeConnection_(nativeConn)
        , userData_(nullptr)
        , connectionId_(0)
        , connectTime_(getCurrentTimeMs())
        , lastActiveTime_(connectTime_)
        , sentBytes_(0)
        , receivedBytes_(0) {
        if (nativeConnection_) {
            remoteAddress_ = nativeConnection_->GetRemoteIPStr();
            localAddress_ = nativeConnection_->GetLocalIPStr();
        }
    }

    ~SDNetConnection() override {
        // 由 SDNet 管理生命周期，不在这里释放
    }

    // Connection 接口实现
    bool isConnected() const override {
        return nativeConnection_ && nativeConnection_->IsConnected();
    }

    ConnectionState getState() const override {
        if (isConnected()) return ConnectionState::Connected;
        return ConnectionState::Disconnected;
    }

    int32_t send(const char* data, uint32_t length) override {
        if (!nativeConnection_ || !isConnected()) return -1;
        nativeConnection_->Send(data, length);
        sentBytes_ += length;
        lastActiveTime_ = getCurrentTimeMs();
        return static_cast<int32_t>(length);
    }

    bool sendAsync(const char* data, uint32_t length) override {
        if (!nativeConnection_ || !isConnected()) return false;
        nativeConnection_->DelaySend(data, length);
        sentBytes_ += length;
        lastActiveTime_ = getCurrentTimeMs();
        return true;
    }

    void disconnect(const char* reason) override {
        (void)reason; // SDNet 不支持断开原因
        if (nativeConnection_) {
            nativeConnection_->Disconnect();
        }
    }

    void close() override {
        disconnect(nullptr);
    }

    const std::string& getRemoteAddress() const override {
        static std::string empty;
        return nativeConnection_ ? remoteAddress_ : empty;
    }

    uint16_t getRemotePort() const override {
        return nativeConnection_ ? nativeConnection_->GetRemotePort() : 0;
    }

    const std::string& getLocalAddress() const override {
        static std::string empty;
        return nativeConnection_ ? localAddress_ : empty;
    }

    uint16_t getLocalPort() const override {
        return nativeConnection_ ? nativeConnection_->GetLocalPort() : 0;
    }

    uint64_t getConnectionId() const override {
        return connectionId_;
    }

    void* getUserData() const override {
        return userData_;
    }

    void setUserData(void* data) override {
        userData_ = data;
    }

    uint64_t getLastActiveTime() const override {
        return lastActiveTime_;
    }

    uint64_t getConnectTime() const override {
        return connectTime_;
    }

    uint64_t getSentBytes() const override {
        return sentBytes_;
    }

    uint64_t getReceivedBytes() const override {
        return receivedBytes_;
    }

    // SDNet 特定方法
    SSCP::ISSConnection* getNativeConnection() const { return nativeConnection_; }
    void setNativeConnection(SSCP::ISSConnection* conn) {
        nativeConnection_ = conn;
        if (conn) {
            remoteAddress_ = conn->GetRemoteIPStr();
            localAddress_ = conn->GetLocalIPStr();
        }
    }

    void setConnectionId(uint64_t id) { connectionId_ = id; }
    void addReceivedBytes(uint64_t bytes) { receivedBytes_ += bytes; }

private:
    SSCP::ISSConnection* nativeConnection_;
    void* userData_;
    uint64_t connectionId_;
    uint64_t connectTime_;
    uint64_t lastActiveTime_;
    uint64_t sentBytes_;
    uint64_t receivedBytes_;
    std::string remoteAddress_;
    std::string localAddress_;
};

/**
 * @brief SDNet 数据包解析器适配器
 *
 * 将抽象接口适配到 ISSPacketParser
 */
class SDNetPacketParser : public SSCP::ISSPacketParser {
public:
    using ParseCallback = std::function<SSCP::INT32(const char*, SSCP::UINT32)>;

    SDNetPacketParser() : parseCallback_(nullptr) {}
    ~SDNetPacketParser() override = default;

    // ISSPacketParser 接口实现
    SSCP::INT32 ParsePacket(const char* pBuf, SSCP::UINT32 dwLen) override {
        if (parseCallback_) {
            return parseCallback_(pBuf, dwLen);
        }
        // 默认实现: 简单的长度前缀协议
        if (dwLen < 4) return 0;
        SSCP::UINT32 packetLen = *reinterpret_cast<const SSCP::UINT32*>(pBuf);
        if (packetLen > 64 * 1024) return -1; // 最大包大小限制
        if (dwLen < 4 + packetLen) return 0;
        return static_cast<SSCP::INT32>(4 + packetLen);
    }

    SSCP::INT32 ParseFirstPacket(const char* pBuf, SSCP::UINT32 dwLen) override {
        return ParsePacket(pBuf, dwLen);
    }

    void setParseCallback(ParseCallback callback) {
        parseCallback_ = std::move(callback);
    }

private:
    ParseCallback parseCallback_;
};

} // namespace adapters
} // namespace net
} // namespace apollo
