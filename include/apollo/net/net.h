#pragma once

/**
 * @file net.h
 * @brief Apollo 网络抽象层统一头文件
 *
 * 提供与具体网络库实现无关的网络接口
 * 支持 SSEngine/SDNet、Boost.Asio、libuv 等多种实现
 *
 * 默认使用原生网络实现 (Winsock2/epoll)
 */

#include "apollo/net/connection.h"
#include "apollo/net/session.h"
#include "apollo/net/session_factory.h"
#include "apollo/net/packet_parser.h"
#include "apollo/net/listener.h"
#include "apollo/net/connector.h"
#include "apollo/net/network_manager.h"

namespace apollo {
namespace net {

/**
 * @brief 创建默认网络管理器实例
 *
 * 根据编译选项选择网络库实现:
 * - APOLLO_USE_NATIVE: 原生网络实现 (默认)
 * - APOLLO_USE_SDNET: SSEngine/SDNet
 * - APOLLO_USE_ASIO: Boost.Asio
 * - APOLLO_USE_LIBUV: libuv
 *
 * @return 网络管理器指针
 */
NetworkManagerPtr createNetworkManager();

/**
 * @brief 设置默认网络管理器类型
 *
 * @param type 网络库类型 ("native", "sdnet", "asio", "libuv")
 */
void setNetworkBackend(const std::string& type);

/**
 * @brief 获取当前网络后端名称
 */
const std::string& getNetworkBackend();

} // namespace net
} // namespace apollo
