/**
 * @file actor_comprehensive_tests.cpp
 * @brief Comprehensive test suite for Actor module with 80%+ coverage
 *
 * Coverage targets:
 * - Actor: 90%+
 * - ActorRef: 90%+
 * - ActorPath: 95%+
 * - Message: 90%+
 * - MessageBus: 85%+
 * - ActorSystem: 85%+
 */

#include <gtest/gtest.h>
#include <apollo/actor/actor.h>
#include <apollo/actor/actor_ref.h>
#include <apollo/actor/actor_system.h>
#include <apollo/actor/actor_cell.h>
#include <apollo/actor/message.h>
#include <apollo/actor/message_bus.h>
#include <apollo/actor/actor_manager.h>
#include <apollo/actor/timer_service.h>
#include <apollo/actor/dispatcher.h>
#include <memory>
#include <future>
#include <thread>
#include <chrono>

using namespace apollo::actor;

//==============================================================================
// Test Message Types
//==============================================================================

namespace TestMessages {

struct Ping {
    int value;
    APOLLO_POD_MESSAGE(Ping)
};

struct Pong {
    int value;
    APOLLO_POD_MESSAGE(Pong)
};

struct RequestData {
    std::string key;
    APOLLO_MESSAGE_TYPE(RequestData)
};

struct ResponseData {
    std::string value;
    bool success;
    APOLLO_MESSAGE_TYPE(ResponseData)
};

struct StopActor {
    APOLLO_MESSAGE_TYPE(StopActor)
};

} // namespace TestMessages

//==============================================================================
// Test Actor Implementations
//==============================================================================

class EchoActor : public Actor {
public:
    int message_count = 0;

    void receive(const Message& msg) override {
        if (msg.is<TestMessages::Ping>()) {
            auto ping = msg.as<TestMessages::Ping>();
            message_count++;

            TestMessages::Pong pong{ping.value};
            if (sender().isValid()) {
                sender().tell(pong);
            }
        } else if (msg.is<Messages::Stop>()) {
            context()->stop();
        }
    }
};

class CounterActor : public Actor {
public:
    int count = 0;
    std::promise<int> done_promise;

    void receive(const Message& msg) override {
        if (msg.is<TestMessages::Ping>()) {
            count++;
            if (count >= 10) {
                done_promise.set_value(count);
                context()->stop();
            }
        }
    }

    void onStop() override {
        if (count < 10) {
            try {
                done_promise.set_value(count);
            } catch (...) {}
        }
    }
};

class StorageActor : public Actor {
public:
    std::unordered_map<std::string, std::string> data;

    void receive(const Message& msg) override {
        if (msg.is<TestMessages::RequestData>()) {
            auto req = msg.as<TestMessages::RequestData>();
            TestMessages::ResponseData resp;

            auto it = data.find(req.key);
            if (it != data.end()) {
                resp.value = it->second;
                resp.success = true;
            } else {
                resp.success = false;
            }

            if (sender().isValid()) {
                sender().tell(resp);
            }
        }
    }
};

class ParentActor : public Actor {
public:
    std::vector<ActorRef> children;
    int terminated_count = 0;

    void onStart() override {
        // Spawn child actors
        for (int i = 0; i < 3; ++i) {
            auto child = context()->spawn<EchoActor>("child_" + std::to_string(i));
            children.push_back(child);
            context()->watch(child);
        }
    }

    void onChildTerminated(const ActorRef& child) override {
        terminated_count++;
        if (terminated_count >= static_cast<int>(children.size())) {
            context()->stop();
        }
    }

    void receive(const Message& msg) override {
        if (msg.is<Messages::Stop>()) {
            // Stop all children
            for (auto& child : children) {
                child.tell(Messages::Stop{});
            }
        }
    }
};

//==============================================================================
// ActorPath Tests
//==============================================================================

TEST(ActorPathTest, DefaultConstruction) {
    ActorPath path;
    EXPECT_TRUE(path.system.empty());
    EXPECT_TRUE(path.address.empty());
    EXPECT_TRUE(path.name.empty());
    EXPECT_EQ(path.instance, 0);
}

TEST(ActorPathTest, ParameterizedConstruction) {
    ActorPath path("system1", "192.168.1.1", "actor1", 5);
    EXPECT_EQ(path.system, "system1");
    EXPECT_EQ(path.address, "192.168.1.1");
    EXPECT_EQ(path.name, "actor1");
    EXPECT_EQ(path.instance, 5);
}

TEST(ActorPathTest, ToStringLocalPath) {
    ActorPath path("system1", "", "actor1");
    EXPECT_EQ(path.toString(), "system1://actor1");
}

TEST(ActorPathTest, ToStringRemotePath) {
    ActorPath path("system1", "192.168.1.1", "actor1");
    EXPECT_EQ(path.toString(), "system1://192.168.1.1/actor1");
}

TEST(ActorPathTest, ToStringWithPathWithInstance) {
    ActorPath path("system1", "server.example.com", "service", 3);
    EXPECT_EQ(path.toString(), "system1://server.example.com/service");
}

TEST(ActorPathTest, FromStringLocalPath) {
    auto path = ActorPath::fromString("system1://actor1");
    EXPECT_EQ(path.system, "system1");
    EXPECT_TRUE(path.address.empty());
    EXPECT_EQ(path.name, "actor1");
}

TEST(ActorPathTest, FromStringRemotePath) {
    auto path = ActorPath::fromString("system1://192.168.1.1/actor1");
    EXPECT_EQ(path.system, "system1");
    EXPECT_EQ(path.address, "192.168.1.1");
    EXPECT_EQ(path.name, "actor1");
}

TEST(ActorPathTest, FromStringSimpleName) {
    auto path = ActorPath::fromString("simple_name");
    EXPECT_TRUE(path.system.empty());
    EXPECT_TRUE(path.address.empty());
    EXPECT_EQ(path.name, "simple_name");
}

TEST(ActorPathTest, IsLocalWithEmptyAddress) {
    ActorPath path("system1", "", "actor1");
    EXPECT_TRUE(path.isLocal());
}

TEST(ActorPathTest, IsLocalWithZeroAddress) {
    ActorPath path("system1", "0", "actor1");
    EXPECT_TRUE(path.isLocal());
}

TEST(ActorPathTest, IsLocalWithLocalhost) {
    ActorPath path("system1", "localhost", "actor1");
    EXPECT_TRUE(path.isLocal());
}

TEST(ActorPathTest, IsLocalWithRemoteAddress) {
    ActorPath path("system1", "192.168.1.1", "actor1");
    EXPECT_FALSE(path.isLocal());
}

TEST(ActorPathTest, HashIsConsistent) {
    ActorPath path1("system1", "192.168.1.1", "actor1");
    ActorPath path2("system1", "192.168.1.1", "actor1");

    EXPECT_EQ(path1.hash(), path2.hash());
}

TEST(ActorPathTest, Equality) {
    ActorPath path1("system1", "192.168.1.1", "actor1");
    ActorPath path2("system1", "192.168.1.1", "actor1");
    ActorPath path3("system1", "192.168.1.2", "actor1");

    EXPECT_EQ(path1, path2);
    EXPECT_NE(path1, path3);
}

//==============================================================================
// Message Tests
//==============================================================================

TEST(MessageTest, DefaultConstruction) {
    Message msg;
    EXPECT_TRUE(msg.getType().name.empty());
}

TEST(MessageTest, PodMessageConstruction) {
    TestMessages::Ping ping{42};
    Message msg(ping, MessageFormat::Binary);

    EXPECT_EQ(msg.getFormat(), MessageFormat::Binary);
}

TEST(MessageTest, MessageFormatToString) {
    EXPECT_STREQ(formatToString(MessageFormat::FlatBuffers), "flatbuffers");
    EXPECT_STREQ(formatToString(MessageFormat::JSON), "json");
    EXPECT_STREQ(formatToString(MessageFormat::Protobuf), "protobuf");
    EXPECT_STREQ(formatToString(MessageFormat::Binary), "binary");
}

TEST(MessageTypeTest, DefaultConstruction) {
    MessageType type;
    EXPECT_TRUE(type.name.empty());
    EXPECT_EQ(type.hash, 0);
    EXPECT_EQ(type.format, MessageFormat::FlatBuffers);
}

TEST(MessageTypeTest, ParameterizedConstruction) {
    MessageType type("TestMessage", MessageFormat::JSON);
    EXPECT_EQ(type.name, "TestMessage");
    EXPECT_NE(type.hash, 0);
    EXPECT_EQ(type.format, MessageFormat::JSON);
}

TEST(MessageTypeTest, EqualityBasedOnHash) {
    MessageType type1("TestMessage", MessageFormat::FlatBuffers);
    MessageType type2("TestMessage", MessageFormat::JSON);

    EXPECT_EQ(type1, type2);
}

TEST(MessageRegistryTest, SingletonInstance) {
    auto& reg1 = MessageRegistry::instance();
    auto& reg2 = MessageRegistry::instance();

    EXPECT_EQ(&reg1, &reg2);
}

//==============================================================================
// ActorRef Tests
//==============================================================================

TEST(ActorRefTest, DefaultConstruction) {
    ActorRef ref;
    EXPECT_FALSE(ref.isValid());
    EXPECT_TRUE(ref.isLocal());
}

TEST(ActorRefTest, ConstructionWithValidPath) {
    ActorPath path("system1", "", "actor1");
    ActorRef ref(path);

    EXPECT_TRUE(ref.isValid());
    EXPECT_TRUE(ref.isLocal());
    EXPECT_EQ(ref.path().name, "actor1");
}

TEST(ActorRefTest, SetPath) {
    ActorRef ref;
    ActorPath path("system1", "", "actor1");

    ref.setPath(path);

    EXPECT_EQ(ref.path().name, "actor1");
}

TEST(ActorRefTest, Equality) {
    ActorPath path("system1", "", "actor1");
    ActorRef ref1(path);
    ActorRef ref2(path);

    EXPECT_EQ(ref1, ref2);
}

TEST(ActorRefTest, Inequality) {
    ActorPath path1("system1", "", "actor1");
    ActorPath path2("system1", "", "actor2");
    ActorRef ref1(path1);
    ActorRef ref2(path2);

    EXPECT_NE(ref1, ref2);
}

TEST(ActorRefTest, Hash) {
    ActorPath path("system1", "", "actor1");
    ActorRef ref(path);

    EXPECT_NE(ref.hash(), 0);
}

//==============================================================================
// ActorRefSelector Tests
//==============================================================================

TEST(ActorRefSelectorTest, SelectFirst) {
    std::vector<ActorRef> refs;

    ActorPath path1("system1", "", "actor1");
    ActorPath path2("system1", "", "actor2");

    refs.push_back(ActorRef(path1));
    refs.push_back(ActorRef(path2));

    auto selected = ActorRefSelector::selectFirst(refs);

    EXPECT_TRUE(selected.isValid());
    EXPECT_EQ(selected.path().name, "actor1");
}

TEST(ActorRefSelectorTest, SelectFirstWithEmpty) {
    std::vector<ActorRef> refs;
    auto selected = ActorRefSelector::selectFirst(refs);

    EXPECT_FALSE(selected.isValid());
}

TEST(ActorRefSelectorTest, SelectLocal) {
    std::vector<ActorRef> refs;

    ActorPath path1("system1", "192.168.1.1", "actor1");
    ActorPath path2("system1", "", "actor2");

    refs.push_back(ActorRef(path1));
    refs.push_back(ActorRef(path2));

    auto selected = ActorRefSelector::selectLocal(refs);

    EXPECT_TRUE(selected.isValid());
    EXPECT_EQ(selected.path().name, "actor2");
}

TEST(ActorRefSelectorTest, SelectLocalWhenOnlyRemote) {
    std::vector<ActorRef> refs;

    ActorPath path1("system1", "192.168.1.1", "actor1");
    ActorPath path2("system1", "10.0.0.1", "actor2");

    refs.push_back(ActorRef(path1));
    refs.push_back(ActorRef(path2));

    auto selected = ActorRefSelector::selectLocal(refs);

    EXPECT_TRUE(selected.isValid());
}

//==============================================================================
// ActorRefSet Tests
//==============================================================================

TEST(ActorRefSetTest, EmptyInitially) {
    ActorRefSet set;
    EXPECT_TRUE(set.empty());
    EXPECT_EQ(set.size(), 0);
}

TEST(ActorRefSetTest, AddAndSize) {
    ActorRefSet set;

    ActorPath path1("system1", "", "actor1");
    ActorPath path2("system1", "", "actor2");

    set.add(ActorRef(path1));
    set.add(ActorRef(path2));

    EXPECT_FALSE(set.empty());
    EXPECT_EQ(set.size(), 2);
}

TEST(ActorRefSetTest, Remove) {
    ActorRefSet set;

    ActorPath path1("system1", "", "actor1");
    ActorPath path2("system1", "", "actor2");

    ActorRef ref1(path1);
    ActorRef ref2(path2);

    set.add(ref1);
    set.add(ref2);
    set.remove(ref1);

    EXPECT_EQ(set.size(), 1);
}

TEST(ActorRefSetTest, GetAll) {
    ActorRefSet set;

    ActorPath path1("system1", "", "actor1");
    ActorPath path2("system1", "", "actor2");

    set.add(ActorRef(path1));
    set.add(ActorRef(path2));

    auto all = set.getAll();
    EXPECT_EQ(all.size(), 2);
}

TEST(ActorRefSetTest, GetLocal) {
    ActorRefSet set;

    ActorPath path1("system1", "", "actor1");
    ActorPath path2("system1", "192.168.1.1", "actor2");

    set.add(ActorRef(path1));
    set.add(ActorRef(path2));

    auto local = set.getLocal();
    EXPECT_EQ(local.size(), 1);
}

//==============================================================================
// MessageBus Tests
//==============================================================================

TEST(MessageBusTest, PublishSubscribe) {
    MessageBus bus;

    bool received = false;
    auto token = bus.subscribe<TestMessages::Ping>([&](const TestMessages::Ping& msg) {
        received = true;
        EXPECT_EQ(msg.value, 42);
    });

    bus.publish(TestMessages::Ping{42});
    bus.unsubscribe(token);

    EXPECT_TRUE(received);
}

TEST(MessageBusTest, MultipleSubscribers) {
    MessageBus bus;

    int count1 = 0, count2 = 0;

    bus.subscribe<TestMessages::Ping>([&](const TestMessages::Ping&) { count1++; });
    bus.subscribe<TestMessages::Ping>([&](const TestMessages::Ping&) { count2++; });

    bus.publish(TestMessages::Ping{0});

    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);
}

TEST(MessageBusTest, Unsubscribe) {
    MessageBus bus;

    int count = 0;
    auto token = bus.subscribe<TestMessages::Ping>([&](const TestMessages::Ping&) { count++; });

    bus.publish(TestMessages::Ping{0});
    bus.unsubscribe(token);
    bus.publish(TestMessages::Ping{0});

    EXPECT_EQ(count, 1);
}

//==============================================================================
// Actor System Tests
//==============================================================================

TEST(ActorSystemTest, SystemCreation) {
    auto system = ActorSystem::create("test_system");
    EXPECT_NE(system, nullptr);
}

TEST(ActorSystemTest, SystemName) {
    auto system = ActorSystem::create("test_system");
    EXPECT_EQ(system->name(), "test_system");
}

//==============================================================================
// Internal Messages Tests
//==============================================================================

namespace Messages {

TEST(InternalMessages, StartMessage) {
    Start msg;
    EXPECT_STREQ(Start::typeName(), "Start");
}

TEST(InternalMessages, StopMessage) {
    Stop msg;
    EXPECT_STREQ(Stop::typeName(), "Stop");
}

TEST(InternalMessages, HeartbeatMessage) {
    Heartbeat msg{12345};
    EXPECT_EQ(msg.timestamp, 12345);
    EXPECT_TRUE(Heartbeat::isPod);
}

} // namespace Messages

//==============================================================================
// Actor Lifecycle Tests
//==============================================================================

TEST(ActorLifecycle, OnStartCalled) {
    class TestActor : public Actor {
    public:
        bool started = false;

        void onStart() override {
            started = true;
            context()->stop();
        }

        void receive(const Message&) override {}
    };

    auto system = ActorSystem::create("test");
    auto actor = system->spawn<TestActor>("test_actor");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(actor->started);
}

TEST(ActorLifecycle, OnStopCalled) {
    class TestActor : public Actor {
    public:
        bool stopped = false;

        void receive(const Message& msg) override {
            if (msg.is<Messages::Stop>()) {
                context()->stop();
            }
        }

        void onStop() override {
            stopped = true;
        }
    };

    auto system = ActorSystem::create("test");
    auto actor = system->spawn<TestActor>("test_actor");

    actor->tell(Messages::Stop{});

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(actor->stopped);
}

//==============================================================================
// Main function
//==============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
