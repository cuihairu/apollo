#include "apollo/net/net.h"
#include "apollo/net/adapters/native_adapter.h"

namespace apollo {
namespace net {

// 当前网络后端
static std::string g_networkBackend = "native";
static std::string g_defaultBackend = "native";

/**
 * @brief 创建默认网络管理器实例
 */
NetworkManagerPtr createNetworkManager() {
#ifdef APOLLO_USE_NATIVE
    return adapters::NativeAdapter::createManager();
#else
    return adapters::NativeAdapter::createManager();
#endif
}

/**
 * @brief 设置默认网络管理器类型
 */
void setNetworkBackend(const std::string& type) {
    g_networkBackend = type;
}

/**
 * @brief 获取当前网络后端名称
 */
const std::string& getNetworkBackend() {
    return g_networkBackend;
}

} // namespace net
} // namespace apollo
