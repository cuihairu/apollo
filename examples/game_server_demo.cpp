/**
 * @file game_server_demo.cpp
 * @brief 完整游戏服务器示例
 *
 * 演示如何使用 Apollo 框架构建一个简单的 MMORPG 服务器架构
 * 包含：登录服务器、游戏服务器、网关服务器的完整流程
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace apollo {
namespace demo {

//==============================================================================
// 消息协议定义
//==============================================================================

/// 消息类型
enum class MsgType : uint16_t {
    // 客户端 -> 登录服务器
    CS_LOGIN_REQUEST = 1001,
    CS_LOGOUT_REQUEST = 1002,

    // 登录服务器 -> 客户端
    SC_LOGIN_RESPONSE = 2001,
    SC_LOGOUT_RESPONSE = 2002,

    // 客户端 -> 网关 -> 游戏服务器
    CS_ENTER_GAME = 1003,
    CS_MOVE_REQUEST = 1004,
    CS_CAST_SKILL = 1005,
    CS_CHAT_MESSAGE = 1006,

    // 游戏服务器 -> 网关 -> 客户端
    SC_ENTER_GAME_RESPONSE = 2003,
    SC_PLAYER_INFO = 2004,
    SC_MOVE_NOTIFY = 2005,
    SC_SKILL_EFFECT = 2006,
    SC_CHAT_NOTIFY = 2007,
    SC_AOI_UPDATE = 2008,

    // 服务器间消息
    SS_REGISTER_GATEWAY = 3001,
    SS_PLAYER_LOGIN = 3002,
    SS_PLAYER_LOGOUT = 3003,
    SS_BROADCAST = 3004
};

/// 消息头
struct MsgHeader {
    MsgType type;
    uint32_t length;
    uint64_t playerId;

    static constexpr size_t SIZE = 14;

    MsgHeader() : type(MsgType::CS_LOGIN_REQUEST), length(0), playerId(0) {}
    MsgHeader(MsgType t, uint64_t pid = 0) : type(t), length(0), playerId(pid) {}
};

/// 登录请求
struct LoginRequest {
    char account[32];
    char password[32];
    char deviceId[64];

    LoginRequest() {
        std::memset(account, 0, sizeof(account));
        std::memset(password, 0, sizeof(password));
        std::memset(deviceId, 0, sizeof(deviceId));
    }
};

/// 登录响应
struct LoginResponse {
    uint32_t resultCode;  // 0=成功
    uint64_t playerId;
    char token[64];
    char gatewayIp[16];
    uint16_t gatewayPort;

    LoginResponse()
        : resultCode(0), playerId(0), gatewayPort(0) {
        std::memset(token, 0, sizeof(token));
        std::memset(gatewayIp, 0, sizeof(gatewayIp));
    }
};

/// 移动请求
struct MoveRequest {
    float x, y, z;
};

/// 玩家信息
struct PlayerInfo {
    uint64_t playerId;
    char name[32];
    uint32_t level;
    uint32_t hp;
    uint32_t maxHp;
    float x, y, z;

    PlayerInfo()
        : playerId(0), level(1), hp(100), maxHp(100), x(0), y(0), z(0) {
        std::memset(name, 0, sizeof(name));
    }
};

/// AOI 实体信息
struct AOIEntityInfo {
    uint64_t entityId;
    uint8_t entityType;  // 0=player, 1=npc, 2=monster
    float x, y, z;

    AOIEntityInfo() : entityId(0), entityType(0), x(0), y(0), z(0) {}
};

//==============================================================================
// 玩家数据存储
//==============================================================================

class PlayerDatabase {
public:
    struct PlayerData {
        uint64_t playerId;
        std::string account;
        std::string name;
        uint32_t level;
        uint64_t exp;
        uint32_t hp;
        uint32_t maxHp;
    };

    static PlayerDatabase& instance() {
        static PlayerDatabase db;
        return db;
    }

    bool verifyAccount(const std::string& account, const std::string& password) {
        // 模拟验证
        return !account.empty() && account.length() >= 3;
    }

    PlayerData* loadByAccount(const std::string& account) {
        std::lock_guard<std::mutex> lock(mutex_);

        // 模拟：如果不存在就创建
        auto& data = players_[account];
        if (data.playerId == 0) {
            data.playerId = nextPlayerId_++;
            data.account = account;
            data.name = "Player" + std::to_string(data.playerId);
            data.level = 1;
            data.exp = 0;
            data.hp = 100;
            data.maxHp = 100;
        }
        return &data;
    }

    PlayerData* loadById(uint64_t playerId) {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& pair : players_) {
            if (pair.second.playerId == playerId) {
                return &pair.second;
            }
        }
        return nullptr;
    }

    void save(const PlayerData& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        // 模拟保存
    }

private:
    PlayerDatabase() : nextPlayerId_(1001) {}

    std::unordered_map<std::string, PlayerData> players_;
    uint64_t nextPlayerId_;
    std::mutex mutex_;
};

//==============================================================================
// 消息队列（用于服务器间通信）
//==============================================================================

template<typename T>
class MessageQueue {
public:
    void push(const T& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(msg);
        cv_.notify_one();
    }

    bool pop(T& msg, int timeoutMs = 0) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (timeoutMs > 0) {
            if (!cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                            [this] { return !queue_.empty(); })) {
                return false;
            }
        } else {
            cv_.wait(lock, [this] { return !queue_.empty(); });
        }

        msg = queue_.front();
        queue_.pop();
        return true;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

//==============================================================================
// 服务器基类
//==============================================================================

class BaseServer {
public:
    BaseServer(const std::string& name, uint16_t port)
        : name_(name), port_(port), running_(false) {}

    virtual ~BaseServer() = default;

    bool start() {
        if (running_) return false;

        std::cout << "[" << name_ << "] Starting on port " << port_ << "..." << std::endl;

        running_ = true;
        thread_ = std::thread(&BaseServer::run, this);

        onServerStart();
        return true;
    }

    void stop() {
        if (!running_) return;

        std::cout << "[" << name_ << "] Stopping..." << std::endl;
        running_ = false;
        onServerStop();

        if (thread_.joinable()) {
            thread_.join();
        }
    }

    bool isRunning() const { return running_; }

protected:
    virtual void onServerStart() {}
    virtual void onServerStop() {}
    virtual void run() = 0;

    std::string name_;
    uint16_t port_;
    std::atomic<bool> running_;
    std::thread thread_;
};

//==============================================================================
// 登录服务器
//==============================================================================

class LoginServer : public BaseServer {
public:
    struct LoginEvent {
        uint64_t sessionId;
        LoginRequest request;
    };

    LoginServer(uint16_t port = 7001)
        : BaseServer("LoginServer", port) {}

    // 处理登录请求
    void handleLogin(uint64_t sessionId, const LoginRequest& req) {
        std::cout << "[" << name_ << "] Login attempt: " << req.account << std::endl;

        LoginResponse resp;

        // 验证账号
        if (!PlayerDatabase::instance().verifyAccount(req.account, req.password)) {
            resp.resultCode = 1;  // 认证失败
            sendResponse(sessionId, MsgType::SC_LOGIN_RESPONSE, resp);
            return;
        }

        // 加载玩家数据
        auto* playerData = PlayerDatabase::instance().loadByAccount(req.account);

        resp.resultCode = 0;
        resp.playerId = playerData->playerId;
        std::snprintf(resp.token, sizeof(resp.token), "token_%llu", resp.playerId);
        std::snprintf(resp.gatewayIp, sizeof(resp.gatewayIp), "127.0.0.1");
        resp.gatewayPort = 7002;

        sendResponse(sessionId, MsgType::SC_LOGIN_RESPONSE, resp);

        std::cout << "[" << name_ << "] Player " << playerData->playerId
                  << " logged in successfully" << std::endl;
    }

    void sendResponse(uint64_t sessionId, MsgType type, const auto& data) {
        std::cout << "[" << name_ << "] Sending response to session "
                  << sessionId << std::endl;
        // 实际实现会发送到网络
    }

protected:
    void run() override {
        while (running_) {
            // 模拟处理登录请求
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
};

//==============================================================================
// 网关服务器
//==============================================================================

class GatewayServer : public BaseServer {
public:
    struct ClientSession {
        uint64_t sessionId;
        uint64_t playerId;
        std::string account;
        bool authenticated;
    };

    GatewayServer(uint16_t port = 7002)
        : BaseServer("GatewayServer", port), nextSessionId_(1) {}

    // 客户端连接
    uint64_t onClientConnect(const std::string& addr) {
        uint64_t sessionId = nextSessionId_++;

        ClientSession session;
        session.sessionId = sessionId;
        session.playerId = 0;
        session.authenticated = false;

        sessions_[sessionId] = session;

        std::cout << "[" << name_ << "] Client connected from " << addr
                  << " (session: " << sessionId << ")" << std::endl;

        return sessionId;
    }

    // 客户端断开
    void onClientDisconnect(uint64_t sessionId) {
        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            if (it->second.playerId > 0) {
                // 通知游戏服务器玩家离线
                notifyPlayerLogout(it->second.playerId);
            }
            sessions_.erase(it);
            std::cout << "[" << name_ << "] Session " << sessionId << " closed" << std::endl;
        }
    }

    // 转发消息到游戏服务器
    void forwardToGameServer(uint64_t sessionId, MsgType type,
                            const void* data, size_t length) {
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end() || !it->second.authenticated) {
            return;
        }

        std::cout << "[" << name_ << "] Forwarding msg type " << static_cast<int>(type)
                  << " from player " << it->second.playerId << " to game server" << std::endl;

        // 实际实现会通过网络发送
    }

    // 从游戏服务器转发消息到客户端
    void forwardToClient(uint64_t playerId, MsgType type,
                        const void* data, size_t length) {
        // 查找玩家会话
        for (auto& pair : sessions_) {
            if (pair.second.playerId == playerId) {
                std::cout << "[" << name_ << "] Forwarding msg type "
                          << static_cast<int>(type) << " to player " << playerId << std::endl;
                return;
            }
        }
    }

    // 认证成功
    void onAuthenticated(uint64_t sessionId, uint64_t playerId) {
        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            it->second.playerId = playerId;
            it->second.authenticated = true;

            // 通知游戏服务器玩家登录
            notifyPlayerLogin(playerId);
        }
    }

    void notifyPlayerLogin(uint64_t playerId) {
        std::cout << "[" << name_ << "] Notify game server: player "
                  << playerId << " logged in" << std::endl;
    }

    void notifyPlayerLogout(uint64_t playerId) {
        std::cout << "[" << name_ << "] Notify game server: player "
                  << playerId << " logged out" << std::endl;
    }

protected:
    void run() override {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

private:
    std::unordered_map<uint64_t, ClientSession> sessions_;
    uint64_t nextSessionId_;
};

//==============================================================================
// 游戏服务器
//==============================================================================

class GameServer : public BaseServer {
public:
    struct OnlinePlayer {
        uint64_t playerId;
        std::string name;
        uint32_t level;
        float x, y, z;
        uint64_t sessionId;

        // AOI 相关
        std::vector<uint64_t> visibleEntities;
    };

    GameServer(uint16_t port = 7003)
        : BaseServer("GameServer", port) {}

    // 玩家进入游戏
    void onPlayerEnter(uint64_t playerId) {
        auto* playerData = PlayerDatabase::instance().loadById(playerId);
        if (!playerData) {
            std::cout << "[" << name_ << "] Player " << playerId << " not found" << std::endl;
            return;
        }

        OnlinePlayer player;
        player.playerId = playerId;
        player.name = playerData->name;
        player.level = playerData->level;
        player.x = 100.0f;
        player.y = 0.0f;
        player.z = 100.0f;

        players_[playerId] = player;

        std::cout << "[" << name_ << "] Player " << player.name
                  << " (ID:" << playerId << ") entered game at ("
                  << player.x << ", " << player.z << ")" << std::endl;

        // 发送玩家信息
        sendPlayerInfo(playerId, player);

        // 发送周围实体（AOI）
        updateAOI(playerId);
    }

    // 玩家离开
    void onPlayerLeave(uint64_t playerId) {
        auto it = players_.find(playerId);
        if (it != players_.end()) {
            std::cout << "[" << name_ << "] Player " << it->second.name
                      << " (ID:" << playerId << ") left game" << std::endl;

            // 通知周围玩家：playerId 从他们的视野中离开
            for (uint64_t otherId : it->second.visibleEntities) {
                notifyEntityLeave(otherId, playerId);
            }

            players_.erase(it);
        }
    }

    // 处理移动
    void onMoveRequest(uint64_t playerId, const MoveRequest& req) {
        auto it = players_.find(playerId);
        if (it == players_.end()) return;

        auto& player = it->second;

        // 简单的距离检查
        float dx = req.x - player.x;
        float dz = req.z - player.z;
        float dist = std::sqrt(dx * dx + dz * dz);

        if (dist > 10.0f) {
            // 移动距离过大，可能是作弊
            std::cout << "[" << name_ << "] Player " << player.name
                      << " tried to move too far!" << std::endl;
            return;
        }

        player.x = req.x;
        player.y = req.y;
        player.z = req.z;

        // 广播移动
        broadcastMove(playerId, player);

        // 更新 AOI
        updateAOI(playerId);
    }

    // 处理聊天消息
    void onChatMessage(uint64_t playerId, const std::string& msg) {
        auto it = players_.find(playerId);
        if (it == players_.end()) return;

        std::cout << "[" << name_ << "] Chat - " << it->second.name
                  << ": " << msg << std::endl;

        // 广播给附近玩家
        broadcastChat(playerId, it->second.name, msg);
    }

protected:
    void run() override {
        std::cout << "[" << name_ << "] Game loop started" << std::endl;

        while (running_) {
            // 游戏循环 tick
            gameTick();

            std::this_thread::sleep_for(std::chrono::milliseconds(50));  // 20 FPS
        }
    }

private:
    void gameTick() {
        // 处理定时事件、AI、战斗等
        static int tickCount = 0;
        if (++tickCount % 100 == 0) {  // 每5秒
            std::cout << "[" << name_ << "] Online players: "
                      << players_.size() << std::endl;
        }
    }

    void sendPlayerInfo(uint64_t playerId, const OnlinePlayer& player) {
        std::cout << "[" << name_ << "] Sending player info to "
                  << playerId << ": " << player.name
                  << " Lv." << player.level << std::endl;
    }

    void updateAOI(uint64_t playerId) {
        auto& player = players_[playerId];
        std::vector<uint64_t> newVisible;

        // 查找附近的玩家
        for (const auto& pair : players_) {
            if (pair.first == playerId) continue;

            float dx = pair.second.x - player.x;
            float dz = pair.second.z - player.z;
            float dist = std::sqrt(dx * dx + dz * dz);

            if (dist < 50.0f) {  // 50米视野
                newVisible.push_back(pair.first);
            }
        }

        // 检查变化
        auto& oldVisible = player.visibleEntities;

        // 进入视野的实体
        for (uint64_t id : newVisible) {
            if (std::find(oldVisible.begin(), oldVisible.end(), id) == oldVisible.end()) {
                notifyEntityEnter(playerId, id);
            }
        }

        // 离开视野的实体
        for (uint64_t id : oldVisible) {
            if (std::find(newVisible.begin(), newVisible.end(), id) == newVisible.end()) {
                notifyEntityLeave(playerId, id);
            }
        }

        player.visibleEntities = newVisible;
    }

    void notifyEntityEnter(uint64_t playerId, uint64_t entityId) {
        auto it = players_.find(entityId);
        if (it != players_.end()) {
            std::cout << "[" << name_ << "] Entity " << it->second.name
                      << " entered " << playerId << "'s AOI" << std::endl;
        }
    }

    void notifyEntityLeave(uint64_t playerId, uint64_t entityId) {
        std::cout << "[" << name_ << "] Entity " << entityId
                  << " left " << playerId << "'s AOI" << std::endl;
    }

    void broadcastMove(uint64_t playerId, const OnlinePlayer& player) {
        // 实际实现会广播给周围玩家
    }

    void broadcastChat(uint64_t playerId, const std::string& name,
                      const std::string& msg) {
        // 实际实现会广播给附近玩家
    }

    std::unordered_map<uint64_t, OnlinePlayer> players_;
};

//==============================================================================
// 服务器集群管理
//==============================================================================

class ServerCluster {
public:
    static ServerCluster& instance() {
        static ServerCluster cluster;
        return cluster;
    }

    void initialize() {
        std::cout << "=== Initializing Server Cluster ===" << std::endl;

        // 创建服务器
        loginServer_ = std::make_unique<LoginServer>(7001);
        gatewayServer_ = std::make_unique<GatewayServer>(7002);
        gameServer_ = std::make_unique<GameServer>(7003);

        // 启动服务器
        loginServer_->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        gatewayServer_->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        gameServer_->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::cout << "=== Server Cluster Ready ===" << std::endl;
    }

    void shutdown() {
        std::cout << "=== Shutting Down Server Cluster ===" << std::endl;

        if (gameServer_) gameServer_->stop();
        if (gatewayServer_) gatewayServer_->stop();
        if (loginServer_) loginServer_->stop();

        std::cout << "=== Server Cluster Stopped ===" << std::endl;
    }

    LoginServer* getLoginServer() { return loginServer_.get(); }
    GatewayServer* getGatewayServer() { return gatewayServer_.get(); }
    GameServer* getGameServer() { return gameServer_.get(); }

private:
    ServerCluster() = default;

    std::unique_ptr<LoginServer> loginServer_;
    std::unique_ptr<GatewayServer> gatewayServer_;
    std::unique_ptr<GameServer> gameServer_;
};

//==============================================================================
// 模拟客户端
//==============================================================================

class MockClient {
public:
    MockClient(const std::string& account)
        : account_(account), playerId_(0), sessionId_(0) {}

    void connect() {
        std::cout << "[Client] Connecting to gateway..." << std::endl;
        sessionId_ = cluster_.getGatewayServer()->onClientConnect("127.0.0.1");
    }

    void login(const std::string& password) {
        std::cout << "[Client] Sending login request..." << std::endl;

        LoginRequest req;
        std::snprintf(req.account, sizeof(req.account), "%s", account_.c_str());
        std::snprintf(req.password, sizeof(req.password), "%s", password.c_str());
        std::snprintf(req.deviceId, sizeof(req.deviceId), "device_001");

        cluster_.getLoginServer()->handleLogin(sessionId_, req);

        // 模拟响应
        playerId_ = 1001;  // 简化处理

        // 通知网关认证成功
        cluster_.getGatewayServer()->onAuthenticated(sessionId_, playerId_);

        // 进入游戏
        enterGame();
    }

    void enterGame() {
        std::cout << "[Client] Entering game..." << std::endl;
        cluster_.getGameServer()->onPlayerEnter(playerId_);
    }

    void move(float x, float y, float z) {
        std::cout << "[Client] Requesting move to (" << x << ", " << y << ", " << z << ")" << std::endl;

        MoveRequest req{x, y, z};
        cluster_.getGameServer()->onMoveRequest(playerId_, req);
    }

    void chat(const std::string& msg) {
        std::cout << "[Client] Sending chat: " << msg << std::endl;
        cluster_.getGameServer()->onChatMessage(playerId_, msg);
    }

    void disconnect() {
        std::cout << "[Client] Disconnecting..." << std::endl;
        if (sessionId_ > 0) {
            cluster_.getGatewayServer()->onClientDisconnect(sessionId_);
        }
    }

private:
    std::string account_;
    uint64_t playerId_;
    uint64_t sessionId_;
    ServerCluster& cluster_ = ServerCluster::instance();
};

//==============================================================================
// 示例场景
//==============================================================================

void scenario1_SinglePlayerLogin() {
    std::cout << "\n=== Scenario 1: Single Player Login ===" << std::endl;

    MockClient client("player001");

    client.connect();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client.login("password123");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client.move(105.0f, 0.0f, 105.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client.chat("Hello everyone!");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client.disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void scenario2_Multiplayer() {
    std::cout << "\n=== Scenario 2: Multiple Players ===" << std::endl;

    std::vector<std::unique_ptr<MockClient>> clients;

    // 创建多个玩家
    for (int i = 0; i < 5; ++i) {
        std::string account = "player00" + std::to_string(i + 1);
        auto client = std::make_unique<MockClient>(account);

        client->connect();
        client->login("password123");
        client->move(100.0f + i * 5, 0.0f, 100.0f + i * 5);

        clients.push_back(std::move(client));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // 玩家聊天
    clients[0]->chat("Let's form a team!");
    clients[1]->chat("Count me in!");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 断开连接
    for (auto& client : clients) {
        client->disconnect();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void scenario3_AoiDemo() {
    std::cout << "\n=== Scenario 3: AOI Demonstration ===" << std::endl;

    std::vector<std::unique_ptr<MockClient>> clients;

    // 创建中心玩家
    auto centerPlayer = std::make_unique<MockClient>("center");
    centerPlayer->connect();
    centerPlayer->login("password123");
    centerPlayer->move(100.0f, 0.0f, 100.0f);

    // 创建周围玩家（在视野内）
    for (int i = 0; i < 3; ++i) {
        auto client = std::make_unique<MockClient>("near_" + std::to_string(i));
        client->connect();
        client->login("password123");
        client->move(110.0f + i * 5, 0.0f, 110.0f + i * 5);
        clients.push_back(std::move(client));
    }

    // 创建远处玩家（在视野外）
    for (int i = 0; i < 2; ++i) {
        auto client = std::make_unique<MockClient>("far_" + std::to_string(i));
        client->connect();
        client->login("password123");
        client->move(200.0f + i * 10, 0.0f, 200.0f + i * 10);
        clients.push_back(std::move(client));
    }

    std::cout << "\n[Center player moving to check AOI changes...]" << std::endl;

    // 中心玩家移动
    centerPlayer->move(120.0f, 0.0f, 120.0f);
    centerPlayer->move(200.0f, 0.0f, 200.0f);  // 应该能看到远处的玩家了

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 清理
    centerPlayer->disconnect();
    for (auto& client : clients) {
        client->disconnect();
    }
}

//==============================================================================
// 主程序
//==============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "=== Apollo Game Server Demo ===" << std::endl;
    std::cout << "========================================" << std::endl;

    // 初始化服务器集群
    ServerCluster::instance().initialize();

    // 运行场景
    scenario1_SinglePlayerLogin();
    scenario2_Multiplayer();
    scenario3_AoiDemo();

    // 关闭服务器集群
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    ServerCluster::instance().shutdown();

    std::cout << "\n=== Demo Complete ===" << std::endl;

    return 0;
}

} // namespace demo
} // namespace apollo

int main() {
    return apollo::demo::main();
}
