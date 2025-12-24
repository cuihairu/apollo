/**
 * @file rpc_demo.cpp
 * @brief RPC 网络通信示例
 *
 * 演示如何使用 Apollo RPC 系统进行服务间通信
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>

// 模拟会话和消息接口（实际使用时引用真实头文件）
namespace apollo {
namespace net {

// 枚举定义
enum class SessionState : uint8_t {
    Disconnected = 0,
    Connecting = 1,
    Connected = 2,
    Authenticated = 3
};

enum class RpcMsgType : uint8_t {
    Request = 1,
    Response = 2,
    Error = 3,
    OneWay = 4
};

enum class RpcErrorCode : int32_t {
    Success = 0,
    Timeout = -1,
    NotFound = -2,
    InvalidRequest = -3,
    InternalError = -4
};

// RPC 头
struct RpcHeader {
    uint32_t requestId;
    uint16_t serviceId;
    uint16_t methodId;
    RpcMsgType type;
    int32_t errorCode;

    static constexpr size_t SIZE = 13;

    RpcHeader() : requestId(0), serviceId(0), methodId(0),
                  type(RpcMsgType::Request), errorCode(0) {}

    bool encode(uint8_t* buffer) const {
        uint8_t* p = buffer;
        *p++ = (requestId >> 24) & 0xFF;
        *p++ = (requestId >> 16) & 0xFF;
        *p++ = (requestId >> 8) & 0xFF;
        *p++ = requestId & 0xFF;
        *p++ = (serviceId >> 8) & 0xFF;
        *p++ = serviceId & 0xFF;
        *p++ = (methodId >> 8) & 0xFF;
        *p++ = methodId & 0xFF;
        *p++ = static_cast<uint8_t>(type);
        *p++ = (errorCode >> 24) & 0xFF;
        *p++ = (errorCode >> 16) & 0xFF;
        *p++ = (errorCode >> 8) & 0xFF;
        *p++ = errorCode & 0xFF;
        return true;
    }

    bool decode(const uint8_t* buffer) {
        const uint8_t* p = buffer;
        requestId = (static_cast<uint32_t>(p[0]) << 24) |
                     (static_cast<uint32_t>(p[1]) << 16) |
                     (static_cast<uint32_t>(p[2]) << 8) |
                     static_cast<uint32_t>(p[3]);
        p += 4;
        serviceId = (static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]);
        p += 2;
        methodId = (static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]);
        p += 2;
        type = static_cast<RpcMsgType>(p[0]);
        p += 1;
        errorCode = (static_cast<int32_t>(p[0]) << 24) |
                    (static_cast<int32_t>(p[1]) << 16) |
                    (static_cast<int32_t>(p[2]) << 8) |
                    static_cast<int32_t>(p[3]);
        return true;
    }
};

// 模拟会话接口
class ISession {
public:
    virtual ~ISession() = default;
    virtual uint64_t getSessionId() const = 0;
    virtual uint64_t getPlayerId() const = 0;
    virtual void setPlayerId(uint64_t playerId) = 0;
    virtual SessionState getState() const = 0;
    virtual bool send(const void* data, size_t length) = 0;
    virtual bool send(const std::vector<uint8_t>& data) = 0;
    virtual void close(int reason = 0) = 0;
};

// 模拟会话实现
class MockSession : public ISession {
public:
    MockSession(uint64_t sessionId) : sessionId_(sessionId),
                                       playerId_(0),
                                       state_(SessionState::Connected) {}

    uint64_t getSessionId() const override { return sessionId_; }
    uint64_t getPlayerId() const override { return playerId_; }
    void setPlayerId(uint64_t playerId) override { playerId_ = playerId; }
    SessionState getState() const override { return state_; }

    bool send(const void* data, size_t length) override {
        if (data && length > 0) {
            sendBuffer_.insert(sendBuffer_.end(),
                              static_cast<const uint8_t*>(data),
                              static_cast<const uint8_t*>(data) + length);
            return true;
        }
        return false;
    }

    bool send(const std::vector<uint8_t>& data) override {
        return send(data.data(), data.size());
    }

    void close(int reason) override {
        state_ = SessionState::Disconnected;
    }

    // 获取发送的数据
    const std::vector<uint8_t>& getSendBuffer() const { return sendBuffer_; }
    void clearSendBuffer() { sendBuffer_.clear(); }

    // 模拟接收数据
    void simulateReceive(const std::vector<uint8_t>& data) {
        recvBuffer_.insert(recvBuffer_.end(), data.begin(), data.end());
    }

private:
    uint64_t sessionId_;
    uint64_t playerId_;
    SessionState state_;
    std::vector<uint8_t> sendBuffer_;
    std::vector<uint8_t> recvBuffer_;
};

} // namespace net
} // namespace apollo

//==============================================================================
// 服务定义
//==============================================================================

// 服务 ID 定义
constexpr uint16_t PLAYER_SERVICE = 1;
constexpr uint16_t CHAT_SERVICE = 2;
constexpr uint16_t GUILD_SERVICE = 3;
constexpr uint16_t INVENTORY_SERVICE = 4;

// 方法 ID 定义
namespace PlayerMethod {
    constexpr uint16_t GET_INFO = 1;
    constexpr uint16_t SET_NAME = 2;
    constexpr uint16_t LEVEL_UP = 3;
}

namespace ChatMethod {
    constexpr uint16_t SEND_MESSAGE = 1;
    constexpr uint16_t GET_HISTORY = 2;
}

namespace GuildMethod {
    constexpr uint16_t CREATE = 1;
    constexpr uint16_t JOIN = 2;
    constexpr uint16_t LEAVE = 3;
    constexpr uint16_t GET_MEMBERS = 4;
}

//==============================================================================
// 数据结构定义
//==============================================================================

#pragma pack(push, 1)

struct PlayerInfo {
    uint64_t playerId;
    uint32_t level;
    uint64_t exp;
    uint16_t hp;
    uint16_t maxHp;
    char name[32];

    PlayerInfo() : playerId(0), level(1), exp(0), hp(100), maxHp(100) {
        std::memset(name, 0, sizeof(name));
    }
};

struct SendMessageRequest {
    uint64_t senderId;
    uint8_t channel;     // 0=world, 1=guild, 2=private
    uint64_t targetId;    // for private messages
    char message[128];

    SendMessageRequest() : senderId(0), channel(0), targetId(0) {
        std::memset(message, 0, sizeof(message));
    }
};

struct SendMessageResponse {
    bool success;
    uint64_t timestamp;

    SendMessageResponse() : success(false), timestamp(0) {}
};

#pragma pack(pop)

//==============================================================================
// RPC 客户端模拟
//==============================================================================

class RpcClient {
public:
    // 同步调用
    template<typename Request, typename Response>
    bool call(apollo::net::ISession* session,
              uint16_t serviceId, uint16_t methodId,
              const Request& request, Response& response,
              int timeoutMs = 1000) {

        // 编码请求
        std::vector<uint8_t> packet = encodeRequest(serviceId, methodId,
                                                      &request, sizeof(request));

        // 发送
        if (!session->send(packet)) {
            return false;
        }

        // 模拟网络延迟和服务处理
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // 处理响应（模拟）
        return handleResponse(serviceId, methodId, &request, sizeof(request),
                             &response, sizeof(response));
    }

    // 异步调用
    template<typename Request>
    void asyncCall(apollo::net::ISession* session,
                   uint16_t serviceId, uint16_t methodId,
                   const Request& request,
                   std::function<void(bool)> callback) {

        std::thread([this, session, serviceId, methodId, request, callback]() {
            int dummyResponse = 0;
            bool result = call(session, serviceId, methodId, request, dummyResponse, 1000);
            callback(result);
        }).detach();
    }

private:
    std::vector<uint8_t> encodeRequest(uint16_t serviceId, uint16_t methodId,
                                       const void* data, size_t length) {
        std::vector<uint8_t> packet(apollo::net::RpcHeader::SIZE + length);

        apollo::net::RpcHeader header;
        header.requestId = ++nextRequestId_;
        header.serviceId = serviceId;
        header.methodId = methodId;
        header.type = apollo::net::RpcMsgType::Request;
        header.errorCode = 0;

        header.encode(packet.data());
        if (data && length > 0) {
            std::memcpy(packet.data() + apollo::net::RpcHeader::SIZE, data, length);
        }

        return packet;
    }

    template<typename Request, typename Response>
    bool handleResponse(uint16_t serviceId, uint16_t methodId,
                       const Request* request, size_t requestLength,
                       Response* response, size_t responseLength) {

        // 模拟服务端处理
        if (serviceId == PLAYER_SERVICE) {
            return handlePlayerService(methodId, request, response);
        } else if (serviceId == CHAT_SERVICE) {
            return handleChatService(methodId, request, response);
        }

        return false;
    }

    bool handlePlayerService(uint16_t methodId,
                             const void* request, void* response) {
        switch (methodId) {
            case PlayerMethod::GET_INFO: {
                auto* req = static_cast<const uint32_t*>(request);  // playerId
                auto* resp = static_cast<PlayerInfo*>(response);

                resp->playerId = *req;
                resp->level = 50;
                resp->exp = 125000;
                resp->hp = 950;
                resp->maxHp = 1000;
                std::snprintf(resp->name, sizeof(resp->name), "Hero%u", *req);
                return true;
            }
            case PlayerMethod::LEVEL_UP: {
                auto* resp = static_cast<PlayerInfo*>(response);
                resp->playerId = 1001;
                resp->level = 51;
                resp->exp = 0;
                resp->hp = 1000;
                resp->maxHp = 1050;
                std::snprintf(resp->name, sizeof(resp->name), "Hero1001");
                return true;
            }
            default:
                return false;
        }
    }

    bool handleChatService(uint16_t methodId,
                           const void* request, void* response) {
        switch (methodId) {
            case ChatMethod::SEND_MESSAGE: {
                auto* req = static_cast<const SendMessageRequest*>(request);
                auto* resp = static_cast<SendMessageResponse*>(response);

                std::cout << "    [Chat] Player " << req->senderId
                          << " sends: " << req->message << std::endl;

                resp->success = true;
                resp->timestamp = std::chrono::system_clock::now().time_since_epoch().count();
                return true;
            }
            default:
                return false;
        }
    }

    uint32_t nextRequestId_ = 0;
};

//==============================================================================
// 示例场景
//==============================================================================

void example1_GetPlayerInfo() {
    std::cout << "\n=== Example 1: Get Player Info ===" << std::endl;

    apollo::net::MockSession session(1);
    session.setPlayerId(1001);

    RpcClient client;

    // 调用获取玩家信息
    uint64_t playerId = 1001;
    PlayerInfo playerInfo;

    if (client.call(&session, PLAYER_SERVICE, PlayerMethod::GET_INFO,
                     playerId, playerInfo)) {
        std::cout << "  Player Info:" << std::endl;
        std::cout << "    ID: " << playerInfo.playerId << std::endl;
        std::cout << "    Name: " << playerInfo.name << std::endl;
        std::cout << "    Level: " << playerInfo.level << std::endl;
        std::cout << "    HP: " << playerInfo.hp << "/" << playerInfo.maxHp << std::endl;
    } else {
        std::cout << "  Failed to get player info" << std::endl;
    }
}

void example2_LevelUp() {
    std::cout << "\n=== Example 2: Level Up ===" << std::endl;

    apollo::net::MockSession session(1);
    session.setPlayerId(1001);

    RpcClient client;

    // 调用升级
    uint32_t dummy = 0;
    PlayerInfo newInfo;

    if (client.call(&session, PLAYER_SERVICE, PlayerMethod::LEVEL_UP,
                     dummy, newInfo)) {
        std::cout << "  Level Up Success!" << std::endl;
        std::cout << "    New Level: " << newInfo.level << std::endl;
        std::cout << "    New Max HP: " << newInfo.maxHp << std::endl;
    }
}

void example3_ChatMessage() {
    std::cout << "\n=== Example 3: Send Chat Message ===" << std::endl;

    apollo::net::MockSession session(1);
    session.setPlayerId(1001);

    RpcClient client;

    // 发送聊天消息
    SendMessageRequest chatReq;
    chatReq.senderId = 1001;
    chatReq.channel = 0;  // world chat
    std::snprintf(chatReq.message, sizeof(chatReq.message),
                  "Hello, World! This is a test message.");

    SendMessageResponse chatResp;

    if (client.call(&session, CHAT_SERVICE, ChatMethod::SEND_MESSAGE,
                     chatReq, chatResp)) {
        std::cout << "  Message sent successfully!" << std::endl;
    }
}

void example4_AsyncCall() {
    std::cout << "\n=== Example 4: Async RPC Call ===" << std::endl;

    apollo::net::MockSession session(1);
    session.setPlayerId(1001);

    RpcClient client;

    // 异步调用
    uint64_t playerId = 1001;
    client.asyncCall(&session, PLAYER_SERVICE, PlayerMethod::GET_INFO,
                      playerId, [](bool success) {
        std::cout << "  Async call " << (success ? "succeeded" : "failed") << std::endl;
    });

    // 等待异步完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void example5_BatchCalls() {
    std::cout << "\n=== Example 5: Batch RPC Calls ===" << std::endl;

    apollo::net::MockSession session(1);
    RpcClient client;

    std::cout << "  Calling GetPlayerInfo for 100 players..." << std::endl;

    auto start = std::chrono::steady_clock::now();

    for (uint64_t i = 1000; i < 1100; ++i) {
        uint64_t playerId = i;
        PlayerInfo info;
        client.call(&session, PLAYER_SERVICE, PlayerMethod::GET_INFO,
                     playerId, info);
    }

    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "  Completed 100 calls in " << elapsed.count() << "ms" << std::endl;
    std::cout << "  Average: " << elapsed.count() / 100.0 << "ms per call" << std::endl;
}

//==============================================================================
// 主程序
//==============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "=== Apollo RPC Demo ===" << std::endl;
    std::cout << "========================================" << std::endl;

    example1_GetPlayerInfo();
    example2_LevelUp();
    example3_ChatMessage();
    example4_AsyncCall();
    example5_BatchCalls();

    std::cout << "\n=== Demo Complete ===" << std::endl;

    return 0;
}
