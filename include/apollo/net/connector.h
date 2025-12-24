#pragma once

#include "apollo/net/connection.h"
#include "apollo/net/packet_parser.h"
#include <string>
#include <functional>

namespace apollo {
namespace net {

/**
 * @brief 连接器状态
 */
enum class ConnectorState : int32_t {
    Stopped = 0,        ///< 已停止
    Connecting = 1,     ///< 连接中
    Connected = 2,      ///< 已连接
    Reconnecting = 3,   ///< 重连中
    Failed = 4          ///< 连接失败
};

/**
 * @brief 连接器事件回调
 */
struct ConnectorCallbacks {
    std::function<void(ConnectionPtr)> onConnected;           ///< 连接成功
    std::function<void(const char*)> onDisconnected;          ///< 连接断开
    std::function<void(int32_t, const char*)> onConnectFailed;///< 连接失败

    ConnectorCallbacks() = default;

    // 便捷构造函数
    static ConnectorCallbacks create(
        std::function<void(ConnectionPtr)> connectedCb = nullptr,
        std::function<void(const char*)> disconnectedCb = nullptr,
        std::function<void(int32_t, const char*)> failedCb = nullptr
    ) {
        ConnectorCallbacks cb;
        cb.onConnected = std::move(connectedCb);
        cb.onDisconnected = std::move(disconnectedCb);
        cb.onConnectFailed = std::move(failedCb);
        return cb;
    }
};

/**
 * @brief 连接配置
 */
struct ConnectConfig {
    std::string host;           ///< 目标主机
    uint16_t port = 0;          ///< 目标端口
    uint32_t timeoutMs = 5000;  ///< 连接超时(毫秒)
    bool autoReconnect = false; ///< 是否自动重连
    uint32_t maxRetries = 3;    ///< 最大重试次数
    uint32_t retryIntervalMs = 1000;  ///< 重试间隔(毫秒)

    ConnectConfig() = default;

    ConnectConfig(std::string h, uint16_t p)
        : host(std::move(h)), port(p) {}
};

/**
 * @brief 抽象连接器接口
 *
 * 负责主动发起 TCP 连接
 */
class Connector {
public:
    virtual ~Connector() = default;

    /**
     * @brief 连接到指定地址
     * @param config 连接配置
     * @return 0表示成功，负数表示错误码
     */
    virtual int32_t connect(const ConnectConfig& config) = 0;

    /**
     * @brief 连接到指定地址(简化版本)
     */
    virtual int32_t connect(const std::string& host, uint16_t port) = 0;

    /**
     * @brief 重新连接
     * @return 0表示成功，负数表示错误码
     */
    virtual int32_t reconnect() = 0;

    /**
     * @brief 断开连接
     */
    virtual void disconnect() = 0;

    /**
     * @brief 停止连接器(释放资源)
     */
    virtual void stop() = 0;

    /**
     * @brief 获取连接状态
     */
    virtual ConnectorState getState() const = 0;

    /**
     * @brief 获取当前连接
     */
    virtual ConnectionPtr getConnection() const = 0;

    /**
     * @brief 获取连接配置
     */
    virtual const ConnectConfig& getConfig() const = 0;

    /**
     * @brief 更新连接配置
     */
    virtual void updateConfig(const ConnectConfig& config) = 0;

    /**
     * @brief 设置事件回调
     */
    virtual void setCallbacks(const ConnectorCallbacks& callbacks) = 0;

    /**
     * @brief 设置数据包解析器
     */
    virtual void setPacketParser(PacketParserPtr parser) = 0;

    /**
     * @brief 检查是否已连接
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief 检查是否正在连接
     */
    virtual bool isConnecting() const = 0;
};

/**
 * @brief 连接器智能指针类型
 */
using ConnectorPtr = std::shared_ptr<Connector>;

} // namespace net
} // namespace apollo
