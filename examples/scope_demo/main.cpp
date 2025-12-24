#include "scope_system.hpp"
#include <chrono>
#include <thread>

// ========== 测试用的服务类 ==========

// 数据库服务 - 应该是 Singleton
class DatabaseService {
private:
    std::string connectionInfo_;
    static int instanceCount;

public:
    DatabaseService() {
        instanceCount++;
        connectionInfo_ = "mysql://localhost:3306/game_db";
        std::cout << "    [DatabaseService] 构造函数调用！实例 #" << instanceCount << std::endl;
    }

    ~DatabaseService() {
        std::cout << "    [DatabaseService] 析构函数调用！" << std::endl;
    }

    void Connect() {
        std::cout << "    [DatabaseService] 连接到: " << connectionInfo_ << std::endl;
    }

    int GetInstanceCount() const {
        return instanceCount;
    }
};
int DatabaseService::instanceCount = 0;

// 玩家会话 - 可以是 Prototype 或 Thread Scope
class PlayerSession {
private:
    std::string playerId_;
    std::string sessionId_;
    static int instanceCount;

public:
    PlayerSession() {
        instanceCount++;
        playerId_ = "unknown";
        sessionId_ = GenerateSessionId();
        std::cout << "    [PlayerSession] 构造函数调用！实例 #" << instanceCount
                  << " (Session: " << sessionId_ << ")" << std::endl;
    }

    ~PlayerSession() {
        std::cout << "    [PlayerSession] 析构函数调用！Session: " << sessionId_ << std::endl;
    }

    void SetPlayerId(const std::string& id) {
        playerId_ = id;
        std::cout << "    [PlayerSession] 设置玩家ID: " << id << " (Session: " << sessionId_ << ")" << std::endl;
    }

    const std::string& GetSessionId() const {
        return sessionId_;
    }

private:
    std::string GenerateSessionId() {
        static int counter = 1000;
        return "session_" + std::to_string(++counter);
    }
};
int PlayerSession::instanceCount = 0;

// 计算器 - 应该是 Prototype（每次都是新的）
class Calculator {
private:
    double value_;
    static int instanceCount;

public:
    Calculator() : value_(0) {
        instanceCount++;
        std::cout << "    [Calculator] 构造函数调用！实例 #" << instanceCount << std::endl;
    }

    ~Calculator() {
        std::cout << "    [Calculator] 析构函数调用！" << std::endl;
    }

    void Add(double num) {
        value_ += num;
        std::cout << "    [Calculator] 加法: " << value_ << std::endl;
    }

    void Reset() {
        value_ = 0;
        std::cout << "    [Calculator] 重置！" << std::endl;
    }
};
int Calculator::instanceCount = 0;

// 线程本地计数器
class ThreadLocalCounter {
private:
    int count_;
    std::thread::id threadId_;
    static int instanceCount;

public:
    ThreadLocalCounter() : count_(0) {
        instanceCount++;
        threadId_ = std::this_thread::get_id();
        std::cout << "    [ThreadLocalCounter] 构造函数调用！线程 "
                  << threadId_ << ", 实例 #" << instanceCount << std::endl;
    }

    ~ThreadLocalCounter() {
        std::cout << "    [ThreadLocalCounter] 析构函数调用！线程 " << threadId_ << std::endl;
    }

    void Increment() {
        count_++;
        std::cout << "    [ThreadLocalCounter] 线程 " << threadId_
                  << " 计数: " << count_ << std::endl;
    }
};
int ThreadLocalCounter::instanceCount = 0;

// ========== 测试函数 ==========

void TestSingletonScope(apollo::ApplicationContext& app) {
    std::cout << "\n========== 测试 Singleton Scope ==========" << std::endl;

    // 获取多次，应该是同一个实例
    std::cout << "\n获取 DatabaseService 三次：" << std::endl;
    auto db1 = app.GetBean<DatabaseService>("databaseService");
    auto db2 = app.GetBean<DatabaseService>("databaseService");
    auto db3 = app.GetBean<DatabaseService>("databaseService");

    // 验证是同一个实例
    if (db1.get() == db2.get() && db2.get() == db3.get()) {
        std::cout << "\n✓ 验证通过：三个指针指向同一个实例！" << std::endl;
        std::cout << "  实例总数: " << db1->GetInstanceCount() << std::endl;
    } else {
        std::cout << "\n✗ 验证失败：创建了多个实例！" << std::endl;
    }

    db1->Connect();
}

void TestPrototypeScope(apollo::ApplicationContext& app) {
    std::cout << "\n========== 测试 Prototype Scope ==========" << std::endl;

    // 获取多次，每次都应该是新实例
    std::cout << "\n获取 Calculator 三次：" << std::endl;
    auto calc1 = app.GetBean<Calculator>("calculator");
    auto calc2 = app.GetBean<Calculator>("calculator");
    auto calc3 = app.GetBean<Calculator>("calculator");

    // 验证是不同的实例
    if (calc1.get() != calc2.get() && calc2.get() != calc3.get()) {
        std::cout << "\n✓ 验证通过：每次都创建新实例！" << std::endl;
    } else {
        std::cout << "\n✗ 验证失败：实例被复用了！" << std::endl;
    }

    // 测试每个实例是独立的
    std::cout << "\n测试实例独立性：" << std::endl;
    calc1->Add(10);
    calc2->Add(20);
    calc3->Add(30);

    calc1->Reset();
    calc2->Reset();
    calc3->Reset();
}

void TestThreadScope(apollo::ApplicationContext& app) {
    std::cout << "\n========== 测试 Thread Scope ==========" << std::endl;

    // 在主线程获取
    std::cout << "\n主线程获取 ThreadLocalCounter：" << std::endl;
    auto mainCounter = app.GetBean<ThreadLocalCounter>("threadCounter");
    mainCounter->Increment();
    mainCounter->Increment();

    // 创建多个线程测试
    std::vector<std::thread> threads;
    for (int i = 1; i <= 3; ++i) {
        threads.emplace_back([&app, i]() {
            std::cout << "\n--- 线程 " << i << " 开始 ---" << std::endl;

            // 每个线程第一次获取
            auto counter1 = app.GetBean<ThreadLocalCounter>("threadCounter");
            counter1->Increment();
            counter1->Increment();

            // 同一线程再次获取，应该是同一个实例
            auto counter2 = app.GetBean<ThreadLocalCounter>("threadCounter");
            counter2->Increment();

            // 验证同一线程内的实例是同一个
            if (counter1.get() == counter2.get()) {
                std::cout << "  ✓ 线程 " << i << " 内复用同一个实例" << std::endl;
            }

            std::cout << "--- 线程 " << i << " 结束 ---\n" << std::endl;
        });
    }

    // 主线程再次获取
    auto mainCounter2 = app.GetBean<ThreadLocalCounter>("threadCounter");
    mainCounter2->Increment();

    // 等待所有线程结束
    for (auto& t : threads) {
        t.join();
    }

    std::cout << "✓ Thread Scope 测试完成" << std::endl;
}

void MixedScopeTest(apollo::ApplicationContext& app) {
    std::cout << "\n========== 混合 Scope 测试 ==========" << std::endl;

    // 创建任务线程
    std::vector<std::thread> threads;
    for (int i = 1; i <= 2; ++i) {
        threads.emplace_back([&app, i]() {
            std::cout << "\n[任务线程 " << i << "] 开始处理请求" << std::endl;

            // 每个请求都获取新的计算器
            auto calc = app.GetBean<Calculator>("calculator");
            calc->Add(i * 100);

            // 但是共享同一个数据库连接
            auto db = app.GetBean<DatabaseService>("databaseService");

            // 每个线程有自己的计数器
            auto counter = app.GetBean<ThreadLocalCounter>("threadCounter");
            counter->Increment();

            // 再次获取计算器，应该是新实例
            auto calc2 = app.GetBean<Calculator>("calculator");
            calc2->Add(i * 200);

            // 再次获取数据库，应该是同一个实例
            auto db2 = app.GetBean<DatabaseService>("databaseService");

            if (db.get() == db2.get()) {
                std::cout << "  [任务线程 " << i << "] ✓ 数据库实例复用成功" << std::endl;
            }

            std::cout << "[任务线程 " << i << "] 请求处理完成" << std::endl;
        });
    }

    for (auto& t : threads) {
        t.join();
    }
}

// ========== 主函数 ==========

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "      Apollo Scope 系统演示程序" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // 创建应用上下文
    apollo::ApplicationContext app;

    // 注册不同 Scope 的 Bean
    std::cout << "注册 Beans..." << std::endl;
    app.RegisterBean<DatabaseService>("databaseService", "singleton");
    app.RegisterBean<PlayerSession>("playerSession", "prototype");  // 使用 prototype
    app.RegisterBean<Calculator>("calculator", "prototype");
    app.RegisterBean<ThreadLocalCounter>("threadCounter", "thread");

    // 初始化应用
    app.Initialize();

    // 运行各种测试
    TestSingletonScope(app);
    TestPrototypeScope(app);
    TestThreadScope(app);
    MixedScopeTest(app);

    // 打印最终统计信息
    apollo::ScopeManager::Instance().PrintStatistics();

    std::cout << "\n========================================" << std::endl;
    std::cout << "      程序结束" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}