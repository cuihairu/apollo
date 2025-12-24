#include "apollo/starter/ApolloApplication.h"
#include <fstream>
#include <sstream>

namespace Apollo::Starter {

void ApolloApplication::start() {
    for (const auto& starter : activeStarters_) {
        starter->onStart();
    }
    started_ = true;
}

void ApolloApplication::stop() {
    // 反向停止
    for (auto it = activeStarters_.rbegin(); it != activeStarters_.rend(); ++it) {
        (*it)->onStop();
    }
    started_ = false;
}

void ApolloApplication::initialize() {
    // 1. 加载配置文件
    loadConfiguration();

    // 2. 创建条件上下文
    auto context = createConditionContext();

    // 3. 获取匹配的 Starter
    auto matchingStarters = StarterRegistry::getInstance().getMatchingStarters(*context);

    // 4. 过滤出启用的 Starter
    activeStarters_ = filterEnabledStarters(matchingStarters);

    // 5. 初始化 Starter
    for (const auto& starter : activeStarters_) {
        starter->onInitialize();
    }

#ifdef HAVE_FRUIT
    // 6. 组合所有 Starter 的组件
    // TODO: 实现组件组合逻辑
    // Fruit 的组件组合需要使用 NormalizedComponent 和 install 机制

    // 7. 创建注入器
    // TODO: 正确构造 Fruit Injector
    // injector_ = std::unique_ptr<fruit::Injector<>>(new fruit::Injector<>(getCombinedComponent));
#endif
}

void ApolloApplication::loadConfiguration() {
    // 如果指定了配置文件，从文件加载
    if (!configPath_.empty()) {
        std::ifstream file(configPath_);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();

            // 根据文件扩展名决定解析方式
            if (configPath_.find(".json") != std::string::npos) {
#ifdef HAVE_NLOHMANN_JSON
                // JSON 解析将在 DefaultConditionContext 中处理
#endif
            } else {
                // 默认 key=value 格式
                // properties_ 已在 Builder 中设置
            }
        }
    }

    // TODO: 整合 ConfigManager 来加载配置
}

std::unique_ptr<ConditionContext> ApolloApplication::createConditionContext() {
    auto ctx = std::make_unique<DefaultConditionContext>();

    // 设置配置属性
    for (const auto& pair : properties_) {
        ctx->setProperty(pair.first, pair.second);
    }

    return ctx;
}

std::vector<std::shared_ptr<ApolloStarter>> ApolloApplication::filterEnabledStarters(
    const std::vector<std::shared_ptr<ApolloStarter>>& starters) {

    std::vector<std::shared_ptr<ApolloStarter>> result;

    for (const auto& starter : starters) {
        const std::string& name = starter->getMetadata().name;

        // 检查是否被显式禁用
        if (std::find(disabledStarters_.begin(), disabledStarters_.end(), name) != disabledStarters_.end()) {
            continue;
        }

        // 如果有启用列表，只启用列表中的
        if (!enabledStarters_.empty() &&
            std::find(enabledStarters_.begin(), enabledStarters_.end(), name) == enabledStarters_.end()) {
            continue;
        }

        // 检查是否自动启用
        if (starter->getMetadata().autoEnabled) {
            result.push_back(starter);
        }
    }

    return result;
}

#ifdef HAVE_FRUIT
fruit::PartialComponentVoid ApolloApplication::combineStarterComponents(
    const std::vector<std::shared_ptr<ApolloStarter>>& starters) {
    (void)starters;  // 避免未使用参数警告

    // TODO: 实现组件组合逻辑
    // Fruit 的组件组合需要使用 install 机制
    // 这里返回一个空组件作为占位符
    return fruit::createComponent();
}
#endif

} // namespace Apollo::Starter
