#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include "server_types.hpp"

namespace apollo::core {

/// 服务器ID类 - 64位标识符
/// 格式: Region(16bit) | Group(16bit) | Type(16bit) | Instance(16bit)
class ServerId {
private:
    uint64_t id_ = 0;

public:
    constexpr ServerId() = default;
    constexpr explicit ServerId(uint64_t id) : id_(id) {}

    /// 创建ServerId
    static constexpr ServerId Create(uint16_t region, uint16_t group,
                                     uint16_t type, uint16_t instance) {
        return ServerId(
            (static_cast<uint64_t>(region) << 48) |
            (static_cast<uint64_t>(group) << 32) |
            (static_cast<uint64_t>(type) << 16) |
            static_cast<uint64_t>(instance)
        );
    }

    /// 创建ServerId (使用枚举类型)
    static constexpr ServerId Create(uint16_t region, uint16_t group,
                                     ServerType type, uint16_t instance) {
        return Create(region, group, static_cast<uint16_t>(type), instance);
    }

    /// 从字符串创建 "1-2-6-1"
    static ServerId FromString(const std::string& str) {
        uint16_t r, g, t, i;
        if (sscanf(str.c_str(), "%hu-%hu-%hu-%hu", &r, &g, &t, &i) == 4) {
            return Create(r, g, t, i);
        }
        return ServerId{};
    }

    /// 获取区域ID
    constexpr uint16_t GetRegion() const {
        return static_cast<uint16_t>(id_ >> 48);
    }

    /// 获取逻辑分组ID
    constexpr uint16_t GetGroup() const {
        return static_cast<uint16_t>(id_ >> 32);
    }

    /// 获取服务类型数值
    constexpr uint16_t GetTypeValue() const {
        return static_cast<uint16_t>(id_ >> 16);
    }

    /// 获取服务类型
    constexpr ServerType GetType() const {
        return static_cast<ServerType>(GetTypeValue());
    }

    /// 获取实例编号
    constexpr uint16_t GetInstance() const {
        return static_cast<uint16_t>(id_);
    }

    /// 转换为uint64
    constexpr uint64_t ToUint64() const { return id_; }

    /// 转换为字符串 "1-2-6-1"
    std::string ToString() const {
        return std::to_string(GetRegion()) + "-" +
               std::to_string(GetGroup()) + "-" +
               std::to_string(GetTypeValue()) + "-" +
               std::to_string(GetInstance());
    }

    /// 获取完整的名称 "GameServer-6-1"
    std::string GetFullName() const {
        return std::string(GetServerTypeName(GetType())) +
               "-" + std::to_string(GetTypeValue()) +
               "-" + std::to_string(GetInstance());
    }

    /// 是否有效
    constexpr explicit operator bool() const { return id_ != 0; }

    /// 匹配器 - 检查是否匹配特定类型
    constexpr bool MatchType(ServerType type) const {
        return GetType() == type;
    }

    /// 匹配器 - 检查是否匹配特定区域
    constexpr bool MatchRegion(uint16_t region) const {
        return GetRegion() == region;
    }

    /// 匹配器 - 检查是否匹配特定分组
    constexpr bool MatchGroup(uint16_t group) const {
        return GetGroup() == group;
    }

    /// 比较操作符
    constexpr bool operator==(const ServerId& other) const {
        return id_ == other.id_;
    }

    constexpr bool operator!=(const ServerId& other) const {
        return id_ != other.id_;
    }

    constexpr bool operator<(const ServerId& other) const {
        return id_ < other.id_;
    }

    constexpr bool operator>(const ServerId& other) const {
        return id_ > other.id_;
    }

    constexpr bool operator<=(const ServerId& other) const {
        return id_ <= other.id_;
    }

    constexpr bool operator>=(const ServerId& other) const {
        return id_ >= other.id_;
    }
};

/// ServerId哈希函数对象 (用于unordered_map)
struct ServerIdHash {
    size_t operator()(const ServerId& id) const noexcept {
        return std::hash<uint64_t>{}(id.ToUint64());
    }
};

/// 注册中心存储的服务元数据
struct ServerMeta {
    ServerId id;                         // 逻辑标识 (稳定不变)

    // === 运行时信息 (动态变化) ===
    std::string host;                     // 机器IP或主机名
    uint16_t port = 0;                    // TCP监听端口
    std::string unixSocketPath;           // Unix Socket路径 (可选)
    uint32_t pid = 0;                     // 进程ID
    uint64_t startTime = 0;               // 启动时间 (Unix时间戳)
    std::string version;                  // 版本号

    // === 状态信息 ===
    ServerStatus status = ServerStatus::OFFLINE;
    uint64_t lastHeartbeat = 0;           // 最后心跳时间
    uint32_t load = 0;                    // 负载值 (用于负载均衡)

    // === 能力标记 ===
    bool supportsUnixSocket = false;      // 是否支持 Unix Socket
    bool supportsSharedMemory = false;    // 是否支持共享内存
    bool supportsWebsocket = false;       // 是否支持WebSocket

    ServerMeta() = default;
    ServerMeta(ServerId id) : id(id) {}
};

}  // namespace apollo::core