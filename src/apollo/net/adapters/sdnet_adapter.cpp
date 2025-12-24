#include "apollo/net/adapters/sdnet_adapter.h"

#ifdef APOLLO_USE_SDNET

namespace apollo {
namespace net {
namespace adapters {

// ========== SDNetAdapter 静态方法 ==========

NetworkManagerPtr SDNetAdapter::createManager() {
    return std::make_shared<SDNetNetworkManager>();
}

bool SDNetAdapter::initialize(const NetworkConfig& config) {
    // SDNet 的初始化需要通过 SSNetGetModule 获取模块
    // 这里只是预留接口，实际初始化在网络管理器中进行
    (void)config;
    return true;
}

void SDNetAdapter::shutdown() {
    // SDNet 的清理工作
}

std::string SDNetAdapter::getVersion() {
    return "SDNet/5.0.0"; // 根据 SSEngine 版本调整
}

void SDNetAdapter::setLogCallback(std::function<void(int32_t level, const char* msg)> callback) {
    // 调用 SSNetSetLogger 设置日志
    (void)callback;
}

} // namespace adapters
} // namespace net
} // namespace apollo

#endif // APOLLO_USE_SDNET
