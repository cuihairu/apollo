#pragma once

#include "Starter.h"
#include <string>
#include <functional>

namespace Apollo::Starter {

/**
 * @brief 条件装配辅助类
 *
 * 类似 Spring Boot 的条件注解，提供便捷的条件判断
 *
 * 设计原则：
 * - KISS: 简单的条件表达式
 * - DRY: 复用条件上下文
 * - YAGNI: 只实现常用条件
 */

/**
 * @brief 属性条件：当配置属性匹配时启用
 *
 * 类似 Spring 的 @ConditionalOnProperty
 */
class ConditionalOnProperty {
public:
    explicit ConditionalOnProperty(std::string key)
        : key_(std::move(key)), hasValue_(false) {}

    ConditionalOnProperty(std::string key, std::string value)
        : key_(std::move(key)), value_(std::move(value)), hasValue_(true) {}

    ConditionalOnProperty(std::string key, bool expectedValue)
        : key_(std::move(key)), value_(expectedValue ? "true" : "false"), hasValue_(true) {}

    bool matches(const ConditionContext& ctx) const {
        // 获取属性值作为字符串
        std::string defaultValue("");
        std::string actualValue = ctx.getProperty(key_, defaultValue);
        bool hasProperty = !actualValue.empty();

        if (!hasProperty) {
            // 如果 key 不存在
            return !hasValue_;  // 如果没有期望值，则返回 true
        }

        if (hasValue_) {
            return actualValue == value_;
        }
        // 只检查属性存在
        return true;
    }

    const std::string& key() const { return key_; }

private:
    std::string key_;
    std::string value_;
    bool hasValue_;
};

/**
 * @brief 类存在条件：当类可用时启用
 *
 * 类似 Spring 的 @ConditionalOnClass
 */
class ConditionalOnClass {
public:
    explicit ConditionalOnClass(std::string className)
        : className_(std::move(className)) {}

    bool matches(const ConditionContext& ctx) const {
        return ctx.isClassAvailable(className_);
    }

    const std::string& className() const { return className_; }

private:
    std::string className_;
};

/**
 * @brief Bean 存在条件：当 Bean 已注册时启用
 *
 * 类似 Spring 的 @ConditionalOnBean
 */
class ConditionalOnBean {
public:
    explicit ConditionalOnBean(std::string beanName)
        : beanName_(std::move(beanName)) {}

    bool matches(const ConditionContext& ctx) const {
        return ctx.isBeanPresent(beanName_);
    }

    const std::string& beanName() const { return beanName_; }

private:
    std::string beanName_;
};

/**
 * @brief Starter 启用条件：当指定 Starter 启用时启用
 */
class ConditionalOnStarter {
public:
    explicit ConditionalOnStarter(std::string starterName)
        : starterName_(std::move(starterName)) {}

    bool matches(const ConditionContext& ctx) const {
        return ctx.isStarterEnabled(starterName_);
    }

    const std::string& starterName() const { return starterName_; }

private:
    std::string starterName_;
};

/**
 * @brief 自定义条件：使用自定义函数判断
 */
class CustomConditional {
public:
    using MatchFunction = std::function<bool(const ConditionContext&)>;

    explicit CustomConditional(MatchFunction func)
        : matchFunc_(std::move(func)) {}

    bool matches(const ConditionContext& ctx) const {
        return matchFunc_ ? matchFunc_(ctx) : true;
    }

private:
    MatchFunction matchFunc_;
};

/**
 * @brief 多条件组合：所有条件都满足才启用 (AND)
 *
 * 类似 Spring 的 @ConditionalOnExpression 的 AND 逻辑
 */
class AllOf {
public:
    AllOf() = default;

    AllOf& add(ConditionalOnProperty cond) {
        conditions_.push_back([cond](const ConditionContext& ctx) {
            return cond.matches(ctx);
        });
        return *this;
    }

    AllOf& add(ConditionalOnClass cond) {
        conditions_.push_back([cond](const ConditionContext& ctx) {
            return cond.matches(ctx);
        });
        return *this;
    }

    AllOf& add(ConditionalOnBean cond) {
        conditions_.push_back([cond](const ConditionContext& ctx) {
            return cond.matches(ctx);
        });
        return *this;
    }

    AllOf& add(ConditionalOnStarter cond) {
        conditions_.push_back([cond](const ConditionContext& ctx) {
            return cond.matches(ctx);
        });
        return *this;
    }

    bool matches(const ConditionContext& ctx) const {
        for (const auto& cond : conditions_) {
            if (!cond(ctx)) {
                return false;
            }
        }
        return true;
    }

private:
    std::vector<std::function<bool(const ConditionContext&)>> conditions_;
};

/**
 * @brief 多条件组合：任一条件满足即启用 (OR)
 */
class AnyOf {
public:
    AnyOf() = default;

    AnyOf& add(ConditionalOnProperty cond) {
        conditions_.push_back([cond](const ConditionContext& ctx) {
            return cond.matches(ctx);
        });
        return *this;
    }

    AnyOf& add(ConditionalOnClass cond) {
        conditions_.push_back([cond](const ConditionContext& ctx) {
            return cond.matches(ctx);
        });
        return *this;
    }

    AnyOf& add(ConditionalOnBean cond) {
        conditions_.push_back([cond](const ConditionContext& ctx) {
            return cond.matches(ctx);
        });
        return *this;
    }

    bool matches(const ConditionContext& ctx) const {
        if (conditions_.empty()) {
            return true;
        }
        for (const auto& cond : conditions_) {
            if (cond(ctx)) {
                return true;
            }
        }
        return false;
    }

private:
    std::vector<std::function<bool(const ConditionContext&)>> conditions_;
};

/**
 * @brief 否定条件：条件不满足时启用
 */
class NoneOf {
public:
    NoneOf() = default;

    NoneOf& add(ConditionalOnProperty cond) {
        conditions_.push_back([cond](const ConditionContext& ctx) {
            return !cond.matches(ctx);
        });
        return *this;
    }

    NoneOf& add(ConditionalOnClass cond) {
        conditions_.push_back([cond](const ConditionContext& ctx) {
            return !cond.matches(ctx);
        });
        return *this;
    }

    bool matches(const ConditionContext& ctx) const {
        for (const auto& cond : conditions_) {
            if (!cond(ctx)) {
                return false;
            }
        }
        return true;
    }

private:
    std::vector<std::function<bool(const ConditionContext&)>> conditions_;
};

} // namespace Apollo::Starter
