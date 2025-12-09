#include "apollo/core/server_id.hpp"
#include "apollo/core/server_types.hpp"
#include "apollo/logger.hpp"
#include "apollo/thread_pool.hpp"
#include "apollo/memory_pool.hpp"
#include "apollo/net/socket.hpp"
#include "apollo/net/message.hpp"
#include "apollo/attribute.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

using namespace apollo;
using namespace apollo::core;
using namespace apollo::net;

void TestServerId() {
    std::cout << "\n=== Testing ServerId ===" << std::endl;

    // 创建ServerId
    ServerId id = ServerId::Create(1, 2, ServerType::GAME_SERVER, 1);
    std::cout << "ServerId: " << id.ToString() << std::endl;
    std::cout << "Region: " << id.GetRegion() << std::endl;
    std::cout << "Group: " << id.GetGroup() << std::endl;
    std::cout << "Type: " << static_cast<int>(id.GetType()) << std::endl;
    std::cout << "Instance: " << id.GetInstance() << std::endl;

    // 测试匹配
    std::cout << "Match GAME_SERVER: " << id.MatchType(ServerType::GAME_SERVER) << std::endl;
    std::cout << "Match Region 1: " << id.MatchRegion(1) << std::endl;

    // 测试字符串转换
    ServerId id2 = ServerId::FromString("1-2-6-1");
    std::cout << "FromString result: " << (id == id2) << std::endl;
}

void TestLogger() {
    std::cout << "\n=== Testing Logger ===" << std::endl;

    auto logger = LogManager::Instance().GetLogger("test");

    LOG_INFO(logger, "This is an info message");
    LOG_WARN(logger, "This is a warning message");
    LOG_ERROR(logger, "This is an error message: %d", 42);

    std::cout << "Logger test completed" << std::endl;
}

void TestThreadPool() {
    std::cout << "\n=== Testing ThreadPool ===" << std::endl;

    ThreadPool pool(4);

    // 提交任务
    std::vector<std::future<int>> results;

    for (int i = 0; i < 8; ++i) {
        results.emplace_back(
            pool.Submit([i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return i * i;
            })
        );
    }

    // 获取结果
    for (auto& result : results) {
        std::cout << "Result: " << result.get() << std::endl;
    }

    std::cout << "ThreadPool test completed" << std::endl;
}

void TestMemoryPool() {
    std::cout << "\n=== Testing MemoryPool ===" << std::endl;

    // 测试对象池
    ObjectPool<std::vector<int>> pool(5, 10);

    {
        auto vec = pool.Acquire();
        vec->push_back(1);
        vec->push_back(2);
        vec->push_back(3);
        std::cout << "Vector size: " << vec->size() << std::endl;
    } // 对象自动归还

    std::cout << "Available objects: " << pool.GetAvailableCount() << std::endl;

    // 测试内存池
    MemoryPoolManager& mgr = MemoryPoolManager::Instance();

    void* ptr = mgr.Allocate(1024);
    std::cout << "Allocated memory: " << (ptr ? "success" : "failed") << std::endl;

    if (ptr) {
        mgr.Deallocate(ptr, 1024);
        std::cout << "Deallocated memory" << std::endl;
    }

    std::cout << "MemoryPool test completed" << std::endl;
}

void TestAttribute() {
    std::cout << "\n=== Testing Attribute System ===" << std::endl;

    // 注册属性定义
    AttributeManager& mgr = AttributeManager::Instance();

    AttributeDef hpDef;
    hpDef.id = 1;
    hpDef.name = "HP";
    hpDef.type = AttributeType::INT32;
    hpDef.defaultValue = int32_t(100);
    hpDef.minValue = int32_t(0);
    hpDef.maxValue = int32_t(9999);
    hpDef.persistent = true;
    hpDef.syncToClient = true;

    mgr.RegisterAttribute(hpDef);

    // 创建属性容器
    auto container = mgr.CreateContainer(1001);

    // 设置属性
    SET_ATTR(container, 1, int32_t(150));

    // 获取属性
    int32_t hp = GET_ATTR(container, 1, int32_t);
    std::cout << "Player HP: " << hp << std::endl;

    // 增加属性
    ADD_ATTR(container, 1, int32_t(50));
    hp = GET_ATTR(container, 1, int32_t);
    std::cout << "Player HP after add: " << hp << std::endl;

    // 测试变更监听
    container->SetChangeListener([](const AttributeChangeEvent& event) {
        std::cout << "Attribute " << event.attributeId << " changed" << std::endl;
    });

    SET_ATTR(container, 1, int32_t(200));

    std::cout << "Attribute test completed" << std::endl;
}

void TestSocket() {
    std::cout << "\n=== Testing Socket ===" << std::endl;

    // 创建TCP Socket
    Socket socket;

    if (socket.CreateTCP()) {
        std::cout << "TCP Socket created successfully" << std::endl;

        // 设置非阻塞
        if (socket.SetNonBlocking(true)) {
            std::cout << "Socket set to non-blocking mode" << std::endl;
        }

        // 设置TCP_NODELAY
        if (socket.SetTcpNoDelay(true)) {
            std::cout << "TCP_NODELAY enabled" << std::endl;
        }
    }

    std::cout << "Socket test completed" << std::endl;
}

int main() {
    std::cout << "=== Apollo Framework Feature Tests ===" << std::endl;

    // 初始化日志系统
    LogManager::Instance().Initialize();

    TestServerId();
    TestLogger();
    TestThreadPool();
    TestMemoryPool();
    TestAttribute();
    TestSocket();

    std::cout << "\n=== All Tests Completed ===" << std::endl;

    return 0;
}