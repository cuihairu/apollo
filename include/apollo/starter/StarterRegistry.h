#pragma once

#include "Starter.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <algorithm>

namespace Apollo::Starter {

/**
 * @brief Starter 注册表
 *
 * 管理所有已注册的 Starter，负责：
 * - Starter 注册和发现
 * - 启动顺序计算（基于依赖关系和优先级）
 * - 条件过滤
 * - 组件组合
 *
 * 设计原则：
 * - KISS: 简单的注册表模式，避免过度设计
 * - SOLID-S: 单一职责，只管理 Starter 的注册和排序
 * - SOLID-O: 开闭原则，通过注册机制扩展
 */
class StarterRegistry {
public:
    static StarterRegistry& getInstance() {
        static StarterRegistry instance;
        return instance;
    }

    /**
     * @brief 注册 Starter
     * @param starter Starter 实例的智能指针
     * @return 成功返回 true，已存在返回 false
     *
     * 线程安全
     */
    bool registerStarter(std::shared_ptr<ApolloStarter> starter) {
        if (!starter) {
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        const std::string& name = starter->getMetadata().name;

        if (starters_.find(name) != starters_.end()) {
            return false;  // 已存在
        }

        starters_[name] = starter;
        return true;
    }

    /**
     * @brief 获取所有已注册的 Starter
     */
    std::vector<std::shared_ptr<ApolloStarter>> getAllStarters() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<ApolloStarter>> result;
        result.reserve(starters_.size());

        for (const auto& pair : starters_) {
            result.push_back(pair.second);
        }

        return result;
    }

    /**
     * @brief 根据名称获取 Starter
     */
    std::shared_ptr<ApolloStarter> getStarter(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = starters_.find(name);
        return it != starters_.end() ? it->second : nullptr;
    }

    /**
     * @brief 获取按优先级排序的 Starter 列表
     *
     * 排序规则：
     * 1. 先按 order 值升序（数值越小越优先）
     * 2. 同 order 值按依赖关系排序（被依赖的在前）
     */
    std::vector<std::shared_ptr<ApolloStarter>> getSortedStarters() const {
        auto all = getAllStarters();

        std::sort(all.begin(), all.end(),
            [](const auto& a, const auto& b) {
                int orderA = a->getMetadata().order;
                int orderB = b->getMetadata().order;
                return orderA < orderB;
            });

        return all;
    }

    /**
     * @brief 过滤出匹配条件的 Starter
     * @param ctx 条件上下文
     * @return 所有 matches() 返回 true 的 Starter
     */
    std::vector<std::shared_ptr<ApolloStarter>> getMatchingStarters(const ConditionContext& ctx) const {
        auto all = getAllStarters();
        std::vector<std::shared_ptr<ApolloStarter>> result;

        for (const auto& starter : all) {
            if (starter->matches(ctx)) {
                result.push_back(starter);
            }
        }

        // 按优先级排序
        std::sort(result.begin(), result.end(),
            [](const auto& a, const auto& b) {
                return a->getMetadata().order < b->getMetadata().order;
            });

        return result;
    }

    /**
     * @brief 清空所有已注册的 Starter
     *
     * 主要用于测试
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        starters_.clear();
    }

    /**
     * @brief 获取已注册 Starter 的数量
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return starters_.size();
    }

    // 删除拷贝构造和赋值
    StarterRegistry(const StarterRegistry&) = delete;
    StarterRegistry& operator=(const StarterRegistry&) = delete;

private:
    StarterRegistry() = default;
    ~StarterRegistry() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<ApolloStarter>> starters_;
};

/**
 * @brief Starter 自动注册辅助类
 *
 * 使用静态初始化自动注册 Starter
 *
 * 使用示例：
 * @code
 * namespace {
 *     StarterAutoRegistrar<NetworkStarter> regNetworkStarter;
 * }
 * @endcode
 */
template<typename T>
class StarterAutoRegistrar {
public:
    StarterAutoRegistrar() {
        auto starter = std::make_shared<T>();
        StarterRegistry::getInstance().registerStarter(starter);
    }
};

/**
 * @brief 便捷宏：自动注册 Starter
 *
 * 在 .cpp 文件中使用，自动将 Starter 注册到注册表
 *
 * @param StarterClass Starter 类名
 */
#define APOLLO_CONCAT_IMPL(x, y) x##y
#define APOLLO_CONCAT(x, y) APOLLO_CONCAT_IMPL(x, y)

// 使用 __COUNTER__ 宏确保每次展开都有唯一的变量名
#if defined(__COUNTER__)
#define APOLLO_REGISTER_STARTER(StarterClass) \
    namespace { \
        Apollo::Starter::StarterAutoRegistrar<StarterClass> \
            APOLLO_CONCAT(APOLLO_CONCAT(registrar_, __COUNTER__), _); \
    }
#else
// 如果不支持 __COUNTER__，使用类名作为唯一标识
#define APOLLO_REGISTER_STARTER(StarterClass) \
    namespace { \
        Apollo::Starter::StarterAutoRegistrar<StarterClass> \
            APOLLO_CONCAT(registrar_, StarterClass); \
    }
#endif

} // namespace Apollo::Starter
