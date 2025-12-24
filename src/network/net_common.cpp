#include "apollo/network/transport/net_common.hpp"
#include <sstream>
#include <cstring>

#ifdef _WIN32
    static struct WinSockInitializer {
        WinSockInitializer() {
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
        }
        ~WinSockInitializer() {
            WSACleanup();
        }
    } g_wsInit;
#endif

namespace apollo::net {

NetAddress::NetAddress(const std::string& ip, uint16_t port) {
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    if (ip.empty() || ip == "0.0.0.0") {
        addr_.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr);
    }
}

NetAddress::NetAddress(const sockaddr_in& addr) : addr_(addr) {
}

std::string NetAddress::GetIP() const {
    char ip[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &addr_.sin_addr, ip, sizeof(ip));
    return std::string(ip);
}

uint16_t NetAddress::GetPort() const {
    return ntohs(addr_.sin_port);
}

std::string NetAddress::ToString() const {
    return GetIP() + ":" + std::to_string(GetPort());
}

}  // namespace apollo::net