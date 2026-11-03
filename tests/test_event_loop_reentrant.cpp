// CR-06 回归：event_loop 持锁回调导致回调重入本组件（addTimer/
// registerHeartbeat/removeReconnect）自锁死锁。修复后回调在出锁后触发，
// 以下用例在原实现下会死锁（本测试带超时护栏，超时即失败）。
#include "apollo/net/event_loop.h"
#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>

#define TEST_ASSERT(cond, msg) \
    do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; return false; } } while (0)

namespace {

bool test_timer_callback_reentrant() {
    apollo::net::EventLoop loop;
    TEST_ASSERT(loop.init(), "loop init");
    TEST_ASSERT(loop.runInThread(), "loop run in thread");

    std::atomic<int> fires{0};
    // 一次性定时器回调内重入 addTimer/removeTimer（原实现持 timerMutex_ 调
    // fire()，重入 addTimer 即自锁死锁）。
    const auto self = loop.addTimer(10, [&]() {
        (void)loop.addTimer(10, [&]() { fires.fetch_add(1); });
        fires.fetch_add(1);
    });
    (void)self;

    // HeartbeatManager 内部检查定时器固定 1000ms，两次心跳需约 2s；放宽到 3s
    // 避免与 2s 护栏竞争（原实现下死锁仍会在护栏内超时失败）。
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (fires.load() < 2 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    loop.stop();
    TEST_ASSERT(fires.load() >= 2, "timer callback reentrant fired (would deadlock before fix)");
    return true;
}

bool test_heartbeat_callback_reentrant() {
    apollo::net::EventLoop loop;
    TEST_ASSERT(loop.init(), "loop init");
    TEST_ASSERT(loop.runInThread(), "loop run in thread");

    apollo::net::HeartbeatManager hb(&loop);
    std::atomic<int> beats{0};
    int pipefd[2] = {-1, -1};
    TEST_ASSERT(::pipe(pipefd) == 0, "pipe");
    const socket_t sock = static_cast<socket_t>(pipefd[0]);
    // 回调内重入 updateHeartbeat/addHeartbeat（原实现持 mutex_ 调 callback，
    // 重入 addHeartbeat 即自锁死锁）。
    hb.addHeartbeat(sock, 500, [&](socket_t) {
        beats.fetch_add(1);
        hb.updateHeartbeat(sock);
        hb.addHeartbeat(sock, 500, [](socket_t) {},
                         [](socket_t) {});
    }, [](socket_t) {});

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (beats.load() < 1 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    loop.stop();
    ::close(pipefd[0]); ::close(pipefd[1]);
    TEST_ASSERT(beats.load() >= 1, "heartbeat callback reentrant fired (would deadlock before fix)");
    return true;
}

bool test_reconnect_callback_reentrant() {
    apollo::net::EventLoop loop;
    TEST_ASSERT(loop.init(), "loop init");
    TEST_ASSERT(loop.runInThread(), "loop run in thread");

    apollo::net::ReconnectManager rc(&loop);
    std::atomic<int> attempts{0};
    // 回调内重入 removeReconnect（原实现持 mutex_ 调 callback，重入
    // removeReconnect 即自锁死锁）。
    static uint64_t reid = 0;
    reid = rc.addReconnect("127.0.0.1", 1, 10, 3, [&]() {
        attempts.fetch_add(1);
        rc.removeReconnect(reid);
        return false;
    });

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (attempts.load() < 1 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    loop.stop();
    TEST_ASSERT(attempts.load() >= 1, "reconnect callback reentrant fired (would deadlock before fix)");
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
    run("test_timer_callback_reentrant", test_timer_callback_reentrant);
    run("test_heartbeat_callback_reentrant", test_heartbeat_callback_reentrant);
    run("test_reconnect_callback_reentrant", test_reconnect_callback_reentrant);
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    return (passed == total) ? 0 : 1;
}
