#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <functional>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socket_t = SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    using socket_t = int;
    const int INVALID_SOCKET = -1;
    const int SOCKET_ERROR = -1;
    const int SD_SEND = SHUT_WR;
    const int SD_RECEIVE = SHUT_RD;
    const int SD_BOTH = SHUT_RDWR;
    #define closesocket close
#endif

namespace apollo::net {

/// 网络错误码
enum class NetError {
    SUCCESS = 0,
    INVALID_PARAM,
    SOCKET_CREATE_FAILED,
    SOCKET_BIND_FAILED,
    SOCKET_LISTEN_FAILED,
    SOCKET_ACCEPT_FAILED,
    SOCKET_CONNECT_FAILED,
    SOCKET_SEND_FAILED,
    SOCKET_RECV_FAILED,
    SOCKET_CLOSED,
    WOULDBLOCK,
    TIMEOUT,
    UNKNOWN
};

/// 网络地址
class NetAddress {
public:
    NetAddress() = default;
    NetAddress(const std::string& ip, uint16_t port);
    NetAddress(const sockaddr_in& addr);

    std::string GetIP() const;
    uint16_t GetPort() const;
    const sockaddr_in& GetSockAddr() const { return addr_; }
    sockaddr_in& GetSockAddr() { return addr_; }

    std::string ToString() const;

private:
    sockaddr_in addr_{};
};

/// 网络事件类型
enum class NetEventType {
    READABLE = 0x01,   // 可读事件
    WRITABLE = 0x02,   // 可写事件
    ERROR    = 0x04,   // 错误事件
    CLOSE    = 0x08    // 关闭事件
};

/// 网络事件处理器接口
class INetEventHandler {
public:
    virtual ~INetEventHandler() = default;

    /// 连接建立
    virtual void OnConnected(socket_t sockfd, const NetAddress& addr) {}

    /// 连接断开
    virtual void OnDisconnected(socket_t sockfd) {}

    /// 数据可读
    virtual void OnReadable(socket_t sockfd) {}

    /// 数据可写
    virtual void OnWritable(socket_t sockfd) {}

    /// 错误发生
    virtual void OnError(socket_t sockfd, NetError error) {}
};

/// 网络统计信息
struct NetStats {
    uint64_t bytesSent = 0;
    uint64_t bytesReceived = 0;
    uint64_t packetsSent = 0;
    uint64_t packetsReceived = 0;
    uint64_t connectionsAccepted = 0;
    uint64_t connectionsActive = 0;
    uint64_t connectionsTotal = 0;
    uint64_t errors = 0;
};

}  // namespace apollo::net