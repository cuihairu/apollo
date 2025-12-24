/**
 * @file game_server_template.cpp
 * @brief 完整的游戏服务器模板
 *
 * 可直接使用或作为开发起点
 */

#include <iostream>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <chrono>

using namespace std;

//==============================================================================
// 基础类型定义
//==============================================================================

using PlayerId = uint64_t;
using SessionId = uint64_t;

/// 消息类型
enum class MsgType : uint16_t {
    // 心跳
    CS_PING = 1,
    SC_PONG = 2,

    // 登录
    CS_LOGIN = 100,
    SC_LOGIN_RESULT = 101,

    // 玩家移动
    CS_MOVE = 200,
    SC_MOVE_NOTIFY = 201,

    // 聊天
    CS_CHAT = 300,
    SC_CHAT_NOTIFY = 301,

    // 战斗
    CS_ATTACK = 400,
    SC_ATTACK_RESULT = 401
};

/// 消息头
#pragma pack(push, 1)
struct MessageHead {
    MsgType type;
    uint16_t length;
};
#pragma pack(pop)

/// 登录请求
struct LoginRequest {
    char account[32];
    char password[32];
};

/// 登录响应
struct LoginResult {
    uint32_t resultCode;
    PlayerId playerId;
    char token[64];
};

/// 移动请求
struct MoveRequest {
    float x, y, z;
};

/// 移动通知
struct MoveNotify {
    PlayerId playerId;
    float x, y, z;
};

/// 聊天消息
struct ChatMessage {
    uint8_t channel;      // 0=世界 1=私聊 2=公会
    PlayerId senderId;
    char content[128];
};

/// 攻击请求
struct AttackRequest {
    PlayerId targetId;
    uint32_t skillId;
};

/// 攻击结果
struct AttackResult {
    PlayerId attackerId;
    PlayerId targetId;
    uint32_t damage;
    uint32_t currentHp;
};

//==============================================================================
// 玩家数据
//==============================================================================

struct Player {
    PlayerId id;
    SessionId sessionId;
    char name[32];
    uint32_t level;
    uint32_t exp;
    uint32_t hp;
    uint32_t maxHp;
    float x, y, z;
    uint64_t lastHeartbeat;

    Player() : id(0), sessionId(0), level(1), exp(0), hp(100), maxHp(100),
               x(0), y(0), z(0), lastHeartbeat(0) {
        memset(name, 0, sizeof(name));
    }
};

//==============================================================================
// 服务器配置
//==============================================================================

struct ServerConfig {
    string host;
    uint16_t port;
    int threadCount;
    int maxPlayers;

    ServerConfig()
        : host("0.0.0.0"), port(8080)
        , threadCount(4), maxPlayers(1000) {}
};

//==============================================================================
// 环形缓冲区实现
//==============================================================================

template<typename T, size_t N>
class RingBuffer {
public:
    RingBuffer() : head_(0), tail_(0), size_(0) {}

    bool push(const T& item) {
        if (size_ >= N) return false;
        buffer_[tail_] = item;
        tail_ = (tail_ + 1) % N;
        ++size_;
        return true;
    }

    bool pop(T& item) {
        if (size_ == 0) return false;
        item = buffer_[head_];
        head_ = (head_ + 1) % N;
        --size_;
        return true;
    }

    bool empty() const { return size_ == 0; }
    size_t size() const { return size_; }
    size_t capacity() const { return N; }

private:
    T buffer_[N];
    size_t head_;
    size_t tail_;
    size_t size_;
};

//==============================================================================
// 消息队列
//==============================================================================

struct NetworkMessage {
    SessionId sessionId;
    MsgType type;
    std::vector<uint8_t> data;
};

class MessageQueue {
public:
    void push(SessionId sessionId, MsgType type, const void* data, size_t length) {
        lock_guard<mutex> lock(mutex_);

        NetworkMessage msg;
        msg.sessionId = sessionId;
        msg.type = type;
        if (data && length > 0) {
            msg.data.assign(static_cast<const uint8_t*>(data),
                          static_cast<const uint8_t*>(data) + length);
        }

        queue_.push(msg);
        cv_.notify_one();
    }

    bool pop(NetworkMessage& msg, int timeoutMs = 0) {
        unique_lock<mutex> lock(mutex_);

        if (timeoutMs > 0) {
            if (!cv_.wait_for(lock, chrono::milliseconds(timeoutMs),
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
        lock_guard<mutex> lock(mutex_);
        return queue_.size();
    }

private:
    queue<NetworkMessage> queue_;
    mutable mutex mutex_;
    condition_variable cv_;
};

//==============================================================================
// 玩家管理器
//==============================================================================

class PlayerManager {
public:
    Player* addPlayer(SessionId sessionId, const std::string& name) {
        lock_guard<mutex> lock(mutex_);

        PlayerId playerId = nextPlayerId_++;

        Player& player = players_[playerId];
        player.id = playerId;
        player.sessionId = sessionId;
        strncpy(player.name, name.c_str(), sizeof(player.name) - 1);
        player.x = 100.0f;
        player.y = 0.0f;
        player.z = 100.0f;

        sessionIdToPlayerId_[sessionId] = playerId;

        cout << "[PlayerManager] Player " << name << " (ID:" << playerId << ") added" << endl;

        return &player;
    }

    void removePlayer(SessionId sessionId) {
        lock_guard<mutex> lock(mutex_);

        auto it = sessionIdToPlayerId_.find(sessionId);
        if (it != sessionIdToPlayerId_.end()) {
            PlayerId playerId = it->second;
            cout << "[PlayerManager] Player ID:" << playerId << " removed" << endl;
            players_.erase(playerId);
            sessionIdToPlayerId_.erase(it);
        }
    }

    Player* getPlayer(PlayerId playerId) {
        lock_guard<mutex> lock(mutex_);
        auto it = players_.find(playerId);
        return (it != players_.end()) ? &it->second : nullptr;
    }

    Player* getPlayerBySession(SessionId sessionId) {
        lock_guard<mutex> lock(mutex_);
        auto it = sessionIdToPlayerId_.find(sessionId);
        if (it != sessionIdToPlayerId_.end()) {
            return &players_[it->second];
        }
        return nullptr;
    }

    size_t getPlayerCount() const {
        lock_guard<mutex> lock(mutex_);
        return players_.size();
    }

    void broadcast(const void* data, size_t length, SessionId exceptSession = 0) {
        lock_guard<mutex> lock(mutex_);

        for (auto& pair : players_) {
            if (pair.second.sessionId != exceptSession) {
                // 发送消息给玩家
                // 实际实现会通过网络发送
            }
        }
    }

private:
    unordered_map<PlayerId, Player> players_;
    unordered_map<SessionId, PlayerId> sessionIdToPlayerId_;
    PlayerId nextPlayerId_ = 1001;
    mutable mutex mutex_;
};

//==============================================================================
// AOI (兴趣区域) - 简化实现
//==============================================================================

class AOIManager {
public:
    struct Entity {
        PlayerId playerId;
        float x, y, z;
        float range;
    };

    void updateEntity(PlayerId playerId, float x, float y, float z, float range = 50.0f) {
        lock_guard<mutex> lock(mutex_);

        Entity& entity = entities_[playerId];
        entity.playerId = playerId;
        entity.x = x;
        entity.y = y;
        entity.z = z;
        entity.range = range;
    }

    vector<PlayerId> query(float centerX, float centerZ, float radius) {
        lock_guard<mutex> lock(mutex_);

        vector<PlayerId> result;

        for (const auto& pair : entities_) {
            const Entity& entity = pair.second;

            float dx = entity.x - centerX;
            float dz = entity.z - centerZ;
            float dist = sqrtf(dx * dx + dz * dz);

            if (dist <= radius) {
                result.push_back(entity.playerId);
            }
        }

        return result;
    }

    void removeEntity(PlayerId playerId) {
        lock_guard<mutex> lock(mutex_);
        entities_.erase(playerId);
    }

private:
    unordered_map<PlayerId, Entity> entities_;
    mutable mutex mutex_;
};

//==============================================================================
// 网络会话
//==============================================================================

class Session {
public:
    Session(SessionId id, const string& addr)
        : id_(id), address_(addr), active_(true) {
    }

    SessionId getId() const { return id_; }
    const string& getAddress() const { return address_; }
    bool isActive() const { return active_; }

    void send(MsgType type, const void* data, size_t length) {
        lock_guard<mutex> lock(mutex_);

        MessageHead head;
        head.type = type;
        head.length = static_cast<uint16_t>(length);

        // 实际实现会通过网络发送
        // 这里模拟发送
        sentBytes_ += sizeof(head) + length;
        ++sentPackets_;
    }

    void close() {
        active_ = false;
    }

    uint64_t getSentBytes() const { return sentBytes_; }
    uint32_t getSentPackets() const { return sentPackets_; }

private:
    SessionId id_;
    string address_;
    bool active_;
    uint64_t sentBytes_ = 0;
    uint32_t sentPackets_ = 0;
    mutable mutex mutex_;
};

//==============================================================================
// 网络管理器
//==============================================================================

class NetworkManager {
public:
    NetworkManager(const ServerConfig& config)
        : config_(config), running_(false), nextSessionId_(1) {}

    bool start() {
        cout << "[Network] Starting on " << config_.host << ":" << config_.port << endl;

        running_ = true;

        // 启动接收线程
        recvThread_ = thread(&NetworkManager::receiveLoop, this);

        // 启动处理线程
        for (int i = 0; i < config_.threadCount; ++i) {
            workers_.emplace_back(&NetworkManager::workerLoop, this);
        }

        return true;
    }

    void stop() {
        cout << "[Network] Stopping..." << endl;

        running_ = false;

        // 唤醒所有工作线程
        cv_.notify_all();

        if (recvThread_.joinable()) {
            recvThread_.join();
        }

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        // 关闭所有会话
        lock_guard<mutex> lock(mutex_);
        sessions_.clear();
    }

    SessionId createSession(const string& address) {
        lock_guard<mutex> lock(mutex_);

        SessionId id = nextSessionId_++;
        auto session = make_unique<Session>(id, address);
        sessions_[id] = std::move(session);

        cout << "[Network] Session " << id << " created from " << address << endl;

        return id;
    }

    void removeSession(SessionId sessionId) {
        lock_guard<mutex> lock(mutex_);

        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            it->second->close();
            cout << "[Network] Session " << sessionId << " removed" << endl;
            sessions_.erase(it);
        }
    }

    Session* getSession(SessionId sessionId) {
        lock_guard<mutex> lock(mutex_);
        auto it = sessions_.find(sessionId);
        return (it != sessions_.end()) ? it->second.get() : nullptr;
    }

    size_t getSessionCount() const {
        lock_guard<mutex> lock(mutex_);
        return sessions_.size();
    }

    MessageQueue& getMessageQueue() { return messageQueue_; }

private:
    void receiveLoop() {
        // 模拟接收网络数据
        while (running_) {
            // 实际实现会从 socket 读取
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }

    void workerLoop() {
        NetworkMessage msg;

        while (running_) {
            if (!messageQueue_.pop(msg, 100)) {
                continue;
            }

            handleMessage(msg);
        }
    }

    void handleMessage(const NetworkMessage& msg) {
        switch (msg.type) {
            case MsgType::CS_LOGIN:
                handleLogin(msg);
                break;

            case MsgType::CS_MOVE:
                handleMove(msg);
                break;

            case MsgType::CS_CHAT:
                handleChat(msg);
                break;

            case MsgType::CS_ATTACK:
                handleAttack(msg);
                break;

            case MsgType::CS_PING:
                handlePing(msg);
                break;

            default:
                break;
        }
    }

    void handleLogin(const NetworkMessage& msg) {
        if (msg.data.size() < sizeof(LoginRequest)) {
            return;
        }

        auto* req = reinterpret_cast<const LoginRequest*>(msg.data.data());

        // 验证账号（简化）
        if (strlen(req->account) < 3) {
            sendLoginResult(msg.sessionId, 1, 0);
            return;
        }

        // 创建玩家
        auto* player = playerManager_.addPlayer(msg.sessionId, req->account);

        sendLoginResult(msg.sessionId, 0, player->id);

        cout << "[Network] Player " << req->account << " logged in from session "
                  << msg.sessionId << endl;
    }

    void handleMove(const NetworkMessage& msg) {
        if (msg.data.size() < sizeof(MoveRequest)) {
            return;
        }

        auto* req = reinterpret_cast<const MoveRequest*>(msg.data.data());

        auto* player = playerManager_.getPlayerBySession(msg.sessionId);
        if (!player) {
            return;
        }

        // 更新位置
        player->x = req->x;
        player->y = req->y;
        player->z = req->z;

        // 更新 AOI
        aoiManager_.updateEntity(player->id, req->x, req->y, req->z);

        // 广播移动给附近玩家
        broadcastMove(player->id, req->x, req->y, req->z);
    }

    void handleChat(const NetworkMessage& msg) {
        if (msg.data.size() < sizeof(ChatMessage)) {
            return;
        }

        auto* chatMsg = reinterpret_cast<const ChatMessage*>(msg.data.data());

        auto* sender = playerManager_.getPlayerBySession(msg.sessionId);
        if (!sender) {
            return;
        }

        cout << "[Chat] " << sender->name << ": " << chatMsg->content << endl;

        // 广播聊天消息
        broadcastChat(sender->id, sender->name, chatMsg->content, chatMsg->channel);
    }

    void handleAttack(const NetworkMessage& msg) {
        if (msg.data.size() < sizeof(AttackRequest)) {
            return;
        }

        auto* req = reinterpret_cast<const AttackRequest*>(msg.data.data());

        auto* attacker = playerManager_.getPlayerBySession(msg.sessionId);
        if (!attacker) {
            return;
        }

        auto* target = playerManager_.getPlayer(req->targetId);
        if (!target) {
            return;
        }

        // 计算伤害
        uint32_t damage = 10 + attacker->level;
        target->hp = std::max(0, static_cast<int>(target->hp - damage));

        cout << "[Combat] " << attacker->name << " attacks " << target->name
                  << " for " << damage << " damage!" << endl;

        // 发送攻击结果
        sendAttackResult(msg.sessionId, attacker->id, target->id, damage, target->hp);
    }

    void handlePing(const NetworkMessage& msg) {
        MessageHead head;
        head.type = MsgType::SC_PONG;
        head.length = 0;

        auto* session = getSession(msg.sessionId);
        if (session) {
            session->send(MsgType::SC_PONG, &head, sizeof(head));
        }
    }

    void sendLoginResult(SessionId sessionId, uint32_t result, PlayerId playerId) {
        LoginResult response;
        response.resultCode = result;
        response.playerId = playerId;
        memset(response.token, 0, sizeof(response.token));

        if (result == 0) {
            snprintf(response.token, sizeof(response.token), "token_%llu", playerId);
        }

        auto* session = getSession(sessionId);
        if (session) {
            session->send(MsgType::SC_LOGIN_RESULT, &response, sizeof(response));
        }
    }

    void broadcastMove(PlayerId playerId, float x, float y, float z) {
        auto* player = playerManager_.getPlayer(playerId);
        if (!player) {
            return;
        }

        MoveNotify notify;
        notify.playerId = playerId;
        notify.x = x;
        notify.y = y;
        notify.z = z;

        playerManager_.broadcast(&notify, sizeof(notify), 0);
    }

    void broadcastChat(PlayerId senderId, const char* senderName,
                      const char* content, uint8_t channel) {
        playerManager_.broadcast(content, strlen(content), 0);
    }

    void sendAttackResult(SessionId sessionId, PlayerId attackerId, PlayerId targetId,
                          uint32_t damage, uint32_t currentHp) {
        AttackResult result;
        result.attackerId = attackerId;
        result.targetId = targetId;
        result.damage = damage;
        result.currentHp = currentHp;

        auto* session = getSession(sessionId);
        if (session) {
            session->send(MsgType::SC_ATTACK_RESULT, &result, sizeof(result));
        }
    }

    ServerConfig config_;
    atomic<bool> running_;
    SessionId nextSessionId_;
    unordered_map<SessionId, unique_ptr<Session>> sessions_;
    MessageQueue messageQueue_;
    vector<thread> workers_;
    thread recvThread_;

    PlayerManager playerManager_;
    AOIManager aoiManager_;

    mutable mutex mutex_;
    condition_variable cv_;
};

//==============================================================================
// 游戏服务器
//==============================================================================

class GameServer {
public:
    GameServer(const ServerConfig& config)
        : config_(config), running_(false), network_(config) {}

    bool start() {
        cout << "========================================" << endl;
        cout << "=== Game Server Starting ===" << endl;
        cout << "========================================" << endl;

        // 初始化网络
        if (!network_.start()) {
            return false;
        }

        running_ = true;

        // 启动定时器线程
        timerThread_ = thread(&GameServer::timerLoop, this);

        cout << "\n*** Server is running! ***" << endl;
        cout << "Press Ctrl+C to stop\n" << endl;

        return true;
    }

    void stop() {
        if (!running_) {
            return;
        }

        cout << "\n*** Shutting down... ***" << endl;

        running_ = false;

        network_.stop();

        if (timerThread_.joinable()) {
            timerThread_.join();
        }

        cout << "========================================" << endl;
        cout << "=== Server Stopped ===" << endl;
        cout << "========================================" << endl;
    }

    void run() {
        while (running_) {
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }

    bool isRunning() const { return running_; }

    NetworkManager& getNetwork() { return network_; }

private:
    void timerLoop() {
        while (running_) {
            // 每秒执行一次定时任务
            this_thread::sleep_for(chrono::seconds(1));

            // 检查心跳超时
            checkHeartbeats();

            // 更新统计
            printStats();
        }
    }

    void checkHeartbeats() {
        // 检查所有玩家的心跳
        // 实际实现会将超时玩家踢出
    }

    void printStats() {
        static int count = 0;
        if (++count % 60 == 0) {  // 每60秒
            cout << "\n[Stats] Online players: "
                      << network_.getSessionCount()
                      << ", Sessions: " << network_.getSessionCount() << endl;
        }
    }

    ServerConfig config_;
    NetworkManager network_;
    atomic<bool> running_;
    thread timerThread_;
};

//==============================================================================
// 模拟客户端
//==============================================================================

class MockClient {
public:
    MockClient(const string& name) : name_(name), sessionId_(0) {
    }

    void connect(const string& host, uint16_t port) {
        cout << "[" << name_ << "] Connecting to " << host << ":" << port << "..." << endl;

        // 模拟连接
        sessionId_ = 1000 + rand() % 1000;

        cout << "[" << name_ << "] Connected! Session ID: " << sessionId_ << endl;
    }

    void login(const string& account, const string& password) {
        cout << "[" << name_ << "] Logging in as " << account << "..." << endl;

        LoginRequest req;
        strncpy(req.account, account.c_str(), sizeof(req.account) - 1);
        strncpy(req.password, password.c_str(), sizeof(req.password) - 1);

        // 模拟发送登录请求
    }

    void move(float x, float y, float z) {
        MoveRequest req{x, y, z};
        // 模拟发送移动请求
        cout << "[" << name_ << "] Moving to (" << x << ", " << y << ", " << z << ")" << endl;
    }

    void chat(const string& message) {
        ChatMessage msg;
        msg.channel = 0;
        msg.senderId = 0;
        strncpy(msg.content, message.c_str(), sizeof(msg.content) - 1);

        cout << "[" << name_ << "] Chat: " << message << endl;
    }

private:
    string name_;
    SessionId sessionId_;
};

//==============================================================================
// 示例：完整游戏循环
//==============================================================================

void example_CompleteGameLoop() {
    cout << "\n=== Complete Game Loop Example ===" << endl;

    ServerConfig config;
    config.host = "0.0.0.0";
    config.port = 8080;
    config.threadCount = 2;
    config.maxPlayers = 100;

    GameServer server(config);

    if (!server.start()) {
        cout << "Failed to start server!" << endl;
        return;
    }

    // 模拟多个客户端连接
    vector<unique_ptr<MockClient>> clients;

    // 连接5个玩家
    for (int i = 0; i < 5; ++i) {
        auto client = make_unique<MockClient>("Player" + to_string(i + 1));
        client->connect("127.0.0.1", 8080);
        client->login("player" + to_string(i + 1), "pass123");
        clients.push_back(std::move(client));

        this_thread::sleep_for(chrono::milliseconds(100));
    }

    // 模拟游戏活动
    cout << "\n--- Simulating Game Activity ---" << endl;

    for (int i = 0; i < 10; ++i) {
        for (auto& client : clients) {
            float x = 100 + (rand() % 50);
            float z = 100 + (rand() % 50);
            client->move(x, 0, z);

            if (rand() % 3 == 0) {
                client->chat("Hello everyone!");
            }

            this_thread::sleep_for(chrono::milliseconds(50));
        }
    }

    // 运行服务器一段时间
    cout << "\nServer running for 3 seconds..." << endl;
    this_thread::sleep_for(chrono::seconds(3));

    server.stop();
}

//==============================================================================
// 主程序
//==============================================================================

int main() {
    cout << "========================================" << endl;
    cout << "=== Game Server Template ===" << endl;
    cout << "========================================" << endl;

    srand(static_cast<uint32_t>(time(nullptr)));

    example_CompleteGameLoop();

    cout << "\n=== Template Demo Complete ===" << endl;

    return 0;
}
