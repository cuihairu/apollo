#pragma once

#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace gametest {

enum class ClientState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    AUTHENTICATED,
    ERROR
};

class GametestClient {
public:
    using MessageHandler = std::function<void(const json&)>;
    using LogHandler = std::function<void(const std::string&)>;

    GametestClient();
    ~GametestClient();

    // 连接管理
    bool connect(const std::string& host, int port);
    void disconnect();
    ClientState getState() const { return state_; }

    // 认证
    bool login(const std::string& username, const std::string& password);
    bool autoLogin(const std::string& configFile = "config/login.json");
    void saveLoginConfig(const std::string& username, const std::string& password,
                        const std::string& configFile = "config/login.json");

    // 消息处理
    void sendMessage(const json& message);
    void setMessageHandler(MessageHandler handler) { messageHandler_ = handler; }
    void setLogHandler(LogHandler handler) { logHandler_ = handler; }

    // 运行循环
    void run();
    void stop();

    // 获取服务器信息
    json getServerInfo() const { return serverInfo_; }

private:
    // 网络线程函数
    void networkThread();
    void processMessage(const std::string& message);

    // 日志输出
    void log(const std::string& message);
    void logError(const std::string& error);

    // 成员变量
    std::string host_;
    int port_;
    ClientState state_;
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> networkThread_;

    // 消息队列
    std::queue<json> messageQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;

    // 处理器
    MessageHandler messageHandler_;
    LogHandler logHandler_;

    // 服务器信息
    json serverInfo_;

    // 认证信息
    std::string authToken_;
    std::string username_;
};

} // namespace gametest