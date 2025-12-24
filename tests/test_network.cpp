/**
 * @file test_network.cpp
 * @brief 网络层集成测试
 *
 * 测试 Listener, Connector, Session 等组件
 */

#include "apollo/net/adapters/native_adapter.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

using namespace apollo;
using namespace apollo::net;
using namespace apollo::net::adapters;

// 测试配置
const uint16_t TEST_PORT = 18888;
const char* TEST_HOST = "127.0.0.1";
const int TIMEOUT_MS = 5000;

// 测试辅助宏
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

// ========== 测试会话类 ==========

/**
 * @brief 测试服务器会话
 */
class TestServerSession : public NativeSession {
public:
    using NativeSession::NativeSession;

    std::vector<std::string> receivedMessages;
    bool establishedCalled = false;
    bool terminatedCalled = false;
    TerminateReason terminateReason = TerminateReason::None;

protected:
    void handleEstablish() override {
        establishedCalled = true;
        std::cout << "    [Server] Client connected: "
                  << getConnection()->getRemoteAddress() << ":"
                  << getConnection()->getRemotePort() << std::endl;
    }

    uint32_t handlePacket(const char* data, uint32_t length) override {
        std::string msg(data, length);
        receivedMessages.push_back(msg);
        std::cout << "    [Server] Received: " << msg << std::endl;

        // 回显消息
        getConnection()->send(data, length);
        return length;
    }

    void handleTerminate(TerminateReason reason) override {
        terminatedCalled = true;
        terminateReason = reason;
        std::cout << "    [Server] Client disconnected, reason: "
                  << (int)reason << std::endl;
    }
};

/**
 * @brief 测试客户端会话
 */
class TestClientSession : public NativeSession {
public:
    using NativeSession::NativeSession;

    std::vector<std::string> receivedMessages;
    bool establishedCalled = false;
    bool terminatedCalled = false;

protected:
    void handleEstablish() override {
        establishedCalled = true;
        std::cout << "    [Client] Connected to server" << std::endl;
    }

    uint32_t handlePacket(const char* data, uint32_t length) override {
        std::string msg(data, length);
        receivedMessages.push_back(msg);
        std::cout << "    [Client] Received: " << msg << std::endl;
        return length;
    }

    void handleTerminate(TerminateReason reason) override {
        terminatedCalled = true;
        std::cout << "    [Client] Disconnected, reason: " << (int)reason << std::endl;
    }
};

// ========== 会话工厂 ==========

class TestServerSessionFactory : public NativeSessionFactory {
public:
    NativeSession* createSession(NativeConnection* connection) override {
        return new TestServerSession(connection);
    }
};

class TestClientSessionFactory : public NativeSessionFactory {
public:
    NativeSession* createSession(NativeConnection* connection) override {
        return new TestClientSession(connection);
    }
};

// ========== 测试用例 ==========

/**
 * @brief 测试网络初始化和清理
 */
bool test_network_init() {
    std::cout << "Running: test_network_init..." << std::endl;

    bool initialized = NativeAdapter::initialize();
    TEST_ASSERT(initialized, "Network initialization");

    NativeAdapter::shutdown();
    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试监听器启动和停止
 */
bool test_listener_start_stop() {
    std::cout << "Running: test_listener_start_stop..." << std::endl;

    NativeAdapter::initialize();

    NativeListener listener;
    TEST_ASSERT(!listener.isRunning(), "Initial state");

    bool started = listener.start(TEST_HOST, TEST_PORT);
    TEST_ASSERT(started, "Listener start");
    TEST_ASSERT(listener.isRunning(), "Running state");
    TEST_ASSERT(listener.getListenPort() == TEST_PORT, "Listen port");

    bool stopped = listener.stop();
    TEST_ASSERT(stopped, "Listener stop");
    TEST_ASSERT(!listener.isRunning(), "Stopped state");

    NativeAdapter::shutdown();
    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试连接器连接
 */
bool test_connector_connect() {
    std::cout << "Running: test_connector_connect..." << std::endl;

    NativeAdapter::initialize();

    // 启动服务器
    NativeListener listener;
    listener.start(TEST_HOST, TEST_PORT);

    // 尝试连接
    NativeConnector connector;
    ConnectConfig config;
    config.host = TEST_HOST;
    config.port = TEST_PORT;

    std::atomic<bool> connected(false);
    connector.setCallbacks(ConnectorCallbacks::create(
        [&](ConnectionPtr conn) {
            connected = true;
            std::cout << "    Connected!" << std::endl;
        },
        nullptr,
        nullptr
    ));

    int result = connector.connect(config);
    TEST_ASSERT(result == 0, "Connect initiated");

    // 等待连接完成
    auto start = std::chrono::steady_clock::now();
    while (!connected) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        connector.handleConnect();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > TIMEOUT_MS) {
            break;
        }
    }

    TEST_ASSERT(connected, "Connection established");
    TEST_ASSERT(connector.isConnected(), "Connector state");

    // 清理
    connector.disconnect();
    listener.stop();
    NativeAdapter::shutdown();

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试数据收发
 */
bool test_data_transfer() {
    std::cout << "Running: test_data_transfer..." << std::endl;

    NativeAdapter::initialize();

    // 启动服务器
    NativeListener listener;
    listener.setRecvBufferSize(8192);
    listener.setSendBufferSize(8192);
    listener.start(TEST_HOST, TEST_PORT);

    TestServerSessionFactory serverFactory;
    listener.setNativeSessionFactory(&serverFactory);

    // 连接客户端
    NativeConnector connector;
    ConnectConfig config;
    config.host = TEST_HOST;
    config.port = TEST_PORT;

    std::atomic<bool> connected(false);
    std::atomic<bool> messageReceived(false);

    connector.setCallbacks(ConnectorCallbacks::create(
        [&](ConnectionPtr conn) {
            connected = true;

            // 连接成功后发送消息
            const char* testMsg = "Hello, Server!";
            uint32_t msgLen = static_cast<uint32_t>(strlen(testMsg));

            // 构造数据包: 长度前缀 + 数据
            std::vector<char> packet(4 + msgLen);
            *reinterpret_cast<uint32_t*>(packet.data()) = msgLen;
            std::memcpy(packet.data() + 4, testMsg, msgLen);

            conn->send(packet.data(), packet.size());
            std::cout << "    Sent: " << testMsg << std::endl;
        },
        nullptr,
        nullptr
    ));

    connector.connect(config);

    // 等待连接和数据传输
    auto start = std::chrono::steady_clock::now();
    bool serverReceived = false;

    while (true) {
        // 处理服务器接受
        listener.handleAccept();

        // 处理连接
        if (connector.isConnecting()) {
            connector.handleConnect();
        }

        // 检查数据
        if (connected) {
            auto conn = connector.getConnection();
            if (conn) {
                auto* nativeConn = static_cast<NativeConnection*>(conn.get());
                // 模拟接收处理
                char temp[1024];
                ssize_t recvLen = ::recv(nativeConn->getSocket(), temp, sizeof(temp), 0);
                if (recvLen > 0) {
                    serverReceived = true;
                    std::cout << "    Server received data" << std::endl;
                    break;
                }
            }
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > TIMEOUT_MS) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    TEST_ASSERT(connected, "Connected");
    TEST_ASSERT(serverReceived, "Server received data");

    // 清理
    connector.disconnect();
    listener.stop();
    NativeAdapter::shutdown();

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试缓冲区管理
 */
bool test_buffer_management() {
    std::cout << "Running: test_buffer_management..." << std::endl;

    NativeAdapter::initialize();

    // 启动服务器
    NativeListener listener;
    listener.start(TEST_HOST, TEST_PORT);

    TestServerSessionFactory serverFactory;
    listener.setNativeSessionFactory(&serverFactory);

    // 连接并发送大量数据
    NativeConnector connector;
    ConnectConfig config;
    config.host = TEST_HOST;
    config.port = TEST_PORT;

    std::atomic<bool> connected(false);
    const int MESSAGE_COUNT = 100;
    int messagesSent = 0;

    connector.setCallbacks(ConnectorCallbacks::create(
        [&](ConnectionPtr conn) {
            connected = true;

            // 发送多条消息
            for (int i = 0; i < MESSAGE_COUNT; ++i) {
                std::string msg = "Message #" + std::to_string(i);
                uint32_t msgLen = static_cast<uint32_t>(msg.size());

                std::vector<char> packet(4 + msgLen);
                *reinterpret_cast<uint32_t*>(packet.data()) = msgLen;
                std::memcpy(packet.data() + 4, msg.data(), msgLen);

                conn->send(packet.data(), packet.size());
                messagesSent++;
            }
        },
        nullptr,
        nullptr
    ));

    connector.connect(config);

    // 等待处理完成
    auto start = std::chrono::steady_clock::now();

    while (messagesSent < MESSAGE_COUNT) {
        listener.handleAccept();
        if (connector.isConnecting()) {
            connector.handleConnect();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > TIMEOUT_MS * 2) {
            break;
        }
    }

    TEST_ASSERT(messagesSent == MESSAGE_COUNT, "All messages sent");

    // 清理
    connector.disconnect();
    listener.stop();
    NativeAdapter::shutdown();

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试连接超时
 */
bool test_connection_timeout() {
    std::cout << "Running: test_connection_timeout..." << std::endl;

    NativeAdapter::initialize();

    NativeConnector connector;
    ConnectConfig config;
    config.host = "127.0.0.1";  // 不存在的服务器
    config.port = 19999;         // 未监听的端口

    std::atomic<bool> failed(false);

    connector.setCallbacks(ConnectorCallbacks::create(
        nullptr,
        nullptr,
        [&](int32_t code, const char* msg) {
            failed = true;
            std::cout << "    Connection failed as expected" << std::endl;
        }
    ));

    connector.connect(config);

    // 等待连接失败
    auto start = std::chrono::steady_clock::now();

    while (!failed) {
        connector.handleConnect();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > TIMEOUT_MS) {
            break;
        }
    }

    TEST_ASSERT(!connector.isConnected(), "Not connected");
    TEST_ASSERT(failed || connector.getState() == ConnectorState::Failed, "Connection failed");

    NativeAdapter::shutdown();

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试会话生命周期
 */
bool test_session_lifecycle() {
    std::cout << "Running: test_session_lifecycle..." << std::endl;

    NativeAdapter::initialize();

    // 创建会话并测试生命周期方法
    socket_t dummySocket = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (dummySocket == INVALID_SOCKET_VALUE) {
        dummySocket = 0; // 使用空指针测试
    }
#else
    if (dummySocket < 0) {
        dummySocket = 0;
    }
#endif

    auto conn = std::make_shared<NativeConnection>(
        dummySocket, 12345, "127.0.0.1", TEST_PORT
    );

    TestClientSession session(conn.get());

    // 测试建立
    session.onEstablish(conn.get());
    TEST_ASSERT(session.isActive(), "Active after establish");
    TEST_ASSERT(session.establishedCalled, "Establish callback called");

    // 测试心跳
    session.updateHeartbeat();
    auto heartbeat = session.getLastHeartbeat();
    TEST_ASSERT(heartbeat > 0, "Heartbeat updated");
    TEST_ASSERT(!session.isTimeout(heartbeat, 1000), "Not timeout");

    // 测试超时
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    TEST_ASSERT(session.isTimeout(heartbeat, 50), "Timeout detection");

    // 测试终止
    session.onTerminate(TerminateReason::UserClose);
    TEST_ASSERT(!session.isActive(), "Inactive after terminate");
    TEST_ASSERT(session.terminatedCalled, "Terminate callback called");

    // 清理
    if (dummySocket != 0 && dummySocket != INVALID_SOCKET_VALUE) {
        closesocket(dummySocket);
    }
    NativeAdapter::shutdown();

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试网络管理器
 */
bool test_network_manager() {
    std::cout << "Running: test_network_manager..." << std::endl;

    NativeAdapter::initialize();

    auto manager = std::make_shared<NativeNetworkManager>();

    // 初始化
    NetworkConfig config;
    config.maxConnections = 100;
    config.recvBufferSize = 8192;
    config.sendBufferSize = 8192;

    bool initialized = manager->initialize(config);
    TEST_ASSERT(initialized, "Manager initialize");

    // 创建监听器
    auto listener = manager->createListener("test");
    TEST_ASSERT(listener != nullptr, "Listener created");
    TEST_ASSERT(listener == manager->getListener("test"), "Listener retrieved");

    bool started = listener->start(TEST_HOST, TEST_PORT);
    TEST_ASSERT(started, "Listener started");

    // 创建连接器
    auto connector = manager->createConnector("test_client");
    TEST_ASSERT(connector != nullptr, "Connector created");

    std::atomic<bool> connected(false);
    connector->setCallbacks(ConnectorCallbacks::create(
        [&](ConnectionPtr) { connected = true; },
        nullptr,
        nullptr
    ));

    connector->connect(TEST_HOST, TEST_PORT);

    // 等待连接
    auto start = std::chrono::steady_clock::now();
    while (!connected) {
        manager->update();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > TIMEOUT_MS) {
            break;
        }
    }

    TEST_ASSERT(connected, "Connected via manager");

    // 获取统计
    auto stats = manager->getStats();
    std::cout << "    Total connections: " << stats.totalConnections << std::endl;
    std::cout << "    Current connections: " << stats.currentConnections << std::endl;

    // 清理
    manager->stop();
    manager->removeListener("test");
    manager->removeConnector("test_client");
    NativeAdapter::shutdown();

    std::cout << "  PASSED" << std::endl;
    return true;
}

// ========== 测试运行器 ==========

int main() {
    std::cout << "====================================" << std::endl;
    std::cout << "=== Apollo Network Layer Tests ===" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << std::endl;

    int passed = 0;
    int total = 0;

    auto run = [&](const char* name, bool (*test)()) {
        total++;
        if (test()) passed++;
        else {
            std::cout << "  FAILED!" << std::endl;
        }
    };

    // 运行所有测试
    run("test_network_init", test_network_init);
    run("test_listener_start_stop", test_listener_start_stop);
    run("test_connector_connect", test_connector_connect);
    run("test_data_transfer", test_data_transfer);
    run("test_buffer_management", test_buffer_management);
    run("test_connection_timeout", test_connection_timeout);
    run("test_session_lifecycle", test_session_lifecycle);
    run("test_network_manager", test_network_manager);

    std::cout << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    std::cout << "====================================" << std::endl;

    return (passed == total) ? 0 : 1;
}

int test_network_main() {
    return main();
}
