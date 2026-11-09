#pragma once

#include "apollo/net/adapters/sdnet_sscp.h"
#include "apollo/net/adapters/sdnet_util.h"
#include "apollo/net/adapters/sdnet_connection.h"
#include "apollo/net/net.h"
#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

namespace apollo {
namespace net {
namespace adapters {

/**
 * @brief SDNet 会话基类
 *
 * 适配 ISSSession 接口，同时提供 Apollo 抽象接口
 */
class SDNetSessionBase : public SSCP::ISSSession {
public:
    SDNetSessionBase()
        : apolloSession_(nullptr)
        , nativeConnection_(nullptr)
        , sessionId_(0)
        , lastHeartbeat_(0)
        , active_(false)
        , activeClose_(false) {}

    virtual ~SDNetSessionBase() = default;

    // ISSSession 接口实现 (由 SDNet 调用)
    void SetConnection(SSCP::ISSConnection* poConnection) override {
        nativeConnection_ = poConnection;
        if (apolloSession_) {
            apolloSession_->setConnection(
                poConnection ? &sdnetConnection_ : nullptr
            );
        }
    }

    void OnEstablish() override {
        active_ = true;
        lastHeartbeat_ = getCurrentTimeMs();
        sdnetConnection_.setNativeConnection(nativeConnection_);
        if (apolloSession_) {
            apolloSession_->onEstablish(&sdnetConnection_);
        }
        handleEstablish();
    }

    void OnTerminate() override {
        active_ = false;
        if (apolloSession_) {
            apolloSession_->onTerminate(
                activeClose_ ? TerminateReason::UserClose : TerminateReason::RemoteClose
            );
        }
        handleTerminate(activeClose_ ? TerminateReason::UserClose : TerminateReason::RemoteClose);
        nativeConnection_ = nullptr;
    }

    bool OnError(SSCP::INT32 nModuleErr, SSCP::INT32 nSysErr) override {
        (void)nSysErr;
        if (apolloSession_) {
            apolloSession_->onError(nModuleErr, "");
        }
        handleError(nModuleErr, "");
        return true;
    }

    void OnRecv(const char* pBuf, SSCP::UINT32 dwLen) override {
        lastHeartbeat_ = getCurrentTimeMs();
        if (apolloSession_) {
            apolloSession_->onRecv(pBuf, dwLen);
        }
        handlePacket(pBuf, dwLen);
    }

    void Release() override {
        delete this;
    }

    void ActiveClose() override { activeClose_ = true; }
    bool IsActiveClose() override { return activeClose_; }
    void SetActiveClose(bool bFlag) override { activeClose_ = bFlag; }

    // Apollo 会话相关
    void setApolloSession(Session* session) {
        apolloSession_ = session;
    }

    void setSessionId(std::uint64_t id) { sessionId_ = id; }

    Session* getApolloSession() const { return apolloSession_; }

    // 状态查询
    bool isActive() const { return active_; }
    uint64_t getLastHeartbeat() const { return lastHeartbeat_; }
    bool isTimeout(uint64_t currentTime, uint64_t timeoutMs) const {
        if (lastHeartbeat_ == 0) return false;
        return (currentTime - lastHeartbeat_) >= timeoutMs;
    }

protected:
    /**
     * @brief 子类重写以处理具体的业务逻辑
     */
    virtual void handleEstablish() {}
    virtual void handleTerminate(TerminateReason reason) {}
    virtual void handlePacket(const char* data, uint32_t length) {}
    virtual void handleError(int32_t errorCode, const char* errorMsg) {}

    Session* apolloSession_;
    SSCP::ISSConnection* nativeConnection_;
    uint64_t sessionId_;
    uint64_t lastHeartbeat_;
    bool active_;
    bool activeClose_;
    SDNetConnection sdnetConnection_;
};

/**
 * @brief SDNet 会话适配器
 *
 * 实现 Apollo 的 Session 接口，内部委托给 SDNetSessionBase
 */
class SDNetSession : public Session {
public:
    explicit SDNetSession(SDNetSessionBase* nativeSession)
        : nativeSession_(nativeSession)
        , connection_(nullptr)
        , sessionId_(generateSessionId())
        , lastHeartbeat_(0)
        , active_(false) {
        if (nativeSession_) {
            nativeSession_->setApolloSession(this);
            nativeSession_->setSessionId(sessionId_);
        }
    }

    ~SDNetSession() override {
        // 由 SDNet 管理生命周期
    }

    // Session 接口实现
    void onEstablish(Connection* connection) override {
        connection_ = connection;
        active_ = true;
        updateHeartbeat();
    }

    void onTerminate(TerminateReason reason) override {
        (void)reason;
        active_ = false;
        connection_ = nullptr;
    }

    uint32_t onRecv(const char* data, uint32_t length) override {
        updateHeartbeat();
        return length; // 由 SDNet 调用 native session
    }

    void onError(int32_t errorCode, const char* errorMsg) override {
        (void)errorCode;
        (void)errorMsg;
    }

    uint64_t getSessionId() const override { return sessionId_; }

    Connection* getConnection() const override { return connection_; }

    void setConnection(Connection* connection) override {
        connection_ = connection;
    }

    bool isActive() const override {
        return active_ || (nativeSession_ && nativeSession_->isActive());
    }

    void updateHeartbeat() override {
        lastHeartbeat_ = getCurrentTimeMs();
    }

    uint64_t getLastHeartbeat() const override {
        if (nativeSession_) {
            return nativeSession_->getLastHeartbeat();
        }
        return lastHeartbeat_;
    }

    bool isTimeout(uint64_t currentTime, uint64_t timeoutMs) const override {
        if (nativeSession_) {
            return nativeSession_->isTimeout(currentTime, timeoutMs);
        }
        if (lastHeartbeat_ == 0) return false;
        return (currentTime - lastHeartbeat_) >= timeoutMs;
    }

    // SDNet 特定方法
    SDNetSessionBase* getNativeSession() const { return nativeSession_; }

private:
    static uint64_t generateSessionId() {
        static std::atomic<uint64_t> counter{1};
        return counter.fetch_add(1);
    }

    SDNetSessionBase* nativeSession_;
    Connection* connection_;
    uint64_t sessionId_;
    uint64_t lastHeartbeat_;
    bool active_;
};

/**
 * @brief SDNet 会话工厂适配器
 *
 * 将 Apollo 的 SessionFactory 适配到 SDNet 的 ISSSessionFactory
 */
class SDNetSessionFactory : public SSCP::ISSSessionFactory {
public:
    explicit SDNetSessionFactory(SessionFactoryPtr apolloFactory)
        : apolloFactory_(std::move(apolloFactory)) {}

    ~SDNetSessionFactory() override = default;

    // ISSSessionFactory 接口实现
    SSCP::ISSSession* CreateSession(SSCP::ISSConnection* poConnection) override {
        if (!apolloFactory_) return nullptr;

        // 创建 Apollo 会话
        SessionPtr apolloSession = apolloFactory_->createSession();
        if (!apolloSession) return nullptr;

        // 创建 SDNet 原生会话
        auto* nativeSession = createNativeSession(apolloSession);
        if (nativeSession) {
            nativeSession->SetConnection(poConnection);
            // 保存会话映射
            sessions_[poConnection] = apolloSession;
        }

        return nativeSession;
    }

    SessionPtr getApolloSession(SSCP::ISSConnection* connection) const {
        auto it = sessions_.find(connection);
        return it != sessions_.end() ? it->second : nullptr;
    }

    void removeSession(SSCP::ISSConnection* connection) {
        sessions_.erase(connection);
    }

private:
    virtual SDNetSessionBase* createNativeSession(const SessionPtr& apolloSession) = 0;

    SessionFactoryPtr apolloFactory_;
    std::unordered_map<SSCP::ISSConnection*, SessionPtr> sessions_;
};

/**
 * @brief 通用 SDNet 会话工厂模板
 *
 * @tparam NativeSessionType SDNet 原生会话类型，必须继承自 SDNetSessionBase
 */
template <typename NativeSessionType>
class TemplateSDNetSessionFactory : public SDNetSessionFactory {
public:
    explicit TemplateSDNetSessionFactory(SessionFactoryPtr apolloFactory)
        : SDNetSessionFactory(std::move(apolloFactory)) {}

private:
    SDNetSessionBase* createNativeSession(const SessionPtr& apolloSession) override {
        auto* nativeSession = new NativeSessionType();
        nativeSession->setApolloSession(apolloSession.get());
        return nativeSession;
    }
};

} // namespace adapters
} // namespace net
} // namespace apollo
