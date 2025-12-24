#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <thread>
#include <vector>
#include <sstream>

namespace apollo {

// ========== 基础接口定义 ==========

// Scope 基接口
class IScope {
public:
    virtual ~IScope() = default;
    virtual std::string GetName() const = 0;
    virtual std::shared_ptr<void> Get(const std::string& beanName,
                                    std::function<std::shared_ptr<void>()> factory) = 0;
    virtual void Remove(const std::string& beanName) = 0;
    virtual size_t Size() const = 0;
};

// Bean 定义
struct BeanDefinition {
    std::string name;
    std::string typeName;
    std::string scope = "singleton";
    std::function<std::shared_ptr<void>()> factory;
};

// ========== Singleton Scope 实现 ==========

class SingletonScope : public IScope {
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<void>> objects_;

public:
    std::string GetName() const override {
        return "singleton";
    }

    std::shared_ptr<void> Get(const std::string& beanName,
                            std::function<std::shared_ptr<void>()> factory) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = objects_.find(beanName);
        if (it != objects_.end()) {
            std::cout << "[Singleton] 返回已存在的实例: " << beanName << std::endl;
            return it->second;
        }

        std::cout << "[Singleton] 创建新实例: " << beanName << std::endl;
        auto obj = factory();
        objects_[beanName] = obj;
        return obj;
    }

    void Remove(const std::string& beanName) override {
        std::lock_guard<std::mutex> lock(mutex_);
        objects_.erase(beanName);
    }

    size_t Size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return objects_.size();
    }
};

// ========== Prototype Scope 实现 ==========

class PrototypeScope : public IScope {
public:
    std::string GetName() const override {
        return "prototype";
    }

    std::shared_ptr<void> Get(const std::string& beanName,
                            std::function<std::shared_ptr<void>()> factory) override {
        std::cout << "[Prototype] 每次都创建新实例: " << beanName << std::endl;
        return factory();
    }

    void Remove(const std::string& beanName) override {
        // Prototype 不缓存对象
    }

    size_t Size() const override {
        return 0;
    }
};

// ========== Thread Scope 实现 ==========

class ThreadScope : public IScope {
private:
    struct ThreadLocalData {
        std::unordered_map<std::string, std::shared_ptr<void>> objects;
        std::mutex mutex;
        std::thread::id threadId;

        ThreadLocalData() {
            threadId = std::this_thread::get_id();
        }
    };

    // C++11 thread_local 支持
    static thread_local ThreadLocalData threadLocal_;

public:
    std::string GetName() const override {
        return "thread";
    }

    std::shared_ptr<void> Get(const std::string& beanName,
                            std::function<std::shared_ptr<void>()> factory) override {
        std::lock_guard<std::mutex> lock(threadLocal_.mutex);

        auto it = threadLocal_.objects.find(beanName);
        if (it != threadLocal_.objects.end()) {
            std::cout << "[Thread-" << threadLocal_.threadId << "] 返回线程本地实例: "
                      << beanName << std::endl;
            return it->second;
        }

        std::cout << "[Thread-" << threadLocal_.threadId << "] 创建线程本地新实例: "
                  << beanName << std::endl;
        auto obj = factory();
        threadLocal_.objects[beanName] = obj;
        return obj;
    }

    void Remove(const std::string& beanName) override {
        std::lock_guard<std::mutex> lock(threadLocal_.mutex);
        threadLocal_.objects.erase(beanName);
    }

    size_t Size() const override {
        std::lock_guard<std::mutex> lock(threadLocal_.mutex);
        return threadLocal_.objects.size();
    }

    // 获取当前线程的对象数量
    static size_t GetCurrentThreadSize() {
        std::lock_guard<std::mutex> lock(threadLocal_.mutex);
        return threadLocal_.objects.size();
    }
};

// thread_local 成员定义
thread_local ThreadScope::ThreadLocalData ThreadScope::threadLocal_;

// ========== Scope 管理器 ==========

class ScopeManager {
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<IScope>> scopes_;

public:
    static ScopeManager& Instance() {
        static ScopeManager instance;
        return instance;
    }

    void Initialize() {
        RegisterScope("singleton", std::make_unique<SingletonScope>());
        RegisterScope("prototype", std::make_unique<PrototypeScope>());
        RegisterScope("thread", std::make_unique<ThreadScope>());
    }

    void RegisterScope(const std::string& name, std::unique_ptr<IScope> scope) {
        std::lock_guard<std::mutex> lock(mutex_);
        scopes_[name] = std::move(scope);
        std::cout << "[ScopeManager] 注册 Scope: " << name << std::endl;
    }

    IScope* GetScope(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = scopes_.find(name);
        if (it == scopes_.end()) {
            throw std::runtime_error("Unknown scope: " + name);
        }
        return it->second.get();
    }

    void PrintStatistics() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\n=== Scope 统计信息 ===" << std::endl;
        for (const auto& pair : scopes_) {
            std::cout << "Scope '" << pair.first << "': "
                      << pair.second->Size() << " 个对象" << std::endl;
        }
        std::cout << "===================\n" << std::endl;
    }
};

// ========== 简化的应用上下文 ==========

class ApplicationContext {
private:
    ScopeManager& scopeManager_;
    std::unordered_map<std::string, BeanDefinition> beanDefinitions_;

public:
    ApplicationContext() : scopeManager_(ScopeManager::Instance()) {}

    template<typename T>
    void RegisterBean(const std::string& name, const std::string& scope = "singleton") {
        BeanDefinition def;
        def.name = name;
        def.typeName = typeid(T).name();
        def.scope = scope;
        def.factory = [this]() -> std::shared_ptr<void> {
            return std::make_shared<T>();
        };

        beanDefinitions_[name] = def;
        std::cout << "[ApplicationContext] 注册 Bean: " << name << " (Scope: " << scope << ")" << std::endl;
    }

    template<typename T>
    std::shared_ptr<T> GetBean(const std::string& name) {
        auto it = beanDefinitions_.find(name);
        if (it == beanDefinitions_.end()) {
            throw std::runtime_error("Bean not found: " + name);
        }

        const auto& def = it->second;
        auto scope = scopeManager_.GetScope(def.scope);

        auto obj = scope->Get(name, [this, &def]() -> std::shared_ptr<void> {
            std::cout << "  -> 创建 " << def.name << " 的实例 (类型: "
                      << def.typeName.substr(def.typeName.find_last_of(' ') + 1) << ")" << std::endl;
            return def.factory();
        });

        return std::static_pointer_cast<T>(obj);
    }

    void Initialize() {
        scopeManager_.Initialize();
        std::cout << "\n[ApplicationContext] 初始化完成！" << std::endl;
    }
};

} // namespace apollo