#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>

namespace apollo {
namespace net {
namespace adapters {

/**
 * @brief SDNet 网络后端标识
 */
constexpr const char* SDNET_BACKEND = "sdnet";

/**
 * @brief 获取当前时间戳(毫秒)
 */
inline uint64_t getCurrentTimeMs() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

/**
 * @brief IP 地址转换工具
 */
class IpAddressConverter {
public:
    /**
     * @brief UINT32 IP 转字符串
     */
    static std::string ipToString(uint32_t ip) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u",
                 ip & 0xFF,
                 (ip >> 8) & 0xFF,
                 (ip >> 16) & 0xFF,
                 (ip >> 24) & 0xFF);
        return std::string(buffer);
    }

    /**
     * @brief 字符串转 UINT32 IP
     */
    static uint32_t ipFromString(const std::string& ip) {
        unsigned int a = 0, b = 0, c = 0, d = 0;
        if (sscanf(ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            return (d << 24) | (c << 16) | (b << 8) | a;
        }
        return 0;
    }
};

} // namespace adapters
} // namespace net
} // namespace apollo
