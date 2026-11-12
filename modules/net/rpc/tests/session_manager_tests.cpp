// SessionManager 回归（audit P1#10 修复 2026-11-12）。
// ①internalCloseSession/update/shutdown 原在持 mutex_ 时经 Session::close /
//   Session::update 触发 listener_ 回调，监听器重入 manager 即自死锁
//   （std::mutex 非递归）；②SessionHelper::sendPacket 长度头与体分两次
//   send，第二次失败即整条流错位（后续所有包解析错乱）——合并单次发送、
//   失败断连。
// 注：SessionManager 为单例且 Session 在创建时快照监听器指针——用例体
// 放 lambda、监听器在 lambda 外存活、收尾统一 shutdown，杜绝断言早退
// 把悬空监听器漏给下一用例。
#include "apollo/net/session_manager.h"
#include <atomic>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

using apollo::net::IConnection;
using apollo::net::ISession;
using apollo::net::ISessionListener;
using apollo::net::SessionState;

#define TEST_ASSERT(cond, msg) \
    do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; return false; } } while (0)

// 假连接：记录每次 send 的字节数，可配置第 N 次起失败。closedFlag 经
// shared_ptr 持有——会话销毁（连接随之析构）后测试仍可安全断言关闭状态
class FakeConnection : public IConnection {
public:
    int getFd() const override { return -1; }
    bool isConnected() const override { return !closed_; }
    bool send(const void* /*data*/, size_t length) override {
        sendSizes.push_back(length);
        if (failFromSend_ >= 0 && static_cast<int>(sendSizes.size()) > failFromSend_) {
            return false;
        }
        return true;
    }
    int receive(void* /*buffer*/, size_t /*size*/) override { return 0; }
    void close() override {
        closed_ = true;
        closedFlag->store(true);
    }
    std::string getRemoteAddress() const override { return "127.0.0.1"; }
    uint16_t getRemotePort() const override { return 0; }

    std::vector<size_t> sendSizes;
    int failFromSend_ = -1;  // 第 N 次（1 基）之后全失败；-1 = 从不失败
    bool closed_ = false;
    std::shared_ptr<std::atomic<bool>> closedFlag = std::make_shared<std::atomic<bool>>(false);
};

// 重入监听器：回调里调回 manager（修复前即死锁）
class ReentrantListener : public ISessionListener {
public:
    explicit ReentrantListener(apollo::net::SessionManager& mgr) : mgr_(mgr) {}

    void onConnected(uint64_t) override {}
    void onDisconnected(uint64_t sessionId, int reason) override {
        disconnectedCount++;
        lastReason = reason;
        // 重入 manager：修复前此处持锁再取锁 → 死锁
        (void)mgr_.getSession(sessionId);
        (void)mgr_.getSessionCount();
    }
    void onDataReceived(uint64_t, const void*, size_t) override {}
    void onDataSent(uint64_t) override {}
    void onAuthenticated(uint64_t, uint64_t) override {}
    void onTimeout(uint64_t sessionId) override {
        timeoutCount++;
        (void)mgr_.getSession(sessionId);  // 重入
    }

    apollo::net::SessionManager& mgr_;
    std::atomic<int> disconnectedCount{0};
    std::atomic<int> timeoutCount{0};
    std::atomic<int> lastReason{0};
};

std::unique_ptr<FakeConnection> makeConn() {
    return std::make_unique<FakeConnection>();
}

// audit P1#10 死锁面①：closeSession 在持锁路径经 Session::close 触发
// onDisconnected，监听器重入 manager 即自锁。修复后出锁触发。
bool test_close_session_reentrant_listener() {
    auto& mgr = apollo::net::SessionManager::instance();
    mgr.setListener(nullptr);
    mgr.shutdown();
    ReentrantListener listener(mgr);
    mgr.initialize(100);
    mgr.setListener(&listener);
    auto closedFlag = std::make_shared<std::atomic<bool>>(false);

    const bool ok = [&]() -> bool {
        auto conn = makeConn();
        conn->closedFlag = closedFlag;
        ISession* session = mgr.createSession(std::move(conn));
        TEST_ASSERT(session != nullptr, "create");
        const uint64_t sid = session->getSessionId();
        TEST_ASSERT(mgr.getSessionCount() == 1, "count 1");

        mgr.closeSession(sid, 7);

        TEST_ASSERT(listener.disconnectedCount.load() == 1, "onDisconnected fired once");
        TEST_ASSERT(listener.lastReason.load() == 7, "reason propagated");
        TEST_ASSERT(mgr.getSession(sid) == nullptr, "session removed");
        TEST_ASSERT(mgr.getSessionCount() == 0, "count 0");
        TEST_ASSERT(closedFlag->load(), "connection closed");
        return true;
    }();

    mgr.setListener(nullptr);
    mgr.shutdown();
    return ok;
}

// 死锁面②：kickPlayer / closeSessionByPlayerId 同一持锁路径。
// 玩家映射由 manager 的 onAuthenticated 维护（Session::setPlayerId 只改
// 会话自身状态）。
bool test_kick_player_reentrant_listener() {
    auto& mgr = apollo::net::SessionManager::instance();
    mgr.setListener(nullptr);
    mgr.shutdown();
    ReentrantListener listener(mgr);
    mgr.initialize(100);
    mgr.setListener(&listener);

    const bool ok = [&]() -> bool {
        ISession* session = mgr.createSession(makeConn());
        TEST_ASSERT(session != nullptr, "create");
        session->setPlayerId(42);
        mgr.onAuthenticated(session->getSessionId(), 42);
        TEST_ASSERT(mgr.getSessionByPlayerId(42) == session, "player map");

        TEST_ASSERT(mgr.kickPlayer(42, 3), "kick");
        TEST_ASSERT(listener.disconnectedCount.load() == 1, "onDisconnected fired once");
        TEST_ASSERT(mgr.getSessionByPlayerId(42) == nullptr, "player map cleared");
        TEST_ASSERT(mgr.getSessionCount() == 0, "count 0");
        return true;
    }();

    mgr.setListener(nullptr);
    mgr.shutdown();
    return ok;
}

// 死锁面③：update 超时在锁内经 Session::update 触发 onTimeout +
// onDisconnected。修复后出锁触发，且已断会话下轮清理。
bool test_update_timeout_reentrant_listener() {
    auto& mgr = apollo::net::SessionManager::instance();
    mgr.setListener(nullptr);
    mgr.shutdown();
    ReentrantListener listener(mgr);
    mgr.initialize(100);
    mgr.setListener(&listener);

    const bool ok = [&]() -> bool {
        mgr.setDefaultTimeout(1000);  // setTimeout 在 Session 实现类上，经默认值注入
        ISession* session = mgr.createSession(makeConn());
        TEST_ASSERT(session != nullptr, "create");
        const uint64_t sid = session->getSessionId();
        // lastActiveTime 初值 = Time::now()（毫秒，量级为运行时长），相对其偏移计时
        const uint64_t base = session->getInfo().lastActiveTime;

        // 未超时：无事发生
        mgr.update(base + 500);
        TEST_ASSERT(listener.timeoutCount.load() == 0, "no timeout before deadline");

        // 超时：onTimeout + 关会话，均出锁；已断会话同轮清理（与原实现
        // 同 pass 摘除断言语义一致）
        mgr.update(base + 2000);
        TEST_ASSERT(listener.timeoutCount.load() == 1, "onTimeout fired once");
        TEST_ASSERT(listener.disconnectedCount.load() == 1, "onDisconnected fired once");
        TEST_ASSERT(mgr.getSession(sid) == nullptr, "closed and reaped same pass");
        TEST_ASSERT(mgr.getSessionCount() == 0, "count 0");
        return true;
    }();

    mgr.setListener(nullptr);
    mgr.shutdown();
    return ok;
}

// 死锁面④：shutdown 持锁逐会话 close
bool test_shutdown_reentrant_listener() {
    auto& mgr = apollo::net::SessionManager::instance();
    mgr.setListener(nullptr);
    mgr.shutdown();
    ReentrantListener listener(mgr);
    mgr.initialize(100);
    mgr.setListener(&listener);

    const bool ok = [&]() -> bool {
        for (int i = 0; i < 3; ++i) {
            TEST_ASSERT(mgr.createSession(makeConn()) != nullptr, "create");
        }
        TEST_ASSERT(mgr.getSessionCount() == 3, "count 3");

        mgr.shutdown();
        TEST_ASSERT(listener.disconnectedCount.load() == 3, "all onDisconnected fired");
        TEST_ASSERT(mgr.getSessionCount() == 0, "count 0");
        return true;
    }();

    mgr.setListener(nullptr);
    mgr.shutdown();
    return ok;
}

// audit P1#10 流错位：sendPacket 必须单次发送完整帧（4 字节头+体）；
// 发送失败必须断连。原实现两次 send 中第二次失败时流已错位且会话悬活。
bool test_send_packet_single_send_and_disconnect_on_fail() {
    auto& mgr = apollo::net::SessionManager::instance();
    mgr.setListener(nullptr);
    mgr.shutdown();
    ReentrantListener listener(mgr);
    mgr.initialize(100);
    mgr.setListener(&listener);
    FakeConnection* rawConn = nullptr;

    const bool ok = [&]() -> bool {
        auto conn = makeConn();
        rawConn = conn.get();
        ISession* session = mgr.createSession(std::move(conn));
        TEST_ASSERT(session != nullptr, "create");

        const char payload[] = "hello-frame";
        TEST_ASSERT(apollo::net::SessionHelper::sendPacket(session, payload, sizeof(payload)), "send ok");
        // 单次发送且长度 = 4 + sizeof(payload)（原实现是 4 与 sizeof 两次）
        TEST_ASSERT(rawConn->sendSizes.size() == 1, "single send per packet");
        TEST_ASSERT(rawConn->sendSizes[0] == 4 + sizeof(payload), "frame is header+body combined");

        // 下一次发送失败：断连（流不可续）
        rawConn->failFromSend_ = 1;
        TEST_ASSERT(!apollo::net::SessionHelper::sendPacket(session, payload, sizeof(payload)), "send fails");
        TEST_ASSERT(session->getState() == SessionState::Disconnected, "session disconnected on partial stream");
        TEST_ASSERT(listener.disconnectedCount.load() == 1, "disconnect notified once");
        return true;
    }();

    mgr.setListener(nullptr);
    mgr.shutdown();
    return ok;
}

bool test_send_packet_empty_payload() {
    auto& mgr = apollo::net::SessionManager::instance();
    mgr.setListener(nullptr);
    mgr.shutdown();
    mgr.initialize(100);
    FakeConnection* rawConn = nullptr;

    const bool ok = [&]() -> bool {
        auto conn = makeConn();
        rawConn = conn.get();
        ISession* session = mgr.createSession(std::move(conn));

        TEST_ASSERT(apollo::net::SessionHelper::sendPacket(session, nullptr, 0), "empty packet sends header only");
        TEST_ASSERT(rawConn->sendSizes.size() == 1 && rawConn->sendSizes[0] == 4,
                    "empty frame is 4-byte header");
        return true;
    }();

    mgr.setListener(nullptr);
    mgr.shutdown();
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
    run("test_close_session_reentrant_listener", test_close_session_reentrant_listener);
    run("test_kick_player_reentrant_listener", test_kick_player_reentrant_listener);
    run("test_update_timeout_reentrant_listener", test_update_timeout_reentrant_listener);
    run("test_shutdown_reentrant_listener", test_shutdown_reentrant_listener);
    run("test_send_packet_single_send_and_disconnect_on_fail", test_send_packet_single_send_and_disconnect_on_fail);
    run("test_send_packet_empty_payload", test_send_packet_empty_payload);
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    return (passed == total) ? 0 : 1;
}
