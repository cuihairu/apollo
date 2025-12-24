#pragma once

#include "apollo/net/connection.h"
#include <cstdint>

namespace apollo {
namespace net {

/**
 * @brief 会话生命周期事件
 */
enum class SessionEvent : int32_t {
    Established = 0,    ///< 连接建立
    Terminated = 1,     ///< 连接断开
    Error = 2           ///< 发生错误
};

/**
 * @brief 会话终止原因
 */
enum class TerminateReason : int32_t {
    None = 0,           ///< 无原因
    UserClose = 1,      ///< 用户主动关闭
    RemoteClose = 2,    ///< 远程关闭
    Timeout = 3,        ///< 超时
    Error = 4,          /// 错误导致
    Shutdown = 5        ///< 服务器关闭
};

/**
 * @brief 抽象会话接口
 *
 * 会话代表一个客户端与服务器之间的逻辑连接
 * 负责处理应用层协议逻辑
 */
class Session {
public:
    virtual ~Session() = default;

    /**
     * @brief 连接建立时回调
     * @param connection 网络连接
     */
    virtual void onEstablish(Connection* connection) = 0;

    /**
     * @brief 连接断开时回调
     * @param reason 断开原因
     */
    virtual void onTerminate(TerminateReason reason) = 0;

    /**
     * @brief 收到数据时回调
     * @param data 数据缓冲区
     * @param length 数据长度
     * @return 处理结果的字节数，返回0表示数据不完整需要更多数据
     */
    virtual uint32_t onRecv(const char* data, uint32_t length) = 0;

    /**
     * @brief 发生错误时回调
     * @param errorCode 错误码
     * @param errorMsg 错误消息
     */
    virtual void onError(int32_t errorCode, const char* errorMsg) = 0;

    /**
     * @brief 获取会话ID
     */
    virtual uint64_t getSessionId() const = 0;

    /**
     * @brief 获取关联的连接
     */
    virtual Connection* getConnection() const = 0;

    /**
     * @brief 设置关联的连接
     */
    virtual void setConnection(Connection* connection) = 0;

    /**
     * @brief 发送数据
     */
    virtual int32_t send(const char* data, uint32_t length) {
        Connection* conn = getConnection();
        return conn ? conn->send(data, length) : -1;
    }

    /**
     * @brief 异步发送数据
     */
    virtual bool sendAsync(const char* data, uint32_t length) {
        Connection* conn = getConnection();
        return conn ? conn->sendAsync(data, length) : false;
    }

    /**
     * @brief 断开连接
     */
    virtual void disconnect(const char* reason = nullptr) {
        Connection* conn = getConnection();
        if (conn) {
            conn->disconnect(reason);
        }
    }

    /**
     * @brief 检查会话是否活跃
     */
    virtual bool isActive() const = 0;

    /**
     * @brief 更新会话心跳
     */
    virtual void updateHeartbeat() = 0;

    /**
     * @brief 获取最后心跳时间
     */
    virtual uint64_t getLastHeartbeat() const = 0;

    /**
     * @brief 检查会话是否超时
     */
    virtual bool isTimeout(uint64_t currentTime, uint64_t timeoutMs) const = 0;
};

/**
 * @brief 会话智能指针类型
 */
using SessionPtr = std::shared_ptr<Session>;

} // namespace net
} // namespace apollo
