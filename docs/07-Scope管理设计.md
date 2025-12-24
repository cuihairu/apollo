# Apollo Framework Scope Management Design

## 1. Scope 概念与设计目标

### 1.1 为什么需要 Scope

在游戏服务器中，不同的对象有不同的生命周期需求：

- **数据库连接池**：整个应用生命周期内只有一个实例（Singleton）
- **玩家会话**：每个连接一个实例（Request/Session Scope）
- **任务上下文**：每个异步任务一个实例（Thread Scope）
- **临时计算器**：每次注入都是新实例（Prototype）

### 1.2 设计目标

1. **灵活的生命周期管理**：支持多种作用域，满足不同业务需求
2. **线程安全**：确保多线程环境下 Scope 的正确性
3. **性能优化**：避免不必要的对象创建和销毁
4. **易于扩展**：支持自定义 Scope 实现

## 2. Scope 类型定义

### 2.1 内置 Scope 类型

| Scope 名称 | 生命周期 | 使用场景 | 线程安全 |
|-----------|----------|----------|----------|
| **singleton** | 整个应用生命周期 | 服务类、管理器、配置类 | 需要 |
| **prototype** | 每次注入创建新实例 | 工具类、计算器、临时对象 | 不需要 |
| **thread** | 线程生命周期 | 线程本地存储、事务管理器 | 需要 |
| **request** | 请求生命周期 | 请求上下文、会话管理 | 需要 |
| **session** | 会话生命周期 | 玩家数据、游戏状态 | 需要 |

### 2.2 Scope 生命周期对比

```
Application 启动
│
├── Singleton 创建（应用启动时创建，永不销毁直到应用关闭）
│   └── DatabaseService、ConfigManager 等
│
├── Thread Scope（线程启动时创建，线程结束时销毁）
│   ├── Thread 1: ThreadLocalContext A
│   └── Thread 2: ThreadLocalContext B
│
├── Request Scope（请求开始时创建，请求结束时销毁）
│   ├── Request 1: RequestContext A（包含 SessionX）
│   ├── Request 2: RequestContext B（包含 SessionX）
│   └── Request 3: RequestContext C（包含 SessionY）
│
└── Prototype（每次注入都创建新实例）
    ├── Calculator A
    ├── Calculator B
    └── Calculator C
```

## 3. 核心接口设计

### 3.1 Scope 接口

```cpp
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>

namespace apollo {

class ObjectFactory;

// Scope 基接口
class IScope {
public:
    virtual ~IScope() = default;

    // 获取 Scope 名称
    virtual std::string GetName() const = 0;

    // 从 Scope 中获取对象
    virtual std::shared_ptr<void> Get(const std::string& beanName,
                                    std::function<std::shared_ptr<void>()> factory) = 0;

    // 移除 Scope 中的对象
    virtual void Remove(const std::string& beanName) = 0;

    // 清理 Scope 中的所有对象
    virtual void Clear() = 0;

    // 获取 Scope 中对象的数量
    virtual size_t Size() const = 0;
};

// Scope 管理器接口
class IScopeManager {
public:
    virtual ~IScopeManager() = default;

    // 注册 Scope
    virtual void RegisterScope(const std::string& name,
                             std::unique_ptr<IScope> scope) = 0;

    // 获取 Scope
    virtual IScope* GetScope(const std::string& name) = 0;

    // 销毁 Scope
    virtual void DestroyScope(const std::string& name) = 0;

    // 获取所有已注册的 Scope 名称
    virtual std::vector<std::string> GetScopeNames() const = 0;
};

}
```

### 3.2 Bean 定义扩展

```cpp
// Bean 定义扩展，添加 Scope 支持
struct BeanDefinition {
    std::string name;
    std::string typeName;
    std::string scope = "singleton";  // 默认是 singleton
    bool lazyInit = false;
    std::unordered_map<std::string, std::any> properties;

    // 工厂方法
    std::function<std::shared_ptr<void>()> factory;

    // 依赖项
    std::vector<std::string> dependencies;

    // 初始化和销毁方法
    std::string initMethod;
    std::string destroyMethod;
};

// Bean 注册器扩展
class BeanRegistry {
public:
    // 注册 Bean（带 Scope）
    template<typename T>
    void Register(const std::string& name, const std::string& scope = "singleton") {
        BeanDefinition def;
        def.name = name;
        def.typeName = typeid(T).name();
        def.scope = scope;
        def.factory = [this]() -> std::shared_ptr<void> {
            return std::make_shared<T>();
        };

        RegisterBeanDefinition(def);
    }

    // 注册工厂方法
    template<typename T>
    void RegisterFactory(const std::string& name,
                        std::function<std::shared_ptr<T>()> factory,
                        const std::string& scope = "singleton") {
        BeanDefinition def;
        def.name = name;
        def.typeName = typeid(T).name();
        def.scope = scope;
        def.factory = [factory]() -> std::shared_ptr<void> {
            return std::shared_ptr<void>(factory(), [](void*){});
        };

        RegisterBeanDefinition(def);
    }

private:
    void RegisterBeanDefinition(const BeanDefinition& def);
};
```

## 4. 具体实现

### 4.1 Singleton Scope

```cpp
class SingletonScope : public IScope {
public:
    std::string GetName() const override { return "singleton"; }

    std::shared_ptr<void> Get(const std::string& beanName,
                            std::function<std::shared_ptr<void>()> factory) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = objects_.find(beanName);
        if (it != objects_.end()) {
            return it->second;
        }

        // 首次创建
        auto obj = factory();
        objects_[beanName] = obj;
        return obj;
    }

    void Remove(const std::string& beanName) override {
        std::lock_guard<std::mutex> lock(mutex_);
        objects_.erase(beanName);
    }

    void Clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        objects_.clear();
    }

    size_t Size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return objects_.size();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<void>> objects_;
};
```

### 4.2 Prototype Scope

```cpp
class PrototypeScope : public IScope {
public:
    std::string GetName() const override { return "prototype"; }

    std::shared_ptr<void> Get(const std::string& beanName,
                            std::function<std::shared_ptr<void>()> factory) override {
        // 每次都创建新实例
        return factory();
    }

    void Remove(const std::string& beanName) override {
        // Prototype 不缓存对象，无需移除
    }

    void Clear() override {
        // Prototype 不缓存对象，无需清理
    }

    size_t Size() const override {
        return 0;  // Prototype 不缓存对象
    }
};
```

### 4.3 Thread Scope

```cpp
#include <thread_local>

class ThreadScope : public IScope {
public:
    std::string GetName() const override { return "thread"; }

    std::shared_ptr<void> Get(const std::string& beanName,
                            std::function<std::shared_ptr<void>()> factory) override {
        // 使用线程本地存储
        auto& threadLocal = GetThreadLocal();

        auto it = threadLocal.objects.find(beanName);
        if (it != threadLocal.objects.end()) {
            return it->second;
        }

        // 首次创建
        auto obj = factory();
        threadLocal.objects[beanName] = obj;
        return obj;
    }

    void Remove(const std::string& beanName) override {
        auto& threadLocal = GetThreadLocal();
        threadLocal.objects.erase(beanName);
    }

    void Clear() override {
        auto& threadLocal = GetThreadLocal();
        threadLocal.objects.clear();
    }

    size_t Size() const override {
        auto& threadLocal = GetThreadLocal();
        return threadLocal.objects.size();
    }

private:
    struct ThreadLocalData {
        std::unordered_map<std::string, std::shared_ptr<void>> objects;
        std::mutex mutex;
    };

    ThreadLocalData& GetThreadLocal() {
        // C++11 thread_local 支持
        static thread_local ThreadLocalData instance;
        return instance;
    }
};
```

### 4.4 Request Scope

```cpp
// 请求上下文
class RequestContext {
public:
    static RequestContext& GetCurrent() {
        // 需要与具体的网络框架集成
        // 这里简化为 thread_local
        static thread_local RequestContext instance;
        return instance;
    }

    void SetRequestId(const std::string& requestId) {
        requestId_ = requestId;
    }

    const std::string& GetRequestId() const {
        return requestId_;
    }

    void SetSessionId(const std::string& sessionId) {
        sessionId_ = sessionId;
    }

    const std::string& GetSessionId() const {
        return sessionId_;
    }

private:
    std::string requestId_;
    std::string sessionId_;
};

class RequestScope : public IScope {
public:
    std::string GetName() const override { return "request"; }

    std::shared_ptr<void> Get(const std::string& beanName,
                            std::function<std::shared_ptr<void>()> factory) override {
        auto& context = RequestContext::GetCurrent();
        std::lock_guard<std::mutex> lock(mutex_);

        std::string key = context.GetRequestId() + ":" + beanName;

        auto it = requestObjects_.find(key);
        if (it != requestObjects_.end()) {
            return it->second;
        }

        auto obj = factory();
        requestObjects_[key] = obj;

        // 注册请求结束清理函数
        RegisterCleanupCallback(context.GetRequestId());

        return obj;
    }

    void Remove(const std::string& beanName) override {
        auto& context = RequestContext::GetCurrent();
        std::lock_guard<std::mutex> lock(mutex_);

        std::string key = context.GetRequestId() + ":" + beanName;
        requestObjects_.erase(key);
    }

    void Clear() override {
        auto& context = RequestContext::GetCurrent();
        CleanupRequest(context.GetRequestId());
    }

    size_t Size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return requestObjects_.size();
    }

    // 清理特定请求的对象
    void CleanupRequest(const std::string& requestId) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = requestObjects_.begin();
        while (it != requestObjects_.end()) {
            if (it->first.find(requestId + ":") == 0) {
                it = requestObjects_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<void>> requestObjects_;

    void RegisterCleanupCallback(const std::string& requestId) {
        // 实际实现中，这里应该注册到请求处理框架
        // 请求结束时自动调用 CleanupRequest
    }
};
```

### 4.5 Session Scope

```cpp
class SessionScope : public IScope {
public:
    std::string GetName() const override { return "session"; }

    std::shared_ptr<void> Get(const std::string& beanName,
                            std::function<std::shared_ptr<void>()> factory) override {
        auto& context = RequestContext::GetCurrent();
        std::string sessionId = context.GetSessionId();

        if (sessionId.empty()) {
            throw std::runtime_error("No active session");
        }

        std::lock_guard<std::mutex> lock(mutex_);

        auto& sessionObjects = sessions_[sessionId];
        auto it = sessionObjects.find(beanName);
        if (it != sessionObjects.end()) {
            return it->second;
        }

        auto obj = factory();
        sessionObjects[beanName] = obj;
        return obj;
    }

    void Remove(const std::string& beanName) override {
        auto& context = RequestContext::GetCurrent();
        std::string sessionId = context.GetSessionId();

        if (sessionId.empty()) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            it->second.erase(beanName);

            if (it->second.empty()) {
                sessions_.erase(it);
            }
        }
    }

    void Clear() override {
        auto& context = RequestContext::GetCurrent();
        std::string sessionId = context.GetSessionId();

        if (!sessionId.empty()) {
            CleanupSession(sessionId);
        }
    }

    size_t Size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t total = 0;
        for (const auto& pair : sessions_) {
            total += pair.second.size();
        }
        return total;
    }

    // 清理特定会话的对象
    void CleanupSession(const std::string& sessionId) {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.erase(sessionId);
    }

    // 获取会话数量
    size_t GetSessionCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return sessions_.size();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string,
        std::unordered_map<std::string, std::shared_ptr<void>>> sessions_;
};
```

## 5. Scope 管理器实现

```cpp
class ScopeManager : public IScopeManager {
public:
    static ScopeManager& Instance() {
        static ScopeManager instance;
        return instance;
    }

    void Initialize() {
        // 注册内置 Scope
        RegisterScope("singleton", std::make_unique<SingletonScope>());
        RegisterScope("prototype", std::make_unique<PrototypeScope>());
        RegisterScope("thread", std::make_unique<ThreadScope>());
        RegisterScope("request", std::make_unique<RequestScope>());
        RegisterScope("session", std::make_unique<SessionScope>());
    }

    void RegisterScope(const std::string& name,
                     std::unique_ptr<IScope> scope) override {
        std::lock_guard<std::mutex> lock(mutex_);
        scopes_[name] = std::move(scope);
    }

    IScope* GetScope(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = scopes_.find(name);
        if (it == scopes_.end()) {
            throw std::runtime_error("Unknown scope: " + name);
        }

        return it->second.get();
    }

    void DestroyScope(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = scopes_.find(name);
        if (it != scopes_.end()) {
            it->second->Clear();
            scopes_.erase(it);
        }
    }

    std::vector<std::string> GetScopeNames() const override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<std::string> names;
        for (const auto& pair : scopes_) {
            names.push_back(pair.first);
        }
        return names;
    }

    // 获取所有 Scope 的统计信息
    std::unordered_map<std::string, size_t> GetScopeStatistics() const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::unordered_map<std::string, size_t> stats;
        for (const auto& pair : scopes_) {
            stats[pair.first] = pair.second->Size();
        }
        return stats;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<IScope>> scopes_;
};
```

## 6. ApplicationContext 集成

```cpp
class ApplicationContext {
public:
    static ApplicationContext& Instance() {
        static ApplicationContext instance;
        return instance;
    }

    // 获取 Bean（支持 Scope）
    template<typename T>
    std::shared_ptr<T> GetBean(const std::string& name) {
        auto it = beanDefinitions_.find(name);
        if (it == beanDefinitions_.end()) {
            throw std::runtime_error("Bean not found: " + name);
        }

        const auto& def = it->second;
        auto scope = scopeManager_.GetScope(def.scope);

        // 从 Scope 中获取对象
        auto obj = scope->Get(name, [this, &def]() -> std::shared_ptr<void> {
            auto bean = def.factory();

            // 调用初始化方法
            if (!def.initMethod.empty()) {
                CallInitMethod(bean, def.initMethod);
            }

            return bean;
        });

        return std::static_pointer_cast<T>(obj);
    }

    // 按类型获取 Bean
    template<typename T>
    std::shared_ptr<T> GetBean() {
        std::string typeName = typeid(T).name();

        // 查找匹配的 Bean
        for (const auto& pair : beanDefinitions_) {
            if (pair.second.typeName == typeName) {
                return GetBean<T>(pair.first);
            }
        }

        throw std::runtime_error("Bean not found for type: " + typeName);
    }

    // 注册 Bean（带 Scope）
    template<typename T>
    void RegisterBean(const std::string& name,
                     const std::string& scope = "singleton") {
        BeanDefinition def;
        def.name = name;
        def.typeName = typeid(T).name();
        def.scope = scope;
        def.factory = [this]() -> std::shared_ptr<void> {
            return std::make_shared<T>();
        };

        beanDefinitions_[name] = def;
    }

    void Initialize() {
        // 初始化 Scope 管理器
        scopeManager_.Initialize();

        // 初始化所有 singleton beans
        InitializeSingletonBeans();
    }

    void Shutdown() {
        // 清理所有 Scope
        auto& scopeMgr = ScopeManager::Instance();
        auto scopes = scopeMgr.GetScopeNames();

        // 按相反顺序销毁
        std::reverse(scopes.begin(), scopes.end());
        for (const auto& scopeName : scopes) {
            if (scopeName != "prototype") {  // prototype 不需要清理
                scopeMgr.DestroyScope(scopeName);
            }
        }
    }

    // 获取 Scope 统计信息
    void PrintScopeStatistics() {
        auto& scopeMgr = ScopeManager::Instance();
        auto stats = scopeMgr.GetScopeStatistics();

        LOG_INFO("=== Scope Statistics ===");
        for (const auto& pair : stats) {
            LOG_INFO("Scope '%s': %zu objects", pair.first.c_str(), pair.second);
        }
    }

private:
    ScopeManager& scopeManager_ = ScopeManager::Instance();
    std::unordered_map<std::string, BeanDefinition> beanDefinitions_;

    void InitializeSingletonBeans() {
        // 按依赖顺序初始化所有 singleton beans
        std::vector<std::string> initOrder = CalculateInitOrder();

        auto singletonScope = scopeManager_.GetScope("singleton");
        for (const auto& beanName : initOrder) {
            const auto& def = beanDefinitions_[beanName];
            if (def.scope == "singleton" && !def.lazyInit) {
                singletonScope->Get(beanName, [this, &def]() {
                    return def.factory();
                });
            }
        }
    }

    std::vector<std::string> CalculateInitOrder() {
        // 实现依赖排序算法（拓扑排序）
        // 返回按依赖关系排序的 Bean 名称列表
        // ...
        return std::vector<std::string>();
    }

    void CallInitMethod(std::shared_ptr<void> bean, const std::string& methodName) {
        // 通过反射或预定义的初始化方法表调用
        // ...
    }
};
```

## 7. 使用示例

```cpp
// 1. 定义不同类型的组件
class DatabaseService {
public:
    void Connect() { LOG_INFO("Database connected"); }
    void Disconnect() { LOG_INFO("Database disconnected"); }
};

class PlayerSession {
public:
    PlayerSession() { LOG_INFO("New player session created"); }
    ~PlayerSession() { LOG_INFO("Player session destroyed"); }

    void SetPlayerId(uint64_t id) { playerId_ = id; }
    uint64_t GetPlayerId() const { return playerId_; }

private:
    uint64_t playerId_;
};

class RequestCounter {
public:
    RequestCounter() { count_ = 0; }

    void Increment() { count_++; }
    int GetCount() const { return count_; }

private:
    int count_;
};

// 2. 注册不同 Scope 的 Bean
int main() {
    auto& app = ApplicationContext::Instance();

    // 注册不同 Scope 的组件
    app.RegisterBean<DatabaseService>("databaseService", "singleton");
    app.RegisterBean<PlayerSession>("playerSession", "session");
    app.RegisterBean<RequestCounter>("requestCounter", "request");

    // 3. 初始化应用
    app.Initialize();

    // 4. 模拟请求处理
    // Request 1
    RequestContext::GetCurrent().SetRequestId("req1");
    RequestContext::GetCurrent().SetSessionId("session1");

    auto db1 = app.GetBean<DatabaseService>("databaseService");  // Singleton
    auto session1 = app.GetBean<PlayerSession>("playerSession");  // Session scope
    auto counter1 = app.GetBean<RequestCounter>("requestCounter");  // Request scope

    // Request 2 (同一个会话)
    RequestContext::GetCurrent().SetRequestId("req2");
    RequestContext::GetCurrent().SetSessionId("session1");

    auto db2 = app.GetBean<DatabaseService>("databaseService");  // 同一个实例
    auto session2 = app.GetBean<PlayerSession>("playerSession");  // 同一个会话实例
    auto counter2 = app.GetBean<RequestCounter>("requestCounter");  // 新的请求实例

    // Request 3 (新会话)
    RequestContext::GetCurrent().SetRequestId("req3");
    RequestContext::GetCurrent().SetSessionId("session2");

    auto db3 = app.GetBean<DatabaseService>("databaseService");  // 同一个实例
    auto session3 = app.GetBean<PlayerSession>("playerSession");  // 新的会话实例
    auto counter3 = app.GetBean<RequestCounter>("requestCounter");  // 新的请求实例

    // 打印 Scope 统计信息
    app.PrintScopeStatistics();

    // 5. 关闭应用
    app.Shutdown();

    return 0;
}
```

## 8. 自定义 Scope 支持

```cpp
// 自定义 Scope：游戏房间 Scope
class RoomScope : public IScope {
public:
    std::string GetName() const override { return "room"; }

    std::shared_ptr<void> Get(const std::string& beanName,
                            std::function<std::shared_ptr<void>()> factory) override {
        std::string roomId = GetCurrentRoomId();
        if (roomId.empty()) {
            throw std::runtime_error("No active room");
        }

        std::lock_guard<std::mutex> lock(mutex_);

        auto& roomObjects = rooms_[roomId];
        auto it = roomObjects.find(beanName);
        if (it != roomObjects.end()) {
            return it->second;
        }

        auto obj = factory();
        roomObjects[beanName] = obj;
        return obj;
    }

    void Remove(const std::string& beanName) override {
        std::string roomId = GetCurrentRoomId();
        if (roomId.empty()) return;

        std::lock_guard<std::mutex> lock(mutex_);

        auto it = rooms_.find(roomId);
        if (it != rooms_.end()) {
            it->second.erase(beanName);
            if (it->second.empty()) {
                rooms_.erase(it);
            }
        }
    }

    void Clear() override {
        std::string roomId = GetCurrentRoomId();
        if (!roomId.empty()) {
            CleanupRoom(roomId);
        }
    }

    size_t Size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t total = 0;
        for (const auto& pair : rooms_) {
            total += pair.second.size();
        }
        return total;
    }

    void CleanupRoom(const std::string& roomId) {
        std::lock_guard<std::mutex> lock(mutex_);
        rooms_.erase(roomId);
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string,
        std::unordered_map<std::string, std::shared_ptr<void>>> rooms_;

    std::string GetCurrentRoomId() const {
        // 从上下文中获取当前房间ID
        // ...
        return "";
    }
};

// 注册自定义 Scope
int main() {
    auto& scopeMgr = ScopeManager::Instance();

    // 注册自定义 Scope
    scopeMgr.RegisterScope("room", std::make_unique<RoomScope>());

    // 使用自定义 Scope
    auto& app = ApplicationContext::Instance();
    app.RegisterBean<GameState>("gameState", "room");

    // ...
}
```

## 9. 性能优化建议

1. **延迟初始化**：Lazy-init Beans 只在第一次使用时创建
2. **对象池化**：对频繁创建销毁的 Prototype 对象使用对象池
3. **缓存优化**：Scope 内部使用更高效的缓存策略
4. **异步清理**：Scope 清理使用异步任务，避免阻塞主线程
5. **内存管理**：使用 weak_ptr 避免循环引用

## 10. 注意事项

1. **线程安全**：所有 Scope 实现都需要考虑线程安全问题
2. **资源泄漏**：确保 Scope 清理时正确释放资源
3. **性能影响**：Scope 查找会增加一定的开销，需要权衡
4. **调试支持**：提供 Scope 状态查询和调试接口
5. **集成要求**：Request/Session Scope 需要与网络框架集成