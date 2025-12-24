#pragma once

#ifdef HAVE_FRUIT
#include <fruit/fruit.h>

// 使用 Fruit 提供的类型别名
namespace fruit {
    using PartialComponentVoid = PartialComponent<>;
}
#else
// 简化的内置 DI 组件类型定义
namespace fruit {
    // 前向声明
    template<typename...>
    struct Component;

    // 空的 PartialComponent 占位符
    struct PartialComponentVoid {
        // 占位符
    };

    struct Injector {
        // 占位符
    };

    // 内置的简化版本
    inline PartialComponentVoid createComponent() {
        return PartialComponentVoid();
    }
}
#endif

#include <string>
#include <vector>
#include <memory>

namespace Apollo::Starter {

/**
 * @brief Starter 优先级枚举
 *
 * 类似 Spring Boot 的 @Order 注解，控制 Starter 的加载顺序
 */
enum class StarterOrder {
    HIGHEST_PRECEDENCE = -1000,
    VERY_HIGH_PRECEDENCE = -500,
    HIGH_PRECEDENCE = -100,
    MEDIUM_PRECEDENCE = 0,
    LOW_PRECEDENCE = 100,
    VERY_LOW_PRECEDENCE = 500,
    LOWEST_PRECEDENCE = 1000
};

/**
 * @brief Starter 配置元数据
 *
 * 包含 Starter 的名称、描述、优先级等信息
 */
struct StarterMetadata {
    std::string name;                    // Starter 名称，如 "network-starter"
    std::string description;             // 描述信息
    int order;                           // 加载顺序，数值越小越优先
    std::vector<std::string> dependsOn;  // 依赖的其他 Starter
    bool autoEnabled;                    // 是否默认自动启用

    StarterMetadata(std::string n = "", int o = static_cast<int>(StarterOrder::MEDIUM_PRECEDENCE))
        : name(std::move(n)), order(o), autoEnabled(true) {}
};

/**
 * @brief ApolloStarter 基类
 *
 * 所有 Starter 的抽象基类，提供类似 Spring Boot Starter 的功能：
 * - 自动配置注册
 * - 条件装配支持
 * - 依赖注入集成
 * - 生命周期管理
 *
 * 使用示例：
 * @code
 * class NetworkStarter : public ApolloStarter {
 * public:
 *     NetworkStarter() {
 *         setMetadata(StarterMetadata("network-starter")
 *             .setDescription("Network communication auto-configuration")
 *             .setOrder(StarterOrder::HIGH_PRECEDENCE));
 *     }
 *
 *     fruit::Component<> getComponent() override {
 *         return fruit::createComponent()
 *             .registerFactory<NetworkService>()
 *             .registerFactory<Reactor>();
 *     }
 *
 *     bool matches(const ConditionContext& ctx) override {
 *         return ctx.getProperty("network.enabled", true);
 *     }
 * };
 * @endcode
 */
class ApolloStarter {
public:
    virtual ~ApolloStarter() = default;

    /**
     * @brief 获取 Starter 的 Fruit 组件配置
     * @return Fruit PartialComponentVoid，包含此 Starter 注册的所有服务
     *
     * 注意：返回的是 PartialComponentVoid，可以链式调用绑定方法
     */
    virtual fruit::PartialComponentVoid getComponent() {
        return fruit::createComponent();
    }

    /**
     * @brief 条件匹配判断
     * @param ctx 条件上下文，包含配置属性等信息
     * @return true 表示应该启用此 Starter
     *
     * 类似 Spring Boot 的 @ConditionalOnProperty、@ConditionalOnClass 等
     */
    virtual bool matches(const struct ConditionContext& ctx) {
        (void)ctx;
        return true;  // 默认启用
    }

    /**
     * @brief Starter 初始化回调
     *
     * 在组件注册完成后、注入器创建之前调用
     */
    virtual void onInitialize() {}

    /**
     * @brief Starter 启动回调
     *
     * 在注入器创建完成后调用
     */
    virtual void onStart() {}

    /**
     * @brief Starter 停止回调
     */
    virtual void onStop() {}

    /**
     * @brief 获取 Starter 元数据
     */
    const StarterMetadata& getMetadata() const { return metadata_; }

    /**
     * @brief 设置 Starter 元数据
     */
    void setMetadata(const StarterMetadata& metadata) { metadata_ = metadata; }

protected:
    ApolloStarter() = default;

private:
    StarterMetadata metadata_;
};

/**
 * @brief 条件上下文
 *
 * 用于条件判断的上下文信息
 */
struct ConditionContext {
    virtual ~ConditionContext() = default;

    /**
     * @brief 获取配置属性值
     * @param key 配置键
     * @param defaultValue 默认值
     * @return 配置值
     */
    virtual bool getProperty(const std::string& key, bool defaultValue) const = 0;
    virtual int getProperty(const std::string& key, int defaultValue) const = 0;
    virtual std::string getProperty(const std::string& key, const std::string& defaultValue) const = 0;

    /**
     * @brief 检查类是否可用（是否已链接）
     */
    virtual bool isClassAvailable(const std::string& className) const = 0;

    /**
     * @brief 检查 Bean 是否已存在
     */
    virtual bool isBeanPresent(const std::string& beanName) const = 0;

    /**
     * @brief 检查指定的 Starter 是否已启用
     */
    virtual bool isStarterEnabled(const std::string& starterName) const = 0;
};

} // namespace Apollo::Starter
