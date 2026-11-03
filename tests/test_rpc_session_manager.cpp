#include "apollo/net/session_manager.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

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
        ++sendCalls;
        return true;
    }
    int sendCalls = 0;
};

} // namespace

int main() {
    auto& manager = apollo::net::SessionManager::instance();
    auto connection = std::make_unique<RecordingConnection>();
    auto* connectionRaw = connection.get();
    manager.initialize(1);
    auto* session = manager.createSession(std::move(connection));
    if (session == nullptr) {
        std::cerr << "failed to create session\n";
        return 1;
    }
    session->setPlayerId(77);
    manager.onAuthenticated(session->getSessionId(), 77);

    std::atomic<bool> returned{false};
    std::thread watcher([&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
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

    bool passed = found == session;
    if (passed) {
        const auto broadcastCount = manager.broadcast(nullptr, 0);
        passed = broadcastCount == 1 && connectionRaw->sendCalls == 1;
    }
    manager.shutdown();
    std::cout << (passed ? "RpcSessionManagerTests: passed\n"
                         : "RpcSessionManagerTests: wrong session\n");
    return passed ? 0 : 1;
}
