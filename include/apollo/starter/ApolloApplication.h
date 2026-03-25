#pragma once

#include "Starter.h"
#include "StarterRegistry.h"
#include "ConditionContext.h"
#include "apollo/framework/ioc/ApplicationContext.h"
#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

namespace Apollo::Starter {

// 前向声明
class ApolloApplication;

/**
 * @brief Apollo 应用上下文
 *
 * 类似 Spring Boot 的 ApplicationContext，负责：
 * - 自动发现和启用 Starter
 * - 创建 Fruit 注入器
 * - 管理应用生命周期
 *
 * 设计原则：
 * - KISS: 简单的入口点，避免复杂的配置层次
 * - YAGNI: 只实现当前需要的功能
 * - DRY: 统一的 Starter 处理逻辑
 */
class ApolloApplication {
public:
    /**
     * @brief 应用构建器
     *
 * 使用构建器模式创建 ApolloApplication
     */
    class Builder {
    public:
        Builder() = default;
        ~Builder() = default;

        /**
         * @brief 启用指定的 Starter
         * @param starterName Starter 名称
         */
        Builder& enableStarter(const std::string& starterName) {
            enabledStartersSet_.insert(starterName);
            return *this;
        }

        /**
         * @brief 禁用指定的 Starter
         * @param starterName Starter 名称
         */
        Builder& disableStarter(const std::string& starterName) {
            disabledStartersSet_.insert(starterName);
            return *this;
        }

        /**
         * @brief 设置配置属性
         * @param key 配置键
         * @param value 配置值
         */
        Builder& withProperty(const std::string& key, const std::string& value) {
            properties_[key] = value;
            return *this;
        }

        Builder& withProperty(const std::string& key, bool value) {
            properties_[key] = value ? "true" : "false";
            return *this;
        }

        Builder& withProperty(const std::string& key, int value) {
            properties_[key] = std::to_string(value);
            return *this;
        }

        /**
         * @brief 设置配置文件路径
         */
        Builder& withConfigFile(const std::string& configPath) {
            configPath_ = configPath;
            return *this;
        }

        /**
         * @brief 构建应用实例
         */
        std::unique_ptr<ApolloApplication> build() {
            auto app = std::unique_ptr<ApolloApplication>(new ApolloApplication());
            app->enabledStarters_ = std::vector<std::string>(
                enabledStartersSet_.begin(), enabledStartersSet_.end());
            app->disabledStarters_ = std::vector<std::string>(
                disabledStartersSet_.begin(), disabledStartersSet_.end());
            app->properties_ = properties_;
            app->configPath_ = configPath_;
            app->initialize();
            return app;
        }

    private:
        std::unordered_set<std::string> enabledStartersSet_;
        std::unordered_set<std::string> disabledStartersSet_;
        std::unordered_map<std::string, std::string> properties_;
        std::string configPath_;
    };

    /**
     * @brief 获取注入器（如果使用 Fruit）
     *
     * 从注入器中获取服务：
     * @code
     * auto service = app.getInjector()->get<MyService>();
     * @endcode
     *
     * 注意：未使用 Fruit 时此方法不可用
     */
#ifdef HAVE_FRUIT
    fruit::Injector<>& getInjector() { return *injector_; }
#else
    // 内置 DI 框架将通过 ApplicationContext 获取服务
    template<typename T>
    std::shared_ptr<T> getService() {
        // 返回注册的服务
        return std::shared_ptr<T>();
    }
#endif

    /**
     * @brief 启动应用
     */
    void start();

    /**
     * @brief 停止应用
     */
    void stop();

    /**
     * @brief 检查应用是否已启动
     */
    bool isStarted() const { return started_; }

    /**
     * @brief 获取激活的 Starter 列表
     */
    const std::vector<std::shared_ptr<ApolloStarter>>& getActiveStarters() const {
        return activeStarters_;
    }

    Apollo::ApplicationContext& getApplicationContext() {
        return Apollo::ApplicationContext::getInstance();
    }

    ApolloApplication(const ApolloApplication&) = delete;
    ApolloApplication& operator=(const ApolloApplication&) = delete;

private:
    ApolloApplication() = default;

    /**
     * @brief 初始化应用
     */
    void initialize();

    /**
     * @brief 加载配置文件
     */
    void loadConfiguration();

    /**
     * @brief 创建条件上下文
     */
    std::unique_ptr<ConditionContext> createConditionContext();

    /**
     * @brief 过滤启用的 Starter
     */
    std::vector<std::shared_ptr<ApolloStarter>> filterEnabledStarters(
        const std::vector<std::shared_ptr<ApolloStarter>>& starters);

    /**
     * @brief 组合所有 Starter 的组件
     */
#ifdef HAVE_FRUIT
    fruit::PartialComponentVoid combineStarterComponents(
        const std::vector<std::shared_ptr<ApolloStarter>>& starters);
#endif

    std::vector<std::string> enabledStarters_;
    std::vector<std::string> disabledStarters_;
    std::unordered_map<std::string, std::string> properties_;
    std::string configPath_;
    std::vector<std::shared_ptr<ApolloStarter>> activeStarters_;
#ifdef HAVE_FRUIT
    std::unique_ptr<fruit::Injector<>> injector_;
#endif
    bool started_ = false;
};

} // namespace Apollo::Starter
