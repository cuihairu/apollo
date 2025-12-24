#pragma once

#include <cstdint>
#include <string>

namespace apollo {
namespace net {

/**
 * @brief 网络连接状态
 */
enum class ConnectionState : int32_t {
    Disconnected = 0,   ///< 未连接
    Connecting = 1,     ///< 连接中
    Connected = 2,      ///< 已连接
    Disconnecting = 3   ///< 断开中
};

/**
 * @brief 抽象网络连接接口
 *
 * 封装 TCP 连接的通用操作，可由不同网络库实现
 * - SSEngine/SDNet (IOCP/epoll)
 * - Boost.Asio
 * - libuv
 * - 自定义网络层
 */
class Connection {
public:
    virtual ~Connection() = default;

    /**
     * @brief 检查连接是否建立
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief 获取连接状态
     */
    virtual ConnectionState getState() const = 0;

    /**
     * @brief 发送数据
     * @param data 数据缓冲区
     * @param length 数据长度
     * @return 实际发送的字节数，-1 表示失败
     */
    virtual int32_t send(const char* data, uint32_t length) = 0;

    /**
     * @brief 发送数据(带缓冲)
     * @param data 数据缓冲区
     * @param length 数据长度
     * @return 是否成功加入发送队列
     */
    virtual bool sendAsync(const char* data, uint32_t length) = 0;

    /**
     * @brief 断开连接
     * @param reason 断开原因(可选)
     */
    virtual void disconnect(const char* reason = nullptr) = 0;

    /**
     * @brief 强制关闭连接(不等待发送队列完成)
     */
    virtual void close() = 0;

    /**
     * @brief 获取远程地址
     */
    virtual const std::string& getRemoteAddress() const = 0;

    /**
     * @brief 获取远程端口
     */
    virtual uint16_t getRemotePort() const = 0;

    /**
     * @brief 获取本地地址
     */
    virtual const std::string& getLocalAddress() const = 0;

    /**
     * @brief 获取本地端口
     */
    virtual uint16_t getLocalPort() const = 0;

    /**
     * @brief 获取连接ID(由网络层分配)
     */
    virtual uint64_t getConnectionId() const = 0;

    /**
     * @brief 获取用户数据指针
     */
    virtual void* getUserData() const = 0;

    /**
     * @brief 设置用户数据指针
     */
    virtual void setUserData(void* data) = 0;

    /**
     * @brief 获取最后活跃时间(时间戳)
     */
    virtual uint64_t getLastActiveTime() const = 0;

    /**
     * @brief 获取连接建立时间(时间戳)
     */
    virtual uint64_t getConnectTime() const = 0;

    /**
     * @brief 获取已发送字节数
     */
    virtual uint64_t getSentBytes() const = 0;

    /**
     * @brief 获取已接收字节数
     */
    virtual uint64_t getReceivedBytes() const = 0;
};

/**
 * @brief 连接智能指针类型
 */
using ConnectionPtr = std::shared_ptr<Connection>;

} // namespace net
} // namespace apollo
