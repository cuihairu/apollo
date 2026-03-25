#include "apollo/starter/ApolloApplication.h"
#include "apollo/core/config/config_manager.h"
#include "apollo/framework/ioc/ApplicationContext.h"
#include "apollo/framework/ioc/ConfigEnvironment.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <cctype>
#include <vector>

#if defined(_WIN32)
extern char** _environ;
#else
extern char** environ;
#endif

#ifdef HAVE_NLOHMANN_JSON
    #include <nlohmann/json.hpp>
#endif

namespace Apollo::Starter {

namespace {

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string trim(std::string s) {
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && isSpace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && isSpace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

std::string stripOptionalQuotes(std::string s) {
    s = trim(std::move(s));
    if (s.size() >= 2) {
        char first = s.front();
        char last = s.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return s.substr(1, s.size() - 2);
        }
    }
    return s;
}

std::unordered_map<std::string, std::string> parseIniLikeProperties(const std::string& content) {
    std::unordered_map<std::string, std::string> props;
    std::istringstream iss(content);

    std::string line;
    std::string section;

    while (std::getline(iss, line)) {
        line = trim(std::move(line));
        if (line.empty()) {
            continue;
        }
        if (line.rfind("#", 0) == 0 || line.rfind(";", 0) == 0) {
            continue;
        }

        if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }

        auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }

        std::string key = trim(line.substr(0, pos));
        std::string value = stripOptionalQuotes(line.substr(pos + 1));
        if (key.empty()) {
            continue;
        }

        std::string fullKey = section.empty() ? key : (section + "/" + key);
        props[std::move(fullKey)] = std::move(value);
    }

    return props;
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string normalizeEnvironmentKey(std::string s) {
    s = toLower(std::move(s));
    std::replace(s.begin(), s.end(), '_', '.');
    return s;
}

std::unordered_map<std::string, std::string> readEnvironmentOverrides() {
    std::unordered_map<std::string, std::string> props;

#if defined(_WIN32)
    char** env = _environ;
#else
    char** env = environ;
#endif

    if (env == nullptr) {
        return props;
    }

    const std::vector<std::string> prefixes = {"APOLLO_", "APP_"};
    for (char** current = env; *current != nullptr; ++current) {
        std::string entry(*current);
        const auto pos = entry.find('=');
        if (pos == std::string::npos) {
            continue;
        }

        std::string key = entry.substr(0, pos);
        std::string value = entry.substr(pos + 1);

        for (const auto& prefix : prefixes) {
            if (key.rfind(prefix, 0) == 0 && key.size() > prefix.size()) {
                props[normalizeEnvironmentKey(key.substr(prefix.size()))] = value;
                break;
            }
        }
    }

    return props;
}

#ifdef HAVE_NLOHMANN_JSON
void flattenJsonToProperties(const nlohmann::json& j,
                             const std::string& prefix,
                             std::unordered_map<std::string, std::string>& out) {
    auto join = [&](const std::string& key) -> std::string {
        return prefix.empty() ? key : (prefix + "/" + key);
    };

    if (j.is_object()) {
        for (auto it = j.begin(); it != j.end(); ++it) {
            flattenJsonToProperties(it.value(), join(it.key()), out);
        }
        return;
    }

    if (j.is_array()) {
        bool allPrimitive = true;
        for (const auto& item : j) {
            if (!(item.is_string() || item.is_boolean() || item.is_number() || item.is_null())) {
                allPrimitive = false;
                break;
            }
        }

        if (allPrimitive) {
            std::string joined;
            for (size_t i = 0; i < j.size(); ++i) {
                if (i > 0) joined += ",";
                const auto& item = j[i];
                if (item.is_string()) joined += item.get<std::string>();
                else if (item.is_boolean()) joined += (item.get<bool>() ? "true" : "false");
                else if (item.is_number_integer()) joined += std::to_string(item.get<int64_t>());
                else if (item.is_number_float()) joined += std::to_string(item.get<double>());
            }
            if (!prefix.empty()) {
                out[prefix] = std::move(joined);
            }
            return;
        }

        if (!prefix.empty()) {
            out[prefix] = j.dump();
        }
        return;
    }

    if (prefix.empty()) {
        return;
    }

    if (j.is_string()) {
        out[prefix] = j.get<std::string>();
    } else if (j.is_boolean()) {
        out[prefix] = j.get<bool>() ? "true" : "false";
    } else if (j.is_number_integer()) {
        out[prefix] = std::to_string(j.get<int64_t>());
    } else if (j.is_number_float()) {
        out[prefix] = std::to_string(j.get<double>());
    } else if (j.is_null()) {
        out[prefix] = "";
    } else {
        out[prefix] = j.dump();
    }
}

std::unordered_map<std::string, std::string> parseJsonProperties(const std::string& content) {
    std::unordered_map<std::string, std::string> props;
    try {
        nlohmann::json j = nlohmann::json::parse(content);
        flattenJsonToProperties(j, "", props);
    } catch (...) {
    }
    return props;
}
#endif

} // namespace

void ApolloApplication::start() {
    if (started_) {
        return;
    }

    auto& appContext = Apollo::ApplicationContext::getInstance();
    if (!appContext.initializeComponents()) {
        throw std::runtime_error("Failed to initialize application components");
    }
    if (!appContext.startComponents()) {
        throw std::runtime_error("Failed to start application components");
    }

    for (const auto& starter : activeStarters_) {
        starter->onStart();
    }
    started_ = true;
}

void ApolloApplication::stop() {
    if (!started_) {
        return;
    }

    // 反向停止
    for (auto it = activeStarters_.rbegin(); it != activeStarters_.rend(); ++it) {
        (*it)->onStop();
    }

    auto& appContext = Apollo::ApplicationContext::getInstance();
    appContext.stopComponents();
    appContext.destroyComponents();
    started_ = false;
}

void ApolloApplication::initialize() {
    // 1. 加载配置文件
    loadConfiguration();
    auto& appContext = Apollo::ApplicationContext::getInstance();

    // 2. 创建条件上下文
    auto context = createConditionContext();

    // 3. 获取匹配的 Starter
    auto matchingStarters = StarterRegistry::getInstance().getMatchingStarters(*context);

    // 4. 过滤出启用的 Starter
    activeStarters_ = filterEnabledStarters(matchingStarters);

    for (const auto& starter : activeStarters_) {
        context->markStarterEnabled(starter->getMetadata().name);
    }

    // 5. 让 Starter 向 ApplicationContext 注册 BeanDefinition/Bean
    for (const auto& starter : activeStarters_) {
        starter->registerBeans(appContext);
    }

    for (const auto& name : appContext.getComponentNames()) {
        context->markBeanPresent(name);
    }

    // 6. 初始化 Starter（兼容旧模式）
    for (const auto& starter : activeStarters_) {
        starter->onInitialize();
    }

#ifdef HAVE_FRUIT
    // 7. 组合所有 Starter 的组件
    // TODO: 实现组件组合逻辑
    // Fruit 的组件组合需要使用 NormalizedComponent 和 install 机制

    // 8. 创建注入器
    // TODO: 正确构造 Fruit Injector
    // injector_ = std::unique_ptr<fruit::Injector<>>(new fruit::Injector<>(getCombinedComponent));
#endif
}

void ApolloApplication::loadConfiguration() {
    std::unordered_map<std::string, std::string> fileProps;
    const auto environmentProps = readEnvironmentOverrides();
    auto& appContext = Apollo::ApplicationContext::getInstance();
    auto configEnvironment = appContext.getConfigEnvironment();
    if (!configEnvironment) {
        configEnvironment = std::make_shared<Apollo::ConfigEnvironment>();
        appContext.setConfigEnvironment(configEnvironment);
    }

    configEnvironment->clearSource("starter.file");
    configEnvironment->clearSource("starter.env");
    configEnvironment->clearSource("starter.builder");

    if (!configPath_.empty()) {
        std::ifstream file(configPath_, std::ios::binary);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();

            if (endsWith(configPath_, ".json")) {
#ifdef HAVE_NLOHMANN_JSON
                fileProps = parseJsonProperties(content);
#else
                (void)content;
#endif
            } else {
                fileProps = parseIniLikeProperties(content);
            }
        }

        apollo::core::config::ConfigManager::instance().loadFile(
            configPath_, apollo::core::config::ConfigFormat::Auto, "starter");
    }

    for (const auto& [key, value] : fileProps) {
        configEnvironment->set(
            "starter.file", Apollo::ConfigSourcePriority::BaseConfig, key, value);
    }

    for (const auto& [key, value] : environmentProps) {
        configEnvironment->set(
            "starter.env", Apollo::ConfigSourcePriority::EnvironmentVariables, key, value);
    }

    for (const auto& [key, value] : properties_) {
        configEnvironment->set(
            "starter.builder", Apollo::ConfigSourcePriority::CommandLine, key, value);
    }

    properties_.clear();
    for (const auto& key : configEnvironment->getAllKeys()) {
        properties_[key] = configEnvironment->getString(key);
    }

    appContext.syncConfigEnvironmentToManager();
}

std::unique_ptr<ConditionContext> ApolloApplication::createConditionContext() {
    auto ctx = std::make_unique<DefaultConditionContext>();
    auto& appContext = Apollo::ApplicationContext::getInstance();

    // 设置配置属性
    for (const auto& pair : properties_) {
        ctx->setProperty(pair.first, pair.second);
    }

    auto configEnvironment = appContext.getConfigEnvironment();
    if (configEnvironment) {
        for (const auto& key : ConfigManager::getInstance()->getAllKeys()) {
            ctx->setProperty(key, configEnvironment->getString(key));
        }
    }

    for (const auto& beanName : appContext.getComponentNames()) {
        ctx->markBeanPresent(beanName);
    }

    for (const auto& starterName : enabledStarters_) {
        ctx->markStarterEnabled(starterName);
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
