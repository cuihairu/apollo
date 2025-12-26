/**
 * @file actor_demo.cpp
 * @brief Actor 框架使用示例
 */

#include "apollo/actor/actor_system.h"
#include "apollo/actor/actor_cell.h"
#include "apollo/actor/actor_utils.h"
#include <iostream>
#include <thread>

using namespace apollo::actor;

//==============================================================================
// 消息定义
//==============================================================================

// 聊天消息
struct ChatMessage {
    std::string content;
    ActorRef from;

    static const char* typeName() { return "ChatMessage"; }

    // FlatBuffers 序列化（简化版，实际应使用 .fbs 生成）
    static std::vector<uint8_t> serialize(const ChatMessage& msg) {
        std::string str = msg.content;
        return std::vector<uint8_t>(str.begin(), str.end());
    }

    static ChatMessage deserialize(const std::vector<uint8_t>& data) {
        ChatMessage msg;
        msg.content = std::string(data.begin(), data.end());
        return msg;
    }
};

// 请求消息
struct GetUserRequest {
    uint64_t userId;

    static const char* typeName() { return "GetUserRequest"; }

    static std::vector<uint8_t> serialize(const GetUserRequest& msg) {
        APOLLO_POD_MESSAGE(GetUserRequest)
        std::vector<uint8_t> data(sizeof(msg));
        std::memcpy(data.data(), &msg, sizeof(msg));
        return data;
    }

    static GetUserRequest deserialize(const std::vector<uint8_t>& data) {
        GetUserRequest msg;
        std::memcpy(&msg, data.data(), sizeof(msg));
        return msg;
    }
};

// 响应消息
struct GetUserResponse {
    uint64_t userId;
    std::string username;
    int level;

    static const char* typeName() { return "GetUserResponse"; }
};

// PING 消息
struct Ping {
    int64_t timestamp;
    ActorRef replyTo;

    static const char* typeName() { return "Ping"; }
};

// PONG 消息
struct Pong {
    int64_t timestamp;
    int64_t originalTimestamp;

    static const char* typeName() { return "Pong"; }
};

//==============================================================================
// Actor 实现
//==============================================================================

// 聊天室 Actor
class ChatRoom : public Actor {
public:
    APOLLO_ACTOR_DECLARE(ChatRoom)

protected:
    void onStart() override {
        std::cout << "[ChatRoom] Started: " << path().name << std::endl;
        Actor::onStart();
    }

    void receive(const Message& msg) override {
        if (msg.is<ChatMessage>()) {
            auto chat = msg.as<ChatMessage>();
            std::cout << "[ChatRoom] " << chat.content << std::endl;

            // 广播给所有成员
            // broadcast(chat);
        }
    }

private:
    std::vector<ActorRef> members_;
};

// 用户服务 Actor
class UserService : public Actor {
protected:
    void receive(const Message& msg) override {
        if (msg.is<GetUserRequest>()) {
            auto req = msg.as<GetUserRequest>();

            std::cout << "[UserService] Handling user request: " << req.userId << std::endl;

            // 模拟查询
            GetUserResponse response;
            response.userId = req.userId;
            response.username = "User" + std::to_string(req.userId);
            response.level = 42;

            // 回复
            // sender().tell(response);
        }
    }

private:
    // 模拟数据库
    std::unordered_map<uint64_t, GetUserResponse> users_;
};

// Ping Pong Actor（用于测试）
class PingPongActor : public Actor {
protected:
    void onStart() override {
        std::cout << "[PingPong] Started: " << path().name << std::endl;

        // 启动定时器
        if (path().name == "ping") {
            context()->scheduleRepeated(1000, [this]() {
                Ping ping;
                ping.timestamp = currentTimeMs();
                // 找到 pong actor
                // auto pong = context()->lookup("pong");
                // pong.get().tell(ping, self());
            });
        }
    }

    void receive(const Message& msg) override {
        if (msg.is<Ping>()) {
            auto ping = msg.as<Ping>();

            Pong pong;
            pong.timestamp = currentTimeMs();
            pong.originalTimestamp = ping.timestamp;

            std::cout << "[Pong] Received ping, sending pong" << std::endl;
            // ping.replyTo.tell(pong);

        } else if (msg.is<Pong>()) {
            auto pong = msg.as<Pong>();

            int64_t rtt = currentTimeMs() - pong.originalTimestamp;
            std::cout << "[Ping] Received pong, RTT: " << rtt << "ms" << std::endl;
        }
    }
};

//==============================================================================
// 网关 Actor（处理客户端请求）
//==============================================================================

class GameGateway : public Actor {
protected:
    void onStart() override {
        std::cout << "[Gateway] Started, connecting to services..." << std::endl;

        // 连接到用户服务
        // userService_ = context()->lookup("user-service").get();
    }

    void receive(const Message& msg) override {
        if (msg.is<GetUserRequest>()) {
            auto req = msg.as<GetUserRequest>();

            std::cout << "[Gateway] Forwarding user request" << std::endl;

            // 转发到用户服务
            // userService_.ask<GetUserRequest, GetUserResponse>(req, 5000)
            //     .then([this](const GetUserResponse& resp) {
            //         sender().tell(resp);
            //     });
        }
    }

private:
    ActorRef userService_;
};

//==============================================================================
// 主函数
//==============================================================================

int main() {
    std::cout << "=== Apollo Actor Framework Demo ===" << std::endl;

    //==========================================================================
    // 1. 创建 Actor 系统
    //==========================================================================

    ActorSystemConfig config;
    config.systemName = "game-server";
    config.address = "127.0.0.1";
    config.dataCenter = "shanghai";
    config.hostId = "server-001";

    // 使用内存模式进行测试（生产环境使用 SQLite 或 Redis）
    config.discoveryConfig.backend = DiscoveryBackend::Memory;

    ActorSystem system(config);

    if (!system.start()) {
        std::cerr << "Failed to start Actor system" << std::endl;
        return 1;
    }

    std::cout << "Actor system started" << std::endl;

    //==========================================================================
    // 2. 创建服务 Actor
    //==========================================================================

    auto chatRoom = system.spawn<ChatRoom>("chat-room");
    auto userService = system.spawn<UserService>("user-service");
    auto gateway = system.spawn<GameGateway>("gateway");

    // Ping Pong 测试
    auto ping = system.spawn<PingPongActor>("ping");
    auto pong = system.spawn<PingPongActor>("pong");

    std::cout << "Actors created" << std::endl;

    //==========================================================================
    // 3. 发送消息
    //==========================================================================

    // 发送聊天消息
    ChatMessage chat;
    chat.content = "Hello, World!";

    chatRoom.tell(chat);

    // 请求用户信息
    GetUserRequest userReq;
    userReq.userId = 12345;

    gateway.tell(userReq);

    //==========================================================================
    // 4. 远程 Actor 示例
    //==========================================================================

    // 查找远程 Actor
    auto remoteFuture = system.lookup("game-server://remote-server/chat-room");

    std::cout << "Running... (press Ctrl+C to exit)" << std::endl;

    //==========================================================================
    // 5. 运行一段时间
    //==========================================================================

    std::this_thread::sleep_for(std::chrono::seconds(5));

    //==========================================================================
    // 6. 关闭系统
    //==========================================================================

    std::cout << "Shutting down..." << std::endl;
    system.shutdown();

    std::cout << "Done!" << std::endl;
    return 0;
}

//==============================================================================
// 编译说明
//==============================================================================
/*
g++ -std=c++20 -I../include -L../build actor_demo.cpp -lapollo -lpthread -o actor_demo

或使用 CMake:
add_executable(actor_demo examples/actor_demo.cpp)
target_link_libraries(actor_demo PRIVATE apollo)
*/
