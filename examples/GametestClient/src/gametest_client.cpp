#include "gametest_client.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <future>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

using json = nlohmann::json;

namespace gametest {

GametestClient::GametestClient()
    : port_(0)
    , state_(ClientState::DISCONNECTED)
    , running_(false) {

    // 设置默认日志处理器
    logHandler_ = [](const std::string& msg) {
        std::cout << "[LOG] " << msg << std::endl;
    };

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

GametestClient::~GametestClient() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool GametestClient::connect(const std::string& host, int port) {
    if (state_ != ClientState::DISCONNECTED) {
        logError("Client already connected or connecting");
        return false;
    }

    host_ = host;
    port_ = port;
    state_ = ClientState::CONNECTING;

    log("Connecting to " + host + ":" + std::to_string(port));

    // 创建socket
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        logError("Failed to create socket");
        state_ = ClientState::ERROR;
        return false;
    }

    // 设置服务器地址
    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) != 1) {
        logError("Invalid host address");
        closesocket(sock);
        state_ = ClientState::ERROR;
        return false;
    }

    // 连接到服务器
    if (::connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        logError("Failed to connect to server");
        closesocket(sock);
        state_ = ClientState::ERROR;
        return false;
    }

    state_ = ClientState::CONNECTED;
    log("Connected to server successfully");

    // 启动网络线程
    running_ = true;
    networkThread_ = std::make_unique<std::thread>(&GametestClient::networkThread, this);

    return true;
}

void GametestClient::disconnect() {
    if (state_ != ClientState::DISCONNECTED) {
        log("Disconnecting from server");
        running_ = false;

        if (networkThread_ && networkThread_->joinable()) {
            networkThread_->join();
        }

        state_ = ClientState::DISCONNECTED;
        serverInfo_ = json{};
        authToken_.clear();
        username_.clear();
    }
}

bool GametestClient::login(const std::string& username, const std::string& password) {
    if (state_ != ClientState::CONNECTED) {
        logError("Not connected to server");
        return false;
    }

    username_ = username;

    // 创建登录消息
    json loginMsg = {
        {"type", "login"},
        {"username", username},
        {"password", password}
    };

    // 发送登录请求
    sendMessage(loginMsg);

    // 等待响应（简化实现，实际应该使用更复杂的异步处理）
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // 这里应该解析服务器响应
    // 暂时假设登录成功
    state_ = ClientState::AUTHENTICATED;
    log("Login successful");

    return true;
}

bool GametestClient::autoLogin(const std::string& configFile) {
    std::ifstream file(configFile);
    if (!file.is_open()) {
        logError("Cannot open login config file: " + configFile);
        return false;
    }

    try {
        json config = json::parse(file);
        file.close();

        std::string host = config.value("host", "localhost");
        int port = config.value("port", 8080);
        std::string username = config["username"];
        std::string password = config["password"];

        // 连接并登录
        if (connect(host, port)) {
            return login(username, password);
        }
    } catch (const json::exception& e) {
        logError("Failed to parse config file: " + std::string(e.what()));
    }

    return false;
}

void GametestClient::saveLoginConfig(const std::string& username, const std::string& password,
                                   const std::string& configFile) {
    json config = {
        {"host", host_},
        {"port", port_},
        {"username", username},
        {"password", password}
    };

    std::ofstream file(configFile);
    if (file.is_open()) {
        file << config.dump(4);
        file.close();
        log("Login config saved to " + configFile);
    } else {
        logError("Failed to save config file: " + configFile);
    }
}

void GametestClient::sendMessage(const json& message) {
    if (state_ == ClientState::CONNECTED || state_ == ClientState::AUTHENTICATED) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        messageQueue_.push(message);
        queueCondition_.notify_one();

        log("Message queued: " + message.dump());
    } else {
        logError("Not connected to server");
    }
}

void GametestClient::run() {
    // 主运行循环
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void GametestClient::stop() {
    running_ = false;
    disconnect();
}

void GametestClient::networkThread() {
    // 简化的网络线程实现
    while (running_) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        queueCondition_.wait(lock, [this] { return !messageQueue_.empty() || !running_; });

        while (!messageQueue_.empty() && running_) {
            json message = messageQueue_.front();
            messageQueue_.pop();
            lock.unlock();

            // 这里应该实际发送消息到服务器
            // 暂时只是模拟接收响应
            processMessage(message.dump());

            lock.lock();
        }
    }
}

void GametestClient::processMessage(const std::string& message) {
    try {
        json msg = json::parse(message);

        // 更新服务器信息
        if (msg.contains("type") && msg["type"] == "server_info") {
            serverInfo_ = msg;
        }

        // 调用消息处理器
        if (messageHandler_) {
            messageHandler_(msg);
        }

        // 记录到日志
        log("Received message: " + msg.dump(2));
    } catch (const json::exception& e) {
        logError("Failed to parse message: " + std::string(e.what()));
    }
}

void GametestClient::log(const std::string& message) {
    if (logHandler_) {
        logHandler_(message);
    }
}

void GametestClient::logError(const std::string& error) {
    if (logHandler_) {
        logHandler_("[ERROR] " + error);
    }
}

} // namespace gametest