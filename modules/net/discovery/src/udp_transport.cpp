#include "apollo/net/discovery/udp_transport.hpp"

#include <cstring>
#include <vector>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <cerrno>
    #include <fcntl.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace apollo::net::discovery {

namespace {

// Winsock 生命周期（进程级一次；POSIX 下为空——与 net/tcp net_common.cpp 同惯例）
#ifdef _WIN32
struct WinSockInitializer {
    WinSockInitializer() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
    ~WinSockInitializer() {
        WSACleanup();
    }
};
const WinSockInitializer g_wsInit{};
#endif

using sock_t = std::intptr_t;   // 存储面句柄（POSIX fd / Windows SOCKET 统一收口）
#ifdef _WIN32
using sock_api_t = SOCKET;      // API 调用面原生类型
#else
using sock_api_t = int;
#endif

sock_t open_udp_socket() {
#ifdef _WIN32
    (void)g_wsInit;  // 静态初始化器已随链接生效（防优化裁剪的显式引用）
    SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    return (s == INVALID_SOCKET) ? -1 : static_cast<sock_t>(s);
#else
    int s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    return (s < 0) ? -1 : static_cast<sock_t>(s);
#endif
}

void sock_close(sock_t h) noexcept {
    if (h < 0) {
        return;
    }
#ifdef _WIN32
    closesocket(static_cast<SOCKET>(h));
#else
    ::close(static_cast<int>(h));
#endif
}

// 点分 IPv4 → 网络序地址（骨架期仅字面量；主机名解析归部署配置层）
bool parse_ipv4(const std::string& host, std::uint32_t& out_net_order) {
    in_addr addr{};
    if (inet_pton(AF_INET, host.c_str(), &addr) != 1) {
        return false;
    }
    out_net_order = addr.s_addr;
    return true;
}

bool sock_set_reuse(sock_t h) {
    int reuse = 1;
    return setsockopt(static_cast<sock_api_t>(h), SOL_SOCKET, SO_REUSEADDR,
                      reinterpret_cast<const char*>(&reuse), sizeof(reuse)) == 0;
}

bool sock_bind(sock_t h, std::uint16_t port, std::uint32_t net_addr) {
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = net_addr;
    return bind(static_cast<sock_api_t>(h), reinterpret_cast<const sockaddr*>(&local),
                sizeof(local)) == 0;
}

std::uint16_t sock_bound_port(sock_t h) {
    sockaddr_in bound{};
#ifdef _WIN32
    int len = sizeof(bound);
#else
    socklen_t len = sizeof(bound);
#endif
    if (getsockname(static_cast<sock_api_t>(h), reinterpret_cast<sockaddr*>(&bound),
                    &len) != 0) {
        return 0;
    }
    return ntohs(bound.sin_port);
}

void sock_set_nonblocking(sock_t h) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(static_cast<SOCKET>(h), FIONBIO, &mode);
#else
    int flags = fcntl(static_cast<int>(h), F_GETFL, 0);
    fcntl(static_cast<int>(h), F_SETFL, flags | O_NONBLOCK);
#endif
}

} // namespace

// ---- UdpBeaconTransport ----

UdpBeaconTransport::~UdpBeaconTransport() {
    close();
}

bool UdpBeaconTransport::open() {
    if (is_open()) {
        return true;  // 幂等
    }
    handle_ = open_udp_socket();
    return is_open();
}

void UdpBeaconTransport::close() noexcept {
    sock_close(handle_);
    handle_ = kInvalidHandle;
}

bool UdpBeaconTransport::send_to(const std::string& host, std::uint16_t port,
                                 const std::uint8_t (&buf)[kWireSize]) {
    return send_bytes(host, port, buf, kWireSize);
}

bool UdpBeaconTransport::send_bytes(const std::string& host, std::uint16_t port,
                                    const std::uint8_t* data, std::size_t len) {
    if (!is_open() || port == 0 || data == nullptr || len == 0 ||
        len > kMaxDatagramSize) {
        return false;
    }
    std::uint32_t net_addr = 0;
    if (!parse_ipv4(host, net_addr)) {
        return false;
    }
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(port);
    dst.sin_addr.s_addr = net_addr;
    const auto sent =
        sendto(static_cast<sock_api_t>(handle_), reinterpret_cast<const char*>(data),
               static_cast<int>(len), 0,
               reinterpret_cast<const sockaddr*>(&dst), sizeof(dst));
    return sent == static_cast<int>(len);
}

bool UdpBeaconTransport::enable_broadcast() {
    if (!is_open()) {
        return false;
    }
    int enable = 1;
    return setsockopt(static_cast<sock_api_t>(handle_), SOL_SOCKET, SO_BROADCAST,
                      reinterpret_cast<const char*>(&enable),
                      sizeof(enable)) == 0;
}

// ---- UdpFeed ----

UdpFeed::~UdpFeed() {
    close();
}

bool UdpFeed::open(std::uint16_t bind_port, const std::string& bind_host) {
    if (is_open()) {
        return true;  // 幂等
    }
    std::uint32_t net_addr = 0;
    if (!parse_ipv4(bind_host, net_addr)) {
        return false;
    }
    const sock_t s = open_udp_socket();
    if (s < 0 || !sock_set_reuse(s) || !sock_bind(s, bind_port, net_addr)) {
        sock_close(s);
        return false;
    }
    const std::uint16_t actual = sock_bound_port(s);
    if (actual == 0) {  // getsockname 失败——句柄不可用，回收
        sock_close(s);
        return false;
    }
    sock_set_nonblocking(s);
    handle_ = s;
    bound_port_ = actual;
    return true;
}

void UdpFeed::close() noexcept {
    sock_close(handle_);
    handle_ = kInvalidHandle;
    bound_port_ = 0;
}

bool UdpFeed::try_receive(std::uint8_t (&buf)[kWireSize]) {
    std::string host;
    std::uint16_t port = 0;
    return try_receive(buf, host, port);
}

bool UdpFeed::try_receive(std::uint8_t (&buf)[kWireSize], std::string& sender_host,
                          std::uint16_t& sender_port) {
    if (!is_open()) {
        return false;
    }
    // 多读 1 字节侦测超长报文（≠32B 一律丢——非本协议）
    std::uint8_t tmp[kWireSize + 1];
    sockaddr_in src{};
#ifdef _WIN32
    int src_len = sizeof(src);
#else
    socklen_t src_len = sizeof(src);
#endif
    const auto n = recvfrom(static_cast<sock_api_t>(handle_),
                            reinterpret_cast<char*>(tmp), static_cast<int>(sizeof(tmp)),
                            0, reinterpret_cast<sockaddr*>(&src), &src_len);
    if (n != static_cast<int>(kWireSize)) {
        return false;  // EWOULDBLOCK / 长度不符 同路静默
    }
    std::memcpy(buf, tmp, kWireSize);
    char ip[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &src.sin_addr, ip, sizeof(ip));
    sender_host = ip;
    sender_port = ntohs(src.sin_port);
    return true;
}

bool UdpFeed::try_receive(std::uint8_t* buf, std::size_t cap, std::size_t& len_out) {
    std::string host;
    std::uint16_t port = 0;
    return try_receive(buf, cap, len_out, host, port);
}

bool UdpFeed::try_receive(std::uint8_t* buf, std::size_t cap, std::size_t& len_out,
                          std::string& sender_host, std::uint16_t& sender_port) {
    len_out = 0;
    if (!is_open() || buf == nullptr || cap == 0) {
        return false;
    }
    // 多读 1 字节侦测超长报文（≥cap 一律按疑似截断丢弃——调用方缓冲须大于
    // 本方协议最大报文；超长静默丢不污染后续收包）
    std::vector<std::uint8_t> tmp(cap + 1);
    sockaddr_in src{};
#ifdef _WIN32
    int src_len = sizeof(src);
#else
    socklen_t src_len = sizeof(src);
#endif
    const auto n = recvfrom(static_cast<sock_api_t>(handle_),
                            reinterpret_cast<char*>(tmp.data()),
                            static_cast<int>(tmp.size()), 0,
                            reinterpret_cast<sockaddr*>(&src), &src_len);
    if (n <= 0 || static_cast<std::size_t>(n) > cap) {
        return false;  // EWOULDBLOCK / 疑似截断 同路静默
    }
    std::memcpy(buf, tmp.data(), static_cast<std::size_t>(n));
    len_out = static_cast<std::size_t>(n);
    char ip[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &src.sin_addr, ip, sizeof(ip));
    sender_host = ip;
    sender_port = ntohs(src.sin_port);
    return true;
}

} // namespace apollo::net::discovery
