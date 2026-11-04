#include "apollo/net/session_manager.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <cstdlib>

namespace {

class NullConnection : public apollo::net::IConnection {
public:
    int getFd() const override { return -1; }
    bool isConnected() const override { return false; }
    bool send(const void*, size_t) override { return false; }
    int receive(void*, size_t) override { return 0; }
    void close() override {}
    std::string getRemoteAddress() const override { return {}; }
    uint16_t getRemotePort() const override { return 0; }
};

class RecordingConnection : public NullConnection {
public:
    bool isConnected() const override { return true; }
    bool send(const void*, size_t) override {
        if (block) {
            // 阻在 send 内，稳定制造 broadcast 锁外窗口。
            enteredSend.store(true);
            while (!release) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        return true;
    }
    bool block = false;
    std::atomic<bool> enteredSend{false};
    std::atomic<bool> release{false};
};

// 持锁回调重入回归（CR-09 复核）：onDisconnected 持 mutex_ 内调
// getSession 二次上锁非递归 mutex 自锁死锁。修复后直接查 sessions_ 不再
// 二次上锁。带护栏，原实现下超时失败（exit 2）。
bool test_onDisconnected_no_double_lock() {
    auto& manager = apollo::net::SessionManager::instance();
    manager.initialize(1);
    auto session = manager.createSession(std::make_unique<NullConnection>());
    if (session == nullptr) {
        std::cerr << "onDisconnected: createSession failed\n";
        return false;
    }

    std::atomic<bool> returned{false};
    std::thread watcher([&] {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!returned.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (!returned.load()) {
            std::cerr << "onDisconnected self-deadlocked\n";
            std::exit(2);
        }
    });
    manager.onDisconnected(session->getSessionId(), 0);
    returned.store(true);
    watcher.join();

    const bool ok = manager.getSessionCount() == 1;  // 映射未清，仅移玩家映射
    manager.shutdown();
    return ok;
}

// broadcast 锁外 send 期间并发 closeSession（CR-09 复核）：sessions_ 存
// shared_ptr，close 擦除映射后广播持有的本地副本仍保活 Session——不悬垂。
// 修复前裸 unique_ptr 被擦除即析构，锁外 send 用后即悬垂（UB/ASan 捕获）。
bool test_broadcast_keeps_alive_during_close() {
    auto& manager = apollo::net::SessionManager::instance();
    manager.initialize(1);
    auto session = manager.createSession(std::make_unique<RecordingConnection>());
    if (session == nullptr) {
        std::cerr << "broadcast: createSession failed\n";
        return false;
    }

    auto* blocking = static_cast<RecordingConnection*>(nullptr);
    // createSession 接管 connection；Session::getConnection() 返回裸指针，
    // 仅测试内用于阻塞控制。createSession 返回 ISession*，向下转型取观测面。
    if (auto* s = dynamic_cast<apollo::net::Session*>(session)) {
        blocking = static_cast<RecordingConnection*>(s->getConnection());
    }
    if (blocking == nullptr) {
        manager.shutdown();
        return false;
    }
    blocking->block = true;
    std::thread closer([&] {
        // 等 broadcast 进入锁外 send（持 shared_ptr 副本）后再关会话，确保
        // 复现「锁外 send 期间并发 closeSession」窗口。
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!blocking->enteredSend.load() &&
               std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (blocking->enteredSend.load()) {
            manager.closeSession(session->getSessionId(), 0);
        }
        blocking->release.store(true);
    });
    // 广播拷贝 shared_ptr 后出锁，逐个 send；close 与 send 并发无害。
    (void)manager.broadcast(nullptr, 0);
    closer.join();

    // close 后映射已擦除（count 0/1 均可——send 与 close 竞争结果无关），
    // 关键是未崩溃（ASan/TSan 下悬垂即失败）。
    // 关键断言：并发 closeSession 未悬垂（ASan/TSan 下 UAF 即失败）；会话
    // 已擦除。sendCalls 不在此读——session 析构后 blocking 悬垂。
    const bool ok = manager.getSessionCount() == 0;
    manager.shutdown();
    return ok;
}

} // namespace

int main() {
    int passed = 0, total = 0;
    auto run = [&](const char* name, bool (*fn)()) {
        std::cout << "== " << name << " ==" << std::endl;
        ++total;
        if (fn()) { ++passed; std::cout << "  PASSED" << std::endl; }
    };
    run("getSessionByPlayerId_no_double_lock", []() {
        auto& manager = apollo::net::SessionManager::instance();
        manager.initialize(1);
        auto* session = manager.createSession(std::make_unique<RecordingConnection>());
        if (session == nullptr) { return false; }
        session->setPlayerId(77);
        manager.onAuthenticated(session->getSessionId(), 77);

        std::atomic<bool> returned{false};
        std::thread watcher([&] {
            const auto deadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (!returned.load() && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            if (!returned.load()) {
                std::cerr << "getSessionByPlayerId self-deadlocked\n";
                std::exit(2);
            }
        });
        auto* found = manager.getSessionByPlayerId(77);
        returned.store(true);
        watcher.join();

        bool ok = found == session;
        if (ok) {
            const auto broadcastCount = manager.broadcast(nullptr, 0);
            ok = broadcastCount == 1;
        }
        manager.shutdown();
        return ok;
    });
    run("onDisconnected_no_double_lock", test_onDisconnected_no_double_lock);
    run("broadcast_keeps_alive_during_close", test_broadcast_keeps_alive_during_close);
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    return (passed == total) ? 0 : 1;
}
