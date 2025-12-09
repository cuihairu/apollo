#include "gtest/gtest.h"
#include "apollo/net/protobuf_message.hpp"
#include "apollo/net/rpc.hpp"
#include "apollo/net/reactor.hpp"
#include <thread>
#include <chrono>

using namespace apollo::net;

// 测试用 Protobuf 消息定义
class TestMessage : public google::protobuf::Message {
public:
    TestMessage() : id_(0), name_("") {}
    TestMessage(int32_t id, const std::string& name) : id_(id), name_(name) {}

    int32_t GetId() const { return id_; }
    void SetId(int32_t id) { id_ = id; }
    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; }

    // Message interface
    TestMessage* New() const override { return new TestMessage(); }
    TestMessage* New(Arena* arena) const override {
        return Arena::CreateMessage<TestMessage>(arena);
    }

    void CopyFrom(const Message& from) override {
        const TestMessage* msg = dynamic_cast<const TestMessage*>(&from);
        if (msg) {
            id_ = msg->id_;
            name_ = msg->name_;
        }
    }

    void MergeFrom(const Message& from) override {
        const TestMessage* msg = dynamic_cast<const TestMessage*>(&from);
        if (msg) {
            id_ = msg->id_;
            name_ = msg->name_;
        }
    }

    void Clear() override {
        id_ = 0;
        name_.clear();
    }

    bool IsInitialized() const override { return true; }

    size_t ByteSizeLong() const override {
        return sizeof(id_) + name_.size();
    }

    bool SerializePartialToArray(void* data, int size) const override {
        return false;  // 简化实现
    }

    bool SerializeToArray(void* data, int size) const override {
        if (size < static_cast<int>(ByteSizeLong())) {
            return false;
        }

        char* ptr = static_cast<char*>(data);
        memcpy(ptr, &id_, sizeof(id_));
        ptr += sizeof(id_);
        memcpy(ptr, name_.c_str(), name_.size());
        return true;
    }

    bool ParsePartialFromArray(const void* data, int size) override {
        return false;  // 简化实现
    }

    bool ParseFromArray(const void* data, int size) override {
        if (size < static_cast<int>(sizeof(id_))) {
            return false;
        }

        const char* ptr = static_cast<const char*>(data);
        memcpy(&id_, ptr, sizeof(id_));
        ptr += sizeof(id_);
        name_.assign(ptr, size - sizeof(id_));
        return true;
    }

    const Descriptor* GetDescriptor() const override {
        static Descriptor descriptor;
        descriptor.set_name("TestMessage");
        return &descriptor;
    }

    const Reflection* GetReflection() const override {
        return nullptr;  // 简化实现
    }

private:
    int32_t id_;
    std::string name_;
};

class ProtobufMessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 注册消息类型
        ProtobufMessageFactory::Instance().Register<TestMessage>();
    }
};

TEST_F(ProtobufMessageTest, MessageWrapper) {
    // 创建消息包装器
    auto wrapper = std::make_shared<ProtobufMessageWrapper<TestMessage>>();
    ASSERT_NE(wrapper, nullptr);

    // 设置消息内容
    TestMessage* msg = wrapper->GetTypedMessage();
    msg->SetId(123);
    msg->SetName("test");

    // 测试序列化
    Buffer buffer;
    EXPECT_TRUE(wrapper->Serialize(buffer));

    // 测试反序列化
    auto newWrapper = std::make_shared<ProtobufMessageWrapper<TestMessage>>();
    EXPECT_TRUE(newWrapper->Deserialize(buffer));

    // 验证内容
    const TestMessage* newMsg = newWrapper->GetTypedMessage();
    EXPECT_EQ(newMsg->GetId(), 123);
    EXPECT_EQ(newMsg->GetName(), "test");
}

TEST_F(ProtobufMessageTest, MessageFactory) {
    auto& factory = ProtobufMessageFactory::Instance();

    // 创建消息
    auto msg = factory.Create(GetMessageType<TestMessage>());
    ASSERT_NE(msg, nullptr);

    // 转换为具体类型
    auto wrapper = std::dynamic_pointer_cast<ProtobufMessageWrapper<TestMessage>>(msg);
    ASSERT_NE(wrapper, nullptr);

    // 设置内容
    wrapper->GetTypedMessage()->SetId(456);
    wrapper->GetTypedMessage()->SetName("factory_test");

    // 验证
    EXPECT_EQ(wrapper->GetTypedMessage()->GetId(), 456);
    EXPECT_EQ(wrapper->GetTypedMessage()->GetName(), "factory_test");
}

TEST_F(ProtobufMessageTest, MessageCodec) {
    TestMessage msg;
    msg.SetId(789);
    msg.SetName("codec_test");

    Buffer buffer;
    EXPECT_TRUE(ProtobufCodec::Encode(msg, GetMessageType<TestMessage>(), buffer));

    auto [type, data] = ProtobufCodec::Decode(buffer);
    EXPECT_EQ(type, GetMessageType<TestMessage>());
    EXPECT_GT(data.size(), 0);

    TestMessage decodedMsg;
    EXPECT_TRUE(decodedMsg.ParseFromArray(data.data(), static_cast<int>(data.size())));
    EXPECT_EQ(decodedMsg.GetId(), 789);
    EXPECT_EQ(decodedMsg.GetName(), "codec_test");
}

TEST_F(ProtobufMessageTest, RpcRequestMessage) {
    TestMessage request;
    request.SetId(1001);
    request.SetName("rpc_request");

    auto rpcMsg = std::make_shared<RpcRequestMessage<TestMessage>>(
        12345, 100, 1, request
    );

    Buffer buffer;
    EXPECT_TRUE(rpcMsg->Serialize(buffer));

    auto newRpcMsg = std::make_shared<RpcRequestMessage<TestMessage>>();
    EXPECT_TRUE(newRpcMsg->Deserialize(buffer));

    EXPECT_EQ(newRpcMsg->GetRpcHeader().id, 12345);
    EXPECT_EQ(newRpcMsg->GetRpcHeader().serviceId, 100);
    EXPECT_EQ(newRpcMsg->GetRpcHeader().methodId, 1);

    const TestMessage* msg = newRpcMsg->GetTypedMessage();
    EXPECT_EQ(msg->GetId(), 1001);
    EXPECT_EQ(msg->GetName(), "rpc_request");
}

TEST_F(ProtobufMessageTest, RpcResponseMessage) {
    TestMessage response;
    response.SetId(2002);
    response.SetName("rpc_response");

    auto rpcMsg = std::make_shared<RpcResponseMessage<TestMessage>>(
        54321, 0, response
    );

    Buffer buffer;
    EXPECT_TRUE(rpcMsg->Serialize(buffer));

    auto newRpcMsg = std::make_shared<RpcResponseMessage<TestMessage>>();
    EXPECT_TRUE(newRpcMsg->Deserialize(buffer));

    EXPECT_EQ(newRpcMsg->GetRpcHeader().id, 54321);
    EXPECT_EQ(newRpcMsg->GetRpcHeader().errorCode, 0);

    const TestMessage* msg = newRpcMsg->GetTypedMessage();
    EXPECT_EQ(msg->GetId(), 2002);
    EXPECT_EQ(msg->GetName(), "rpc_response");
}

// Reactor测试
class ReactorTest : public ::testing::Test {
protected:
    void SetUp() override {
        reactor = std::make_unique<Reactor>();
        ASSERT_TRUE(reactor->Initialize());
    }

    void TearDown() override {
        if (reactor) {
            reactor->Shutdown();
        }
    }

    std::unique_ptr<Reactor> reactor;
};

TEST_F(ReactorTest, BasicOperations) {
    // 这个测试需要真实的socket，这里只做基本操作测试
    EXPECT_TRUE(reactor->IsRunning() == false);
    reactor->Stop();
}

TEST_F(ReactorTest, SocketManagement) {
    // 创建测试socket
    Socket socket;
    EXPECT_TRUE(socket.Create());

    bool readCalled = false;
    auto callback = [&readCalled](socket_t sockfd, NetEventType events) {
        if (events & NetEventType::READ) {
            readCalled = true;
        }
    };

    // 添加到reactor
    EXPECT_TRUE(reactor->AddSocket(socket.GetSocket(), NetEventType::READ, callback));

    // 修改事件
    EXPECT_TRUE(reactor->ModifySocket(socket.GetSocket(), NetEventType::WRITE));

    // 移除socket
    EXPECT_TRUE(reactor->RemoveSocket(socket.GetSocket()));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}