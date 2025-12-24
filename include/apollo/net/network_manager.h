#pragma once

#include "apollo/net/listener.h"
#include "apollo/net/connector.h"
#include "apollo/net/packet_parser.h"
#include <string>
#include <memory>
#include <unordered_map>

namespace apollo {
namespace net {

/**
 * @brief 网络统计信息
 */
struct NetworkStats {
    uint64_t totalConnections = 0;     ///< 总连接数
    uint64_t currentConnections = 0;   ///< 当前连接数
    uint64_t totalBytesSent = 0;       ///< 总发送字节数
    uint64_t totalBytesReceived = 0;   ///< 总接收字节数
    uint64_t totalPacketsSent = 0;     ///< 总发送包数
    uint64_t totalPacketsReceived = 0; ///< 总接收包数
    uint64_t acceptErrors = 0;         ///< 接受连接错误数
    uint64_t readErrors = 0;           ///< 读取错误数
    uint64_t writeErrors = 0;          ///< 写入错误数
};

/**
 * @brief 网络模块配置
 */
struct NetworkConfig {
    uint32_t maxConnections = 10000;       ///< 最大连接数
    uint32_t recvBufferSize = 8192;        ///< 接收缓冲区大小
    uint32_t sendBufferSize = 8192;        ///< 发送缓冲区大小
    bool noDelay = true;                   ///< 禁用 Nagle 算法
    uint32_t workerThreads = 0;            ///< 工作线程数(0=自动)
    uint32_t heartbeatIntervalMs = 30000;  ///< 心跳间隔
    uint32_t connectionTimeoutMs = 120000; ///< 连接超时
};

/**
 * @brief 网络管理器接口
 *
 * 网络层的统一入口，负责管理所有 Listener 和 Connector
 */
class NetworkManager {
public:
    virtual ~NetworkManager() = default;

    /**
     * @brief 初始化网络模块
     * @param config 网络配置
     * @return 是否成功
     */
    virtual bool initialize(const NetworkConfig& config = NetworkConfig()) = 0;

    /**
     * @brief 启动网络模块
     * @return 是否成功
     */
    virtual bool start() = 0;

    /**
     * @brief 停止网络模块
     */
    virtual void stop() = 0;

    /**
     * @brief 检查是否正在运行
     */
    virtual bool isRunning() const = 0;

    /**
     * @brief 更新网络状态(每帧调用)
     */
    virtual void update() = 0;

    // ========== 监听器管理 ==========

    /**
     * @brief 创建监听器
     * @param name 监听器名称
     * @return 监听器指针，失败返回 nullptr
     */
    virtual ListenerPtr createListener(const std::string& name) = 0;

    /**
     * @brief 获取监听器
     */
    virtual ListenerPtr getListener(const std::string& name) const = 0;

    /**
     * @brief 移除监听器
     * @return 是否成功
     */
    virtual bool removeListener(const std::string& name) = 0;

    /**
     * @brief 移除所有监听器
     */
    virtual void removeAllListeners() = 0;

    // ========== 连接器管理 ==========

    /**
     * @brief 创建连接器
     * @param name 连接器名称
     * @return 连接器指针，失败返回 nullptr
     */
    virtual ConnectorPtr createConnector(const std::string& name) = 0;

    /**
     * @brief 获取连接器
     */
    virtual ConnectorPtr getConnector(const std::string& name) const = 0;

    /**
     * @brief 移除连接器
     * @return 是否成功
     */
    virtual bool removeConnector(const std::string& name) = 0;

    /**
     * @brief 移除所有连接器
     */
    virtual void removeAllConnectors() = 0;

    // ========== 全局连接管理 ==========

    /**
     * @brief 获取连接
     */
    virtual ConnectionPtr getConnection(uint64_t connectionId) const = 0;

    /**
     * @brief 断开连接
     */
    virtual bool disconnectConnection(uint64_t connectionId, const char* reason = nullptr) = 0;

    /**
     * @brief 广播数据到所有连接
     * @param data 数据
     * @param length 长度
     * @param excludeIds 排除的连接ID列表
     * @return 发送成功的连接数
     */
    virtual uint32_t broadcast(const char* data, uint32_t length,
                               const std::vector<uint64_t>& excludeIds = {}) = 0;

    // ========== 统计信息 ==========

    /**
     * @brief 获取网络统计信息
     */
    virtual NetworkStats getStats() const = 0;

    /**
     * @brief 重置统计信息
     */
    virtual void resetStats() = 0;

    /**
     * @brief 获取配置
     */
    virtual const NetworkConfig& getConfig() const = 0;
};

/**
 * @brief 网络管理器智能指针类型
 */
using NetworkManagerPtr = std::shared_ptr<NetworkManager>;

} // namespace net
} // namespace apollo
