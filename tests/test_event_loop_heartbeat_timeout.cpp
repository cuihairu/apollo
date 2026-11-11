// EventLoop HeartbeatManager/ReconnectManager 回归（audit P0#3 修复 2026-11-12）。
// ①onCheckTimer 原实现边遍历 heartbeats_ 边 erase(sockfd) → 迭代器失效 UB；
//   修复后超时 fd 收集出循环后删除。②HeartbeatManager/ReconnectManager 原无
//   析构，周期/重连定时器回调捕获 this，manager 先死则回调打悬空 this → UAF；
//   修复后析构移除定时器。
#include "apollo/net/event_loop.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
#else
    #include <unistd.h>
#endif

namespace {

using apollo::net::EventLoop;
using apollo::net::HeartbeatManager;
using apollo::net::ReconnectManager;
// socket_t 为 event_loop.h 的全局 typedef（非 apollo::net 成员）

bool make_read_pair(socket_t& readEnd, socket_t& writeEnd) {
#ifdef _WIN32
    SOCKET a = INVALID_SOCKET, b = INVALID_SOCKET;
    SOCKADDR_IN addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    int len = static_cast<int>(sizeof addr);
    a = ::socket(AF_INET, SOCK_STREAM, 0);
    if (a == INVALID_SOCKET) return false;
    if (::bind(a, reinterpret_cast<SOCKADDR*>(&addr), sizeof addr) != 0) { ::closesocket(a); return false; }
    if (::getsockname(a, reinterpret_cast<SOCKADDR*>(&addr), &len) != 0) { ::closesocket(a); return false; }
    if (::listen(a, 1) != 0) { ::closesocket(a); return false; }
    b = ::socket(AF_INET, SOCK_STREAM, 0);
    if (b == INVALID_SOCKET) { ::closesocket(a); return false; }
    if (::connect(b, reinterpret_cast<SOCKADDR*>(&addr), sizeof addr) != 0) { ::closesocket(a); ::closesocket(b); return false; }
    SOCKET c = ::accept(a, nullptr, nullptr);
    ::closesocket(a);
    if (c == INVALID_SOCKET) { ::closesocket(b); return false; }
    readEnd = static_cast<socket_t>(c);
    writeEnd = static_cast<socket_t>(b);
    return true;
#else
    int fd[2] = {-1, -1};
    if (::pipe(fd) != 0) return false;
    readEnd = static_cast<socket_t>(fd[0]);
    writeEnd = static_cast<socket_t>(fd[1]);
    return true;
#endif
}

void close_pair(socket_t readEnd, socket_t writeEnd) {
#ifdef _WIN32
    ::closesocket(readEnd);
    ::closesocket(writeEnd);
#else
    ::close(readEnd);
    ::close(writeEnd);
#endif
}

#define TEST_ASSERT(cond, msg) \
    do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; return false; } } while (0)

// 多心跳同轮超时：原实现遍历中 erase 迭代器失效（跳过/崩溃），修复后全部
// 超时回调各触发一次且条目清空。
bool test_multiple_timeout_erase_in_loop() {
    EventLoop loop;
    TEST_ASSERT(loop.init(), "loop init");
    TEST_ASSERT(loop.runInThread(), "loop run in thread");

    HeartbeatManager hb(&loop);
    std::atomic<int> timeouts{0};
    socket_t readEnds[3], writeEnds[3];
    for (int i = 0; i < 3; ++i) {
        TEST_ASSERT(make_read_pair(readEnds[i], writeEnds[i]), "read pair");
        // interval 1ms：心跳无回调，仅超时（≥3ms）计数
        hb.addHeartbeat(readEnds[i], 1, [](socket_t) {},
                        [&](socket_t) { timeouts.fetch_add(1); });
    }

    // 等待 elapsed ≥ 3ms（3 倍 interval），再同步触发检查
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    hb.checkTimeouts();
    loop.stop();
    TEST_ASSERT(timeouts.load() == 3, "all 3 heartbeat timeouts fired exactly once (erase-in-loop would skip/UB)");

    for (int i = 0; i < 3; ++i) close_pair(readEnds[i], writeEnds[i]);
    return true;
}

// 构造/析构管理器后 EventLoop 仍存活可用：析构须移除捕获 this 的定时器，
// 否则残留定时器回调打悬空 this（ASan 下必现，普通构建下累积悬空）。
bool test_destructor_removes_timers() {
    EventLoop loop;
    TEST_ASSERT(loop.init(), "loop init");
    TEST_ASSERT(loop.runInThread(), "loop run in thread");

    socket_t readEnd, writeEnd;
    TEST_ASSERT(make_read_pair(readEnd, writeEnd), "read pair");

    {
        HeartbeatManager hb(&loop);
        hb.addHeartbeat(readEnd, 50, [](socket_t) {}, [](socket_t) {});
    }  // 析构：移除周期检查定时器
    {
        ReconnectManager rc(&loop);
        rc.addReconnect("127.0.0.1", 1, 10, 3, []() { return false; });
    }  // 析构：移除重连定时器

    // 泵过心跳检查周期（1000ms），残留定时器若未清则触发悬空回调
    std::atomic<int> probe{0};
    (void)loop.addTimer(10, [&]() { probe.fetch_add(1); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (probe.load() < 1 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    loop.stop();
    close_pair(readEnd, writeEnd);
    TEST_ASSERT(probe.load() >= 1, "loop alive after manager destruction (dangling timer would break it)");
    return true;
}

} // namespace

int main() {
    int passed = 0, total = 0;
    auto run = [&](const char* name, bool (*fn)()) {
        std::cout << "== " << name << " ==" << std::endl;
        ++total;
        if (fn()) { ++passed; std::cout << "  PASSED" << std::endl; }
    };
    run("test_multiple_timeout_erase_in_loop", test_multiple_timeout_erase_in_loop);
    run("test_destructor_removes_timers", test_destructor_removes_timers);
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    return (passed == total) ? 0 : 1;
}
