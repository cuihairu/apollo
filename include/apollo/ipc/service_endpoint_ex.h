#pragma once

#include "apollo/ipc/service_discovery.h"
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace apollo {
namespace ipc {

//==============================================================================
// 支持的通信类型
//==============================================================================

enum class TransportType : uint8_t {
    Unknown     = 0,
    SharedMemory = 1,  // 共享内存
    UnixSocket  = 2,  // Unix Domain Socket
    Tcp         = 3,  // TCP Socket
    Udp         = 4,  // UDP Socket
    Pipe        = 5,  // 命名管道 (Windows)
    File        = 6   // 文件映射
};

// TransportType 转字符串
inline const char* transportToString(TransportType type) {
    switch (type) {
        case TransportType::SharedMemory: return "shm";
        case TransportType::UnixSocket:  return "unix";
        case TransportType::Tcp: return "tcp";
        case TransportType::Udp: return "udp";
        case TransportType::Pipe: return "pipe";
        case TransportType::File: return "file";
        default: return "unknown";
    }
}

inline TransportType transportFromString(const std::string& str) {
    if (str == "shm") return TransportType::SharedMemory;
    if (str == "unix") return TransportType::UnixSocket;
    if (str == "tcp") return TransportType::Tcp;
    if (str == "udp") return TransportType::Udp;
    if (str == "pipe") return TransportType::Pipe;
    if (str == "file") return TransportType::File;
    return TransportType::Unknown;
}

// 通信类型位掩码（用于组合多种类型）
enum class TransportMask : uint8_t {
    None          = 0b000000,
    SharedMemory  = 0b000001,
    UnixSocket    = 0b000010,
    Tcp           = 0b000100,
    Udp           = 0b001000,
    Pipe          = 0b010000,
    File          = 0b100000,
    All           = 0b111111
};

// 运算符重载
inline TransportMask operator|(TransportMask a, TransportMask b) {
    return static_cast<TransportMask>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline TransportMask operator&(TransportMask a, TransportMask b) {
    return static_cast<TransportMask>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline bool hasTransport(TransportMask mask, TransportType type) {
    return (static_cast<uint8_t>(mask) & (1 << static_cast<uint8_t>(type))) != 0;
}

//==============================================================================
// 优先级定义（本地优先）
//==============================================================================

enum class EndpointPriority : uint8_t {
    SharedMemory  = 1,  // 最高优先级：同进程共享内存
    UnixSocket    = 2,  // 同机 Unix Socket
    LocalTcp       = 3,  // localhost TCP
    SameSubnet     = 4,  // 同子网 TCP
    CrossSubnet    = 5,  // 跨子网 TCP
    Remote         = 6   // 远程
};

//==============================================================================
// 服务ID 编码（64位 long 分段）
//==============================================================================
/*
 * 64位服务ID 分段设计：
 *
 * Bit 63-56 (8位):  数据中心/机房 ID
 * Bit 55-48 (8位):  服务器/主机 ID
 * Bit 47-40 (8位):  进程/服务类型 ID
 * Bit 39-32 (8位):  实例编号
 * Bit 31-16 (16位): 端口号
 * Bit 15-0  (16位): 序列号/校验
 *
 * 示例: 0x01 02 03 04 1F90 0001
 *        --  --  --  -- ---- ----
 *        DC  Host Type Inst Port Seq
 *
 * 通过 ID 可以快速判断：
 * - 数据中心是否相同（Bit 63-56）
 * - 主机是否相同（Bit 55-48）
 * - 是否本地服务（DC + Host 都为 0 表示本地）
 * - 端口号（Bit 31-16）
 */

struct ServiceId {
    uint8_t  dataCenter = 0;   // 数据中心 ID (0-255)
    uint8_t  host = 0;         // 主机 ID (0-255, 0 表示本地)
    uint8_t  serviceType = 0;   // 服务类型 ID
    uint8_t  instance = 0;      // 实例编号
    uint16_t port = 0;         // 端口号
    uint16_t sequence = 0;      // 序列号

    uint64_t toUint64() const {
        return (static_cast<uint64_t>(dataCenter) << 56) |
               (static_cast<uint64_t>(host) << 48) |
               (static_cast<uint64_t>(serviceType) << 40) |
               (static_cast<uint64_t>(instance) << 32) |
               (static_cast<uint64_t>(port) << 16) |
               sequence;
    }

    static ServiceId fromUint64(uint64_t value) {
        ServiceId id;
        id.dataCenter  = (value >> 56) & 0xFF;
        id.host        = (value >> 48) & 0xFF;
        id.serviceType = (value >> 40) & 0xFF;
        id.instance    = (value >> 32) & 0xFF;
        id.port        = (value >> 16) & 0xFFFF;
        id.sequence    = value & 0xFFFF;
        return id;
    }

    // 判断是否本地服务
    bool isLocal() const {
        return dataCenter == 0 && host == 0;
    }

    // 获取优先级
    EndpointPriority getPriority() const {
        if (isLocal()) {
            // 本地服务根据支持的传输类型进一步细分
            return EndpointPriority::LocalTcp;
        }
        return EndpointPriority::Remote;
    }

    // 生成字符串表示
    std::string toString() const {
        char buf[32];
        snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%04X%04X",
                 dataCenter, host, serviceType, instance, port, sequence);
        return std::string(buf);
    }
};

//==============================================================================
// 扩展的端点地址信息（五元组）
//==============================================================================

struct EndpointAddress {
    // 源地址（服务端）
    std::string serverHost;      // 服务器地址
    uint16_t serverPort = 0;     // 服务器端口

    // 目标地址（客户端，用于反向连接）
    std::string clientHost;
    uint16_t clientPort = 0;

    // 传输协议
    std::string protocol = "tcp";  // tcp, udp, unix

    // Unix Socket 路径
    std::string unixPath;

    // 共享内存名称
    std::string shmName;

    // 判断是否本地可达
    bool isLocalAccessible(const std::string& localHost = "127.0.0.1") const {
        // 1. 共享内存：总是本地
        // 2. Unix Socket：总是本地
        // 3. TCP: 检查是否 localhost 或 0.0.0.0
        if (protocol == "unix" || !shmName.empty()) return true;
        return serverHost == "127.0.0.1" ||
               serverHost == "localhost" ||
               serverHost == "0.0.0.0" ||
               serverHost == localHost;
    }

    // 获取连接字符串（用于 Channel）
    std::string getConnectionString() const {
        if (protocol == "unix") {
            return "unix://" + unixPath;
        }
        if (!shmName.empty()) {
            return "shm://" + shmName;
        }
        return serverHost + ":" + std::to_string(serverPort);
    }
};

//==============================================================================
// 扩展的服务端点信息
//==============================================================================

struct ServiceEndpointEx : public ServiceEndpoint {
    // 支持的传输类型掩码
    TransportMask transportMask = TransportMask::Tcp;

    // 各传输类型的地址信息
    EndpointAddress shmAddress;      // 共享内存地址
    EndpointAddress unixAddress;     // Unix Socket 地址
    EndpointAddress tcpAddress;      // TCP 地址

    // 扩展信息
    std::string pid;                 // 进程 ID（用于验证）
    uint32_t startTime = 0;           // 服务启动时间

    // 部署信息
    std::string dataCenter;          // 数据中心
    std::string rack;                // 机架
    std::string host;                // 主机名
    uint8_t hostId = 0;              // 主机ID

    // 优先级（计算得出）
    EndpointPriority priority = EndpointPriority::Remote;

    //==========================================================================
    // 根据 ServiceId 生成 ID
    //==========================================================================

    static std::string generateId(const ServiceId& sid) {
        return sid.toString();
    }

    static ServiceId parseId(const std::string& str) {
        uint64_t value = std::stoull(str, nullptr, 16);
        return ServiceId::fromUint64(value);
    }

    //==========================================================================
    // 获取最佳连接地址（本地优先）
    //==========================================================================

    EndpointAddress getBestAddress(const std::string& localHost = "127.0.0.1") const {
        // 优先级顺序
        // 1. 共享内存
        if (hasTransport(transportMask, TransportType::SharedMemory)) {
            // 检查是否可访问（同一进程或同主机）
            if (shmAddress.isLocalAccessible(localHost)) {
                return shmAddress;
            }
        }

        // 2. Unix Socket
        if (hasTransport(transportMask, TransportType::UnixSocket)) {
            if (unixAddress.isLocalAccessible(localHost)) {
                return unixAddress;
            }
        }

        // 3. Local TCP
        if (tcpAddress.isLocalAccessible(localHost)) {
            return tcpAddress;
        }

        // 4. 远程 TCP
        return tcpAddress;
    }

    //==========================================================================
    // 获取特定类型的地址
    //==========================================================================

    EndpointAddress getAddress(TransportType type) const {
        switch (type) {
            case TransportType::SharedMemory: return shmAddress;
            case TransportType::UnixSocket:    return unixAddress;
            case TransportType::Tcp:
            case TransportType::Udp:           return tcpAddress;
            default: return {};
        }
    }

    //==========================================================================
    // 判断是否支持某种传输类型
    //==========================================================================

    bool supportsTransport(TransportType type) const {
        return hasTransport(transportMask, type);
    }

    //==========================================================================
    // 计算本地优先级
    //==========================================================================

    void calculatePriority(const std::string& localDataCenter = "0",
                           const std::string& localHost = "0",
                           const std::string& localSubnet = "") {
        if (dataCenter == localDataCenter && host == localHost) {
            // 同主机
            if (hasTransport(transportMask, TransportType::SharedMemory)) {
                priority = EndpointPriority::SharedMemory;
            } else if (hasTransport(transportMask, TransportType::UnixSocket)) {
                priority = EndpointPriority::UnixSocket;
            } else {
                priority = EndpointPriority::LocalTcp;
            }
        } else if (dataCenter == localDataCenter) {
            // 同数据中心
            priority = EndpointPriority::SameSubnet;
        } else {
            // 跨数据中心
            priority = EndpointPriority::CrossSubnet;
        }
    }

    //==========================================================================
    // 序列化/反序列化
    //==========================================================================

    // 转换为 JSON（包含所有扩展信息）
    std::string toJsonEx() const;

    // 从 JSON 解析
    static ServiceEndpointEx fromJsonEx(const std::string& json);

    // 从基础 ServiceEndpoint 转换
    static ServiceEndpointEx fromBase(const ServiceEndpoint& base) {
        ServiceEndpointEx ex;
        ex.id = base.id;
        ex.host = base.host;
        ex.port = base.port;
        ex.protocol = base.protocol;
        ex.metadata = base.metadata;
        ex.lastHeartbeat = base.lastHeartbeat;
        ex.healthy = base.healthy;
        ex.weight = base.weight;
        return ex;
    }

    // 转换为基础 ServiceEndpoint
    ServiceEndpoint toBase() const {
        ServiceEndpoint base;
        base.id = id;
        base.host = host;
        base.port = port;
        base.protocol = protocol;
        base.metadata = metadata;
        base.lastHeartbeat = lastHeartbeat;
        base.healthy = healthy;
        base.weight = weight;
        return base;
    }
};

//==============================================================================
// 本地优先端点选择器
//==============================================================================

class LocalFirstSelector {
public:
    // 配置
    struct Config {
        std::string localDataCenter = "0";
        std::string localHost = "0";
        std::string localSubnet = "";
        bool preferSharedMemory = true;
        bool preferUnixSocket = true;
    };

    LocalFirstSelector(const Config& config = Config{})
        : config_(config) {}

    // 从端点列表中选择最佳端点
    ServiceEndpointEx select(const std::vector<ServiceEndpointEx>& endpoints) const;

    // 过滤出本地可达的端点
    std::vector<ServiceEndpointEx> filterLocal(const std::vector<ServiceEndpointEx>& endpoints) const;

    // 按优先级排序
    std::vector<ServiceEndpointEx> sortByPriority(std::vector<ServiceEndpointEx> endpoints) const;

private:
    Config config_;
};

//==============================================================================
// 在 ServiceEndpoint 基础上扩展元数据字段
//==============================================================================

// 扩展元数据键名
namespace MetaKeys {
    constexpr const char* TRANSPORTS = "transports";        // 支持的传输类型 "shm|unix|tcp"
    constexpr const char* SHM_PATH = "shm_path";          // 共享内存路径
    constexpr const char* UNIX_PATH = "unix_path";        // Unix Socket 路径
    constexpr const char* DATA_CENTER = "datacenter";     // 数据中心
    constexpr const char* RACK = "rack";                   // 机架
    constexpr const char* HOST = "host";                   // 主机名
    constexpr const char* HOST_ID = "host_id";             // 主机ID
    constexpr const char* PID = "pid";                    // 进程ID
    constexpr const char* START_TIME = "start_time";       // 启动时间
    constexpr const char* PRIORITY = "priority";           // 优先级
    constexpr const char* WEIGHT = "weight";               // 权重
    constexpr const char* VERSION = "version";             // 版本号
}

// 在 metadata 中设置传输类型
inline void setTransports(ServiceEndpoint& endpoint, TransportMask mask) {
    std::vector<std::string> types;
    if (hasTransport(mask, TransportType::SharedMemory)) types.push_back("shm");
    if (hasTransport(mask, TransportType::UnixSocket)) types.push_back("unix");
    if (hasTransport(mask, TransportType::Tcp)) types.push_back("tcp");
    if (hasTransport(mask, TransportType::Udp)) types.push_back("udp");
    if (hasTransport(mask, TransportType::Pipe)) types.push_back("pipe");

    std::string value;
    for (size_t i = 0; i < types.size(); ++i) {
        if (i > 0) value += "|";
        value += types[i];
    }
    endpoint.metadata[MetaKeys::TRANSPORTS] = value;
}

// 从 metadata 读取传输类型
inline TransportMask parseTransports(const ServiceEndpoint& endpoint) {
    auto it = endpoint.metadata.find(MetaKeys::TRANSPORTS);
    if (it == endpoint.metadata.end()) return TransportMask::Tcp;

    TransportMask mask = TransportMask::None;
    std::string value = it->second;

    // 简单解析
    if (value.find("shm") != std::string::npos)
        mask = mask | TransportMask::SharedMemory;
    if (value.find("unix") != std::string::npos)
        mask = mask | TransportMask::UnixSocket;
    if (value.find("tcp") != std::string::npos)
        mask = mask | TransportMask::Tcp;
    if (value.find("udp") != std::string::npos)
        mask = mask | TransportMask::Udp;
    if (value.find("pipe") != std::string::npos)
        mask = mask | TransportMask::Pipe;

    return mask;
}

} // namespace ipc
} // namespace apollo
