/**
 * @file netutil_demo.cpp
 * @brief 网络工具使用示例
 */

#include <iostream>
#include "apollo/net/net_util.h"

#ifdef _WIN32
    #pragma comment(lib, "ws2_32.lib")
#endif

using namespace apollo::net;

//==============================================================================
// 示例1：IPv4 地址操作
//==============================================================================

void example1_IPAddress() {
    std::cout << "\n=== Example 1: IPv4 Address ===" << std::endl;

    // 创建地址
    IPv4Address addr1("192.168.1.100", 8080);
    std::cout << "Address: " << addr1.toString() << std::endl;
    std::cout << "IP: " << addr1.ipToString() << ", Port: " << addr1.port() << std::endl;
    std::cout << "Is localhost: " << (addr1.isLocalhost() ? "yes" : "no") << std::endl;
    std::cout << "Is multicast: " << (addr1.isMulticast() ? "yes" : "no") << std::endl;

    // 从字符串解析
    IPv4Address addr2 = IPv4Address::parse("127.0.0.1:3306");
    std::cout << "Parsed: " << addr2.toString() << std::endl;

    // 预定义地址
    std::cout << "Any address: " << IPv4Address::any().toString() << std::endl;
    std::cout << "Loopback: " << IPv4Address::loopback().toString() << std::endl;

    // 转换为 sockaddr_in
    auto sockaddr = addr1.toSockAddr();
    std::cout << "sockaddr_in port: " << ntohs(sockaddr.sin_port) << std::endl;
}

//==============================================================================
// 示例2：端口范围
//==============================================================================

void example2_PortRange() {
    std::cout << "\n=== Example 2: Port Range ===" << std::endl;

    std::cout << "Well known ports: " << PortRange::wellKnown().min() << "-"
              << PortRange::wellKnown().max() << std::endl;

    std::cout << "Registered ports: " << PortRange::registered().min() << "-"
              << PortRange::registered().max() << std::endl;

    std::cout << "Dynamic ports: " << PortRange::dynamic().min() << "-"
              << PortRange::dynamic().max() << std::endl;

    std::cout << "Is 80 system port: " << (PortRange::isSystemPort(80) ? "yes" : "no") << std::endl;
    std::cout << "Is 8080 system port: " << (PortRange::isSystemPort(8080) ? "yes" : "no") << std::endl;
}

//==============================================================================
// 示例3：URL 解析
//==============================================================================

void example3_UrlParser() {
    std::cout << "\n=== Example 3: URL Parser ===" << std::endl;

    std::vector<std::string> urls = {
        "http://example.com:8080/path/to/resource",
        "https://api.example.com/v1/users?id=123",
        "redis://localhost:6379/0",
        "mysql://db.example.com:3306/mydb",
        "192.168.1.100:8080"
    };

    for (const auto& urlStr : urls) {
        Url url(urlStr);
        std::cout << "\nURL: " << urlStr << std::endl;
        std::cout << "  Protocol: " << url.protocol() << std::endl;
        std::cout << "  Host: " << url.host() << std::endl;
        std::cout << "  Port: " << url.port() << " (default: " << url.getDefaultPort() << ")" << std::endl;
        std::cout << "  Path: " << url.path() << std::endl;
        std::cout << "  Query: " << url.query() << std::endl;
        std::cout << "  Valid: " << (url.isValid() ? "yes" : "no") << std::endl;
    }
}

//==============================================================================
// 示例4：DNS 解析
//==============================================================================

void example4_DnsResolver() {
    std::cout << "\n=== Example 4: DNS Resolver ===" << std::endl;

    std::vector<std::string> hostnames = {
        "localhost",
        "google.com",
        "baidu.com"
    };

    for (const auto& hostname : hostnames) {
        std::vector<IPv4Address> addresses;
        std::cout << "\nResolving: " << hostname << std::endl;

        if (DnsResolver::resolve(hostname, addresses)) {
            std::cout << "  Found " << addresses.size() << " addresses:" << std::endl;
            for (const auto& addr : addresses) {
                std::cout << "    " << addr.ipToString() << std::endl;

                // 反向查询
                std::string reverse = DnsResolver::reverseLookup(addr.ipHost());
                if (!reverse.empty()) {
                    std::cout << "      -> " << reverse << std::endl;
                }
            }
        } else {
            std::cout << "  Failed to resolve" << std::endl;
        }
    }
}

//==============================================================================
// 示例5：端口检查
//==============================================================================

void example5_PortCheck() {
    std::cout << "\n=== Example 5: Port Check ===" << std::endl;

    std::vector<uint16_t> ports = {80, 443, 8080, 3306, 6379, 5000, 5001};

    for (uint16_t port : ports) {
        bool available = isPortAvailable(port);
        std::cout << "Port " << port << ": "
                  << (available ? "available" : "in use") << std::endl;
    }

    // 查找可用端口
    uint16_t availablePort = findAvailablePort(5000, 5100);
    if (availablePort > 0) {
        std::cout << "\nFound available port: " << availablePort << std::endl;
    }
}

//==============================================================================
// 示例6：网络统计
//==============================================================================

void example6_NetworkStats() {
    std::cout << "\n=== Example 6: Network Stats ===" << std::endl;

    NetworkStats stats;

    // 模拟一些活动
    stats.addSent(1024);
    stats.addSent(2048);
    stats.addReceived(512);
    stats.addReceived(1024);
    stats.addReceived(2048);
    stats.connectionsAccepted++;

    stats.print();
}

//==============================================================================
// 示例7：主机名
//==============================================================================

void example7_Hostname() {
    std::cout << "\n=== Example 7: Hostname ===" << std::endl;

    std::string hostname = getHostname();
    std::cout << "Hostname: " << hostname << std::endl;

    // 解析本机 IP
    std::vector<IPv4Address> localAddrs = IPv4Address::getLocalAddresses();
    std::cout << "Local addresses:" << std::endl;
    for (const auto& addr : localAddrs) {
        std::cout << "  " << addr.ipToString() << std::endl;
    }
}

//==============================================================================
// 主程序
//==============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "=== Apollo Network Utility Demo ===" << std::endl;
    std::cout << "========================================" << std::endl;

    example1_IPAddress();
    example2_PortRange();
    example3_UrlParser();
    example4_DnsResolver();
    example5_PortCheck();
    example6_NetworkStats();
    example7_Hostname();

    std::cout << "\n========================================" << std::endl;
    std::cout << "=== All Examples Complete ===" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
