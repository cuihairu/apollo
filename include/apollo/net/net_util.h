#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <iostream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <mstcpip.h>
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    typedef int SOCKET;
#endif

namespace apollo {
namespace net {

//==============================================================================
// IPv4 地址封装
//==============================================================================

class IPv4Address {
public:
    IPv4Address() : ip_(0), port_(0) {}

    IPv4Address(uint32_t ip, uint16_t port)
        : ip_(ip), port_(port) {}

    IPv4Address(const std::string& ip, uint16_t port)
        : port_(port) {
        ip_ = stringToIp(ip);
    }

    IPv4Address(const struct sockaddr_in& addr)
        : port_(ntohs(addr.sin_port)) {
        ip_ = ntohl(addr.sin_addr.s_addr);
    }

    // 获取 IP（网络字节序）
    uint32_t ipNet() const { return htonl(ip_); }

    // 获取 IP（主机字节序）
    uint32_t ipHost() const { return ip_; }

    // 获取端口
    uint16_t port() const { return port_; }

    // 获取端口（网络字节序）
    uint16_t portNet() const { return htons(port_); }

    // 设置 IP
    void setIp(uint32_t ip) { ip_ = ip; }

    // 设置端口
    void setPort(uint16_t port) { port_ = port; }

    // 转换为字符串
    std::string toString() const {
        return ipToString() + ":" + std::to_string(port_);
    }

    // IP 转字符串
    std::string ipToString() const {
        struct in_addr addr;
        addr.s_addr = ipNet();
        char buffer[INET_ADDRSTRLEN];
        const char* result = inet_ntop(AF_INET, &addr, buffer, sizeof(buffer));
        return result ? buffer : "invalid";
    }

    // 是否为有效地址
    bool isValid() const { return ip_ != 0; }

    // 是否为本地地址
    bool isLocalhost() const {
        return ip_ == 0x7F000001; // 127.0.0.1
    }

    // 是否为组播地址
    bool isMulticast() const {
        return (ip_ & 0xF0000000) == 0xE0000000;
    }

    // 转换为 sockaddr_in
    struct sockaddr_in toSockAddr() const {
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = ipNet();
        addr.sin_port = portNet();
        return addr;
    }

    // 解析字符串 "ip:port"
    static IPv4Address parse(const std::string& addr) {
        size_t pos = addr.find(':');
        if (pos == std::string::npos) {
            return IPv4Address();
        }
        std::string ip = addr.substr(0, pos);
        uint16_t port = static_cast<uint16_t>(std::stoi(addr.substr(pos + 1)));
        return IPv4Address(ip, port);
    }

    // 字符串转 IP
    static uint32_t stringToIp(const std::string& ip) {
        struct in_addr addr;
        if (inet_pton(AF_INET, ip.c_str(), &addr) == 1) {
            return ntohl(addr.s_addr);
        }
        return 0;
    }

    // 获取所有本地 IP
    static std::vector<IPv4Address> getLocalAddresses() {
        std::vector<IPv4Address> result;

        // 添加 localhost
        result.emplace_back(0x7F000001, 0);

        // TODO: 遍历网络接口获取实际 IP
        // Windows: GetAdaptersAddresses
        // Linux: ioctl SIOCGIFCONF 或 getifaddrs

        return result;
    }

    // 通配符地址 (0.0.0.0)
    static IPv4Address any() { return IPv4Address(INADDR_ANY, 0); }

    // 本地回环地址 (127.0.0.1)
    static IPv4Address loopback() { return IPv4Address(0x7F000001, 0); }

private:
    uint32_t ip_;   // 主机字节序
    uint16_t port_;
};

//==============================================================================
// 端口范围
//==============================================================================

class PortRange {
public:
    PortRange(uint16_t min, uint16_t max) : min_(min), max_(max) {}

    uint16_t min() const { return min_; }
    uint16_t max() const { return max_; }

    bool contains(uint16_t port) const {
        return port >= min_ && port <= max_;
    }

    // 常用端口范围
    static PortRange wellKnown() { return {0, 1023}; }      // 知名端口
    static PortRange registered() { return {1024, 49151}; } // 注册端口
    static PortRange dynamic() { return {49152, 65535}; }   // 动态端口
    static PortRange ephemeral() { return dynamic(); }

    // 是否为系统保留端口
    static bool isSystemPort(uint16_t port) {
        return port < 1024;
    }

private:
    uint16_t min_;
    uint16_t max_;
};

//==============================================================================
// Socket 选项
//==============================================================================

class SocketOption {
public:
    // 设置非阻塞模式
    static bool setNonBlocking(SOCKET socket, bool nonBlocking = true) {
#ifdef _WIN32
        u_long mode = nonBlocking ? 1 : 0;
        return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
        int flags = fcntl(socket, F_GETFL, 0);
        if (flags == -1) return false;
        return fcntl(socket, F_SETFL,
            nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)) == 0;
#endif
    }

    // 设置地址复用
    static bool setReuseAddr(SOCKET socket, bool reuse = true) {
        int optval = reuse ? 1 : 0;
        return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR,
            reinterpret_cast<const char*>(&optval), sizeof(optval)) == 0;
    }

#ifdef SO_REUSEPORT
    static bool setReusePort(SOCKET socket, bool reuse = true) {
        int optval = reuse ? 1 : 0;
        return setsockopt(socket, SOL_SOCKET, SO_REUSEPORT,
            reinterpret_cast<const char*>(&optval), sizeof(optval)) == 0;
    }
#endif

    // 禁用 Nagle 算法
    static bool setNoDelay(SOCKET socket, bool noDelay = true) {
        int optval = noDelay ? 1 : 0;
        return setsockopt(socket, IPPROTO_TCP, TCP_NODELAY,
            reinterpret_cast<const char*>(&optval), sizeof(optval)) == 0;
    }

    // 设置发送缓冲区大小
    static bool setSendBufferSize(SOCKET socket, int size) {
        return setsockopt(socket, SOL_SOCKET, SO_SNDBUF,
            reinterpret_cast<const char*>(&size), sizeof(size)) == 0;
    }

    // 设置接收缓冲区大小
    static bool setRecvBufferSize(SOCKET socket, int size) {
        return setsockopt(socket, SOL_SOCKET, SO_RCVBUF,
            reinterpret_cast<const char*>(&size), sizeof(size)) == 0;
    }

    // 设置保持连接
    static bool setKeepAlive(SOCKET socket, bool keepAlive = true) {
        int optval = keepAlive ? 1 : 0;
        return setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE,
            reinterpret_cast<const char*>(&optval), sizeof(optval)) == 0;
    }

    // 设置 keep-alive 参数
    static bool setKeepAliveParams(SOCKET socket, int idleSec, int intervalSec, int count) {
#ifdef _WIN32
        tcp_keepalive ka;
        ka.onoff = 1;
        ka.keepalivetime = idleSec * 1000;
        ka.keepaliveinterval = intervalSec * 1000;
        DWORD bytesReturned;
        return WSAIoctl(socket, SIO_KEEPALIVE_VALS, &ka, sizeof(ka),
            nullptr, 0, &bytesReturned, nullptr, nullptr) == 0;
#else
        // Linux
        int keepIdle = idleSec;
        int keepInterval = intervalSec;
        int keepCount = count;

        if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE,
            reinterpret_cast<const char*>(&keepIdle), sizeof(keepIdle)) != 0) {
            return false;
        }
        if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL,
            reinterpret_cast<const char*>(&keepInterval), sizeof(keepInterval)) != 0) {
            return false;
        }
        if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT,
            reinterpret_cast<const char*>(&keepCount), sizeof(keepCount)) != 0) {
            return false;
        }
        return true;
#endif
    }

    // 设置广播
    static bool setBroadcast(SOCKET socket, bool broadcast = true) {
        int optval = broadcast ? 1 : 0;
        return setsockopt(socket, SOL_SOCKET, SO_BROADCAST,
            reinterpret_cast<const char*>(&optval), sizeof(optval)) == 0;
    }

    // 设置 linger
    static bool setLinger(SOCKET socket, bool on, int timeoutSec) {
        struct linger l;
        l.l_onoff = on ? 1 : 0;
        l.l_linger = timeoutSec;
        return setsockopt(socket, SOL_SOCKET, SO_LINGER,
            reinterpret_cast<const char*>(&l), sizeof(l)) == 0;
    }

    // 获取 socket 错误
    static int getSocketError(SOCKET socket) {
        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(socket, SOL_SOCKET, SO_ERROR,
            reinterpret_cast<char*>(&error), &len);
        return error;
    }
};

//==============================================================================
// DNS 解析
//==============================================================================

class DnsResolver {
public:
    struct Result {
        std::string name;
        std::vector<IPv4Address> addresses;
    };

    // 解析域名
    static bool resolve(const std::string& hostname, std::vector<IPv4Address>& addresses) {
        struct addrinfo hints, *result;

        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        int ret = getaddrinfo(hostname.c_str(), nullptr, &hints, &result);
        if (ret != 0) {
            return false;
        }

        for (struct addrinfo* p = result; p != nullptr; p = p->ai_next) {
            if (p->ai_family == AF_INET) {
                struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(p->ai_addr);
                addresses.emplace_back(ntohl(addr->sin_addr.s_addr), 0);
            }
        }

        freeaddrinfo(result);
        return !addresses.empty();
    }

    // 解析单个地址
    static IPv4Address resolveOne(const std::string& hostname, uint16_t port = 0) {
        std::vector<IPv4Address> addresses;
        if (resolve(hostname, addresses)) {
            IPv4Address addr = addresses[0];
            addr.setPort(port);
            return addr;
        }
        return IPv4Address();
    }

    // 反向 DNS 查询
    static std::string reverseLookup(uint32_t ip) {
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(ip);

        char host[NI_MAXHOST];
        int ret = getnameinfo(reinterpret_cast<struct sockaddr*>(&addr),
            sizeof(addr), host, sizeof(host), nullptr, 0, NI_NAMEREQD);

        return (ret == 0) ? std::string(host) : "";
    }
};

//==============================================================================
// URL 解析
//==============================================================================

class Url {
public:
    Url() = default;

    Url(const std::string& url) { parse(url); }

    bool parse(const std::string& url) {
        // 格式: [protocol://][host][:port][/path][?query]
        size_t pos = 0;

        // 解析协议
        size_t protoEnd = url.find("://");
        if (protoEnd != std::string::npos) {
            protocol_ = url.substr(0, protoEnd);
            pos = protoEnd + 3;
        }

        // 解析 host 和 port
        size_t pathStart = url.find('/', pos);
        size_t queryStart = url.find('?', pos);

        size_t hostEnd = pathStart;
        if (queryStart != std::string::npos && (hostEnd == std::string::npos || queryStart < hostEnd)) {
            hostEnd = queryStart;
        }
        if (hostEnd == std::string::npos) {
            hostEnd = url.length();
        }

        std::string hostPort = url.substr(pos, hostEnd - pos);

        // 分离端口
        size_t colonPos = hostPort.find(':');
        if (colonPos != std::string::npos) {
            host_ = hostPort.substr(0, colonPos);
            port_ = static_cast<uint16_t>(std::stoi(hostPort.substr(colonPos + 1)));
        } else {
            host_ = hostPort;
        }

        // 解析路径
        if (pathStart != std::string::npos) {
            size_t pathEnd = url.find('?', pathStart);
            if (pathEnd != std::string::npos) {
                path_ = url.substr(pathStart, pathEnd - pathStart);
                query_ = url.substr(pathEnd + 1);
            } else {
                path_ = url.substr(pathStart);
            }
        }

        return isValid();
    }

    bool isValid() const { return !host_.empty(); }

    const std::string& protocol() const { return protocol_; }
    const std::string& host() const { return host_; }
    uint16_t port() const { return port_; }
    const std::string& path() const { return path_; }
    const std::string& query() const { return query_; }

    std::string toString() const {
        std::string result;
        if (!protocol_.empty()) {
            result += protocol_ + "://";
        }
        result += host_;
        if (port_ > 0) {
            result += ":" + std::to_string(port_);
        }
        if (!path_.empty()) {
            result += path_;
        }
        if (!query_.empty()) {
            result += "?" + query_;
        }
        return result;
    }

    // 获取默认端口
    uint16_t getDefaultPort() const {
        if (protocol_ == "http") return 80;
        if (protocol_ == "https") return 443;
        if (protocol_ == "ftp") return 21;
        if (protocol_ == "ssh") return 22;
        if (protocol_ == "redis") return 6379;
        if (protocol_ == "mysql") return 3306;
        if (protocol_ == "postgres") return 5432;
        return 0;
    }

private:
    std::string protocol_;
    std::string host_;
    uint16_t port_ = 0;
    std::string path_;
    std::string query_;
};

//==============================================================================
// 网络统计
//==============================================================================

struct NetworkStats {
    uint64_t bytesSent = 0;
    uint64_t bytesReceived = 0;
    uint64_t packetsSent = 0;
    uint64_t packetsReceived = 0;
    uint64_t connectionsAccepted = 0;
    uint64_t connectionsClosed = 0;
    uint64_t connectionErrors = 0;
    uint64_t sendErrors = 0;
    uint64_t recvErrors = 0;

    void reset() {
        std::memset(this, 0, sizeof(*this));
    }

    void addSent(size_t bytes) {
        bytesSent += bytes;
        ++packetsSent;
    }

    void addReceived(size_t bytes) {
        bytesReceived += bytes;
        ++packetsReceived;
    }

    void print() const {
        std::cout << "Network Statistics:" << std::endl;
        std::cout << "  Sent: " << bytesSent << " bytes, " << packetsSent << " packets" << std::endl;
        std::cout << "  Received: " << bytesReceived << " bytes, " << packetsReceived << " packets" << std::endl;
        std::cout << "  Connections: " << connectionsAccepted << " accepted, "
                  << connectionsClosed << " closed, "
                  << connectionErrors << " errors" << std::endl;
        std::cout << "  Errors: " << sendErrors << " send, " << recvErrors << " recv" << std::endl;
    }
};

//==============================================================================
// 网络接口信息
//==============================================================================

struct NetworkInterface {
    std::string name;
    std::string description;
    IPv4Address ipAddress;
    IPv4Address netmask;
    IPv4Address broadcast;
    bool isUp;
    bool isLoopback;

    std::string toString() const {
        return name + ": " + ipAddress.ipToString() +
               " (mask: " + netmask.ipToString() + ")";
    }
};

//==============================================================================
// 辅助函数
//==============================================================================

// 检查端口是否可用
inline bool isPortAvailable(uint16_t port, const std::string& host = "0.0.0.0") {
    SOCKET testSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (testSocket == INVALID_SOCKET) {
        return false;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host.c_str());
    addr.sin_port = htons(port);

    bool available = bind(testSocket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0;

#ifdef _WIN32
    closesocket(testSocket);
#else
    close(testSocket);
#endif

    return available;
}

// 查找可用端口
inline uint16_t findAvailablePort(uint16_t startPort, uint16_t endPort,
                                  const std::string& host = "0.0.0.0") {
    for (uint16_t port = startPort; port <= endPort; ++port) {
        if (isPortAvailable(port, host)) {
            return port;
        }
    }
    return 0;
}

// 获取本机主机名
inline std::string getHostname() {
    char buffer[256];
    if (gethostname(buffer, sizeof(buffer)) == 0) {
        return std::string(buffer);
    }
    return "unknown";
}

// TCP 快速打开（实验性）
#ifdef TCP_FASTOPEN
inline bool enableFastOpen(SOCKET socket, int queueSize) {
    return setsockopt(socket, IPPROTO_TCP, TCP_FASTOPEN,
        reinterpret_cast<const char*>(&queueSize), sizeof(queueSize)) == 0;
}
#endif

} // namespace net
} // namespace apollo
