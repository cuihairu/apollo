/**
 * @file event_loop_demo.cpp
 * @brief 事件循环、心跳和重连机制示例
 */

#include "apollo/net/event_loop.h"
#include <iostream>
#include <thread>
#include <chrono>

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    typedef int socket_t;
    #define INVALID_SOCKET -1
#endif

using namespace apollo;
using namespace apollo::net;

//==============================================================================
// 网络初始化
//==============================================================================

struct NetworkInitializer {
    NetworkInitializer() {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }
    ~NetworkInitializer() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

//==============================================================================
// 示例 1: 基本事件循环
//==============================================================================

void basicEventLoopDemo() {
    std::cout << "\n========== 示例 1: 基本事件循环 ==========\n" << std::endl;

    EventLoop loop;
    if (!loop.init()) {
        std::cout << "Failed to initialize event loop" << std::endl;
        return;
    }

    // 添加一个定时器
    auto timerId = loop.addTimer(1000, []() {
        std::cout << "Timer fired!" << std::endl;
    });

    std::cout << "Starting event loop for 3 seconds..." << std::endl;

    // 在独立线程中运行事件循环
    std::thread loopThread([&]() {
        loop.run();
    });

    std::this_thread::sleep_for(std::chrono::seconds(3));

    loop.stop();
    loopThread.join();

    std::cout << "Event loop stopped" << std::endl;
}

//==============================================================================
// 示例 2: 周期性定时器
//==============================================================================

void periodicTimerDemo() {
    std::cout << "\n========== 示例 2: 周期性定时器 ==========\n" << std::endl;

    EventLoop loop;
    loop.init();

    int counter = 0;
    auto timerId = loop.addPeriodicTimer(500, [&counter]() {
        counter++;
        std::cout << "Periodic timer tick: " << counter << std::endl;
    });

    std::thread loopThread([&]() {
        loop.run();
    });

    std::this_thread::sleep_for(std::chrono::seconds(3));

    loop.stop();
    loopThread.join();

    std::cout << "Timer fired " << counter << " times" << std::endl;
}

//==============================================================================
// 示例 3: 在事件循环中执行任务
//==============================================================================

void executeInLoopDemo() {
    std::cout << "\n========== 示例 3: 在事件循环中执行任务 ==========\n" << std::endl;

    EventLoop loop;
    loop.init();

    std::thread loopThread([&]() {
        loop.run();
    });

    // 从其他线程提交任务
    for (int i = 0; i < 5; ++i) {
        loop.executeInLoopAsync([i]() {
            std::cout << "Task " << i << " executed in event loop thread" << std::endl;
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    loop.stop();
    loopThread.join();
}

//==============================================================================
// 示例 4: 心跳管理器
//==============================================================================

void heartbeatManagerDemo() {
    std::cout << "\n========== 示例 4: 心跳管理器 ==========\n" << std::endl;

    EventLoop loop;
    loop.init();

    HeartbeatManager heartbeat(&loop);

    // 创建一个模拟 socket
#ifdef _WIN32
    socket_t testSock = 1;  // Windows 使用句柄
#else
    socket_t testSock = 10;
#endif

    // 添加心跳检测 (1秒间隔)
    heartbeat.addHeartbeat(testSock, 1000,
        [](socket_t sockfd) {
            std::cout << "Sending heartbeat to socket " << sockfd << std::endl;
        },
        [](socket_t sockfd) {
            std::cout << "Socket " << sockfd << " timeout!" << std::endl;
        }
    );

    std::thread loopThread([&]() {
        loop.run();
    });

    // 模拟接收数据，更新心跳
    for (int i = 0; i < 5; ++i) {
        heartbeat.updateHeartbeat(testSock);
        std::cout << "Received data, updated heartbeat" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }

    std::cout << "Stopping heartbeat updates..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(4));

    loop.stop();
    loopThread.join();
}

//==============================================================================
// 示例 5: 重连管理器
//==============================================================================

void reconnectManagerDemo() {
    std::cout << "\n========== 示例 5: 重连管理器 ==========\n" << std::endl;

    EventLoop loop;
    loop.init();

    ReconnectManager reconnect(&loop);

    int attempts = 0;
    bool connected = false;

    // 添加重连任务 (1秒间隔，最多5次)
    auto reconnectId = reconnect.addReconnect("127.0.0.1", 8080, 1000, 5,
        [&attempts, &connected]() -> bool {
            attempts++;
            std::cout << "Reconnection attempt " << attempts << std::endl;

            // 模拟第3次成功
            if (attempts >= 3) {
                connected = true;
                std::cout << "Reconnection successful!" << std::endl;
                return true;
            }

            return false;
        }
    );

    std::thread loopThread([&]() {
        loop.run();
    });

    // 等待重连完成
    while (!connected && attempts < 5) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    loop.stop();
    loopThread.join();

    std::cout << "Final attempts: " << attempts << ", Connected: " << (connected ? "Yes" : "No") << std::endl;
}

//==============================================================================
// 示例 6: 事件循环统计
//==============================================================================

void eventLoopStatsDemo() {
    std::cout << "\n========== 示例 6: 事件循环统计 ==========\n" << std::endl;

    EventLoop loop;
    EventLoopConfig config;
    config.enableStats = true;
    loop.init(config);

    // 添加多个定时器
    loop.addPeriodicTimer(100, []() {});
    loop.addPeriodicTimer(250, []() {});
    loop.addPeriodicTimer(500, []() {});

    std::thread loopThread([&]() {
        loop.run();
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));

    auto stats = loop.getStats();
    std::cout << "Loop count: " << stats.loopCount << std::endl;
    std::cout << "Events processed: " << stats.eventsProcessed << std::endl;
    std::cout << "Timers fired: " << stats.timersFired << std::endl;
    std::cout << "Wakeups: " << stats.wakeups << std::endl;
    std::cout << "Current connections: " << stats.currentConnections << std::endl;

    loop.stop();
    loopThread.join();
}

//==============================================================================
// 示例 7: Socket 事件监听 (模拟)
//==============================================================================

void socketEventDemo() {
    std::cout << "\n========== 示例 7: Socket 事件监听 ==========\n" << std::endl;

    EventLoop loop;
    loop.init();

    // 创建一个简单的 UDP socket 用于演示
#ifdef _WIN32
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#else
    socket_t sock = socket(AF_INET, SOCK_DGRAM, 0);
#endif

    if (sock == INVALID_SOCKET) {
        std::cout << "Failed to create socket" << std::endl;
        return;
    }

    // 绑定到端口
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(0);  // 任意端口

#ifdef _WIN32
    if (bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
#else
    if (bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
#endif
        std::cout << "Failed to bind socket" << std::endl;
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return;
    }

    // 添加 socket 监听
    loop.addSocket(sock, EventType::READ, [](socket_t sockfd, EventType events) {
        if (HasEvent(events, EventType::READ)) {
            std::cout << "Socket " << sockfd << " is readable" << std::endl;
            // 这里可以读取数据
        }
    });

    std::cout << "Socket event monitoring started..." << std::endl;

    std::thread loopThread([&]() {
        // 运行 1 秒后停止
        std::this_thread::sleep_for(std::chrono::seconds(1));
        loop.stop();
    });

    loop.run();
    loopThread.join();

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif

    std::cout << "Socket event monitoring stopped" << std::endl;
}

//==============================================================================
// 主函数
//==============================================================================

int main() {
    std::cout << "==============================================" << std::endl;
    std::cout << "     Apollo 事件循环示例" << std::endl;
    std::cout << "==============================================" << std::endl;

    // 初始化网络
    static NetworkInitializer netInit;

    try {
        basicEventLoopDemo();
        periodicTimerDemo();
        executeInLoopDemo();
        heartbeatManagerDemo();
        reconnectManagerDemo();
        eventLoopStatsDemo();
        socketEventDemo();

        std::cout << "\n==============================================" << std::endl;
        std::cout << "所有示例执行完毕!" << std::endl;
        std::cout << "==============================================" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
