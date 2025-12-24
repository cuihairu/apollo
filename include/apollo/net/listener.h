#pragma once

#include "apollo/net/connection.h"
#include "apollo/net/session_factory.h"
#include "apollo/net/packet_parser.h"
#include <string>
#include <functional>

namespace apollo {
namespace net {

/**
 * @brief 监听器事件回调
 */
struct ListenerCallbacks {
    std::function<void(ConnectionPtr)> onAccept;          ///< 接受新连接
    std::function<void(uint64_t, const char*)> onDisconnect;  ///< 连接断开
    std::function<void(int32_t, const char*)> onError;   ///< 发生错误

    ListenerCallbacks() = default;

    // 便捷构造函数
    static ListenerCallbacks create(
        std::function<void(ConnectionPtr)> acceptCb = nullptr,
        std::function<void(uint64_t, const char*)> disconnectCb = nullptr,
        std::function<void(int32_t, const char*)> errorCb = nullptr
    ) {
        ListenerCallbacks cb;
        cb.onAccept = std::move(acceptCb);
        cb.onDisconnect = std::move(disconnectCb);
        cb.onError = std::move(errorCb);
        return cb;
    }
};

/**
 * @brief 抽象监听器接口
 *
 * 负责监听指定端口，接受传入的连接
 */
class Listener {
public:
    virtual ~Listener() = default;

    /**
     * @brief 启动监听
     * @param ip 监听IP地址，空字符串表示 INADDR_ANY
     * @param port 监听端口
     * @return 是否成功
     */
    virtual bool start(const std::string& ip, uint16_t port) = 0;

    /**
     * @brief 停止监听
     * @return 是否成功
     */
    virtual bool stop() = 0;

    /**
     * @brief 检查是否正在监听
     */
    virtual bool isRunning() const = 0;

    /**
     * @brief 获取监听地址
     */
    virtual const std::string& getListenAddress() const = 0;

    /**
     * @brief 获取监听端口
     */
    virtual uint16_t getListenPort() const = 0;

    /**
     * @brief 获取当前连接数
     */
    virtual uint32_t getConnectionCount() const = 0;

    /**
     * @brief 获取最大连接数
     */
    virtual uint32_t getMaxConnections() const = 0;

    /**
     * @brief 设置最大连接数
     */
    virtual void setMaxConnections(uint32_t maxConn) = 0;

    /**
     * @brief 设置会话工厂
     */
    virtual void setSessionFactory(SessionFactoryPtr factory) = 0;

    /**
     * @brief 设置数据包解析器
     */
    virtual void setPacketParser(PacketParserPtr parser) = 0;

    /**
     * @brief 设置事件回调
     */
    virtual void setCallbacks(const ListenerCallbacks& callbacks) = 0;

    /**
     * @brief 断开指定连接
     * @param connectionId 连接ID
     * @param reason 断开原因
     * @return 是否成功
     */
    virtual bool disconnectConnection(uint64_t connectionId, const char* reason = nullptr) = 0;

    /**
     * @brief 断开所有连接
     * @param reason 断开原因
     * @return 断开的连接数
     */
    virtual uint32_t disconnectAll(const char* reason = nullptr) = 0;

    /**
     * @brief 获取连接
     * @param connectionId 连接ID
     * @return 连接指针，不存在返回 nullptr
     */
    virtual ConnectionPtr getConnection(uint64_t connectionId) const = 0;

    /**
     * @brief 启用/禁用 Nagle 算法
     */
    virtual void setNoDelay(bool enable) = 0;

    /**
     * @brief 设置接收缓冲区大小
     */
    virtual void setRecvBufferSize(int32_t size) = 0;

    /**
     * @brief 设置发送缓冲区大小
     */
    virtual void setSendBufferSize(int32_t size) = 0;
};

/**
 * @brief 监听器智能指针类型
 */
using ListenerPtr = std::shared_ptr<Listener>;

} // namespace net
} // namespace apollo
