/**
 * @file config_manager.cpp
 * @brief 配置管理器实现
 */

#include "apollo/core/config/config_manager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <filesystem>

#ifdef HAVE_NLOHMANN_JSON
    #include <nlohmann/json.hpp>
#endif

namespace fs = std::filesystem;

namespace apollo {
namespace core {
namespace config {

//==============================================================================
// ConfigManager 实现
//==============================================================================

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadFile(const std::string& filePath,
                             ConfigFormat format,
                             const std::string& section) {
    std::string content;
    if (!readFile(filePath, content)) {
        return false;
    }

    return loadString(content, format, section);
}

bool ConfigManager::loadString(const std::string& content,
                               ConfigFormat format,
                               const std::string& section) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    ConfigSection cs;
    cs.root = ConfigNode{};

    // 解析内容
    bool success = false;
    switch (format) {
        case ConfigFormat::Ini:
            success = parseIni(content, cs.root);
            break;
        case ConfigFormat::Json:
            success = parseJson(content, cs.root);
            break;
        case ConfigFormat::Xml:
            success = parseXml(content, cs.root);
            break;
        case ConfigFormat::Lua:
            success = parseLua(content, cs.root);
            break;
        case ConfigFormat::Auto: {
            // 尝试检测格式
            std::string trimmed = content;
            trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
            if (trimmed.empty()) break;

            if (trimmed[0] == '{') {
                success = parseJson(content, cs.root);
            } else if (trimmed[0] == '<') {
                success = parseXml(content, cs.root);
            } else if (trimmed.find('=') != std::string::npos ||
                       trimmed.find('[') != std::string::npos) {
                success = parseIni(content, cs.root);
            }
            break;
        }
    }

    if (!success) {
        return false;
    }

    cs.format = format;
    cs.lastHash = calculateHash(content);
    configs_[section] = std::move(cs);

    return true;
}

bool ConfigManager::reload(const std::string& section) {
    if (section.empty()) {
        // 重新加载所有
        bool allSuccess = true;
        std::vector<std::string> sections;
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            for (const auto& pair : configs_) {
                sections.push_back(pair.first);
            }
        }
        for (const auto& sec : sections) {
            if (!reload(sec)) {
                allSuccess = false;
            }
        }
        return allSuccess;
    }

    std::string filePath;
    ConfigFormat format;
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = configs_.find(section);
        if (it == configs_.end()) {
            return false;
        }
        filePath = it->second.filePath;
        format = it->second.format;
    }

    std::string content;
    if (!readFile(filePath, content)) {
        return false;
    }

    std::string newHash = calculateHash(content);

    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = configs_.find(section);
        if (it != configs_.end() && it->second.lastHash == newHash) {
            return true;  // 内容未变化
        }
    }

    return loadFile(filePath, format, section);
}

bool ConfigManager::saveFile(const std::string& filePath,
                             ConfigFormat format,
                             const std::string& section) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = configs_.find(section);
    if (it == configs_.end()) {
        return false;
    }

    std::string content;
    switch (format) {
        case ConfigFormat::Ini:
            content = serializeIni(it->second.root);
            break;
        case ConfigFormat::Json:
            content = serializeJson(it->second.root);
            break;
        default:
            return false;
    }

    return writeFile(filePath, content);
}

bool ConfigManager::has(const std::string& key,
                        const std::string& section) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = configs_.find(section);
    if (it == configs_.end()) {
        return false;
    }

    return it->second.root.getByPath(key) != nullptr;
}

const ConfigNode* ConfigManager::get(const std::string& key,
                                      const std::string& section) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = configs_.find(section);
    if (it == configs_.end()) {
        return nullptr;
    }

    return it->second.root.getByPath(key);
}

std::string ConfigManager::getString(const std::string& key,
                                     const std::string& defaultValue,
                                     const std::string& section) const {
    auto node = get(key, section);
    return node ? node->asString(defaultValue) : defaultValue;
}

int ConfigManager::getInt(const std::string& key,
                          int defaultValue,
                          const std::string& section) const {
    auto node = get(key, section);
    return node ? node->asInt(defaultValue) : defaultValue;
}

int64_t ConfigManager::getInt64(const std::string& key,
                                int64_t defaultValue,
                                const std::string& section) const {
    auto node = get(key, section);
    return node ? node->asInt64(defaultValue) : defaultValue;
}

double ConfigManager::getDouble(const std::string& key,
                                double defaultValue,
                                const std::string& section) const {
    auto node = get(key, section);
    return node ? node->asDouble(defaultValue) : defaultValue;
}

bool ConfigManager::getBool(const std::string& key,
                            bool defaultValue,
                            const std::string& section) const {
    auto node = get(key, section);
    return node ? node->asBool(defaultValue) : defaultValue;
}

std::vector<std::string> ConfigManager::getArray(const std::string& key,
                                                 const std::string& section) const {
    auto node = get(key, section);
    return node ? node->asArray() : std::vector<std::string>{};
}

void ConfigManager::setValue(const std::string& key, const std::string& value,
                             const std::string& section) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto& sectionData = configs_[section];
    auto* node = sectionData.root.getByPath(key);
    if (node) {
        node->setString(value);
    }
}

void ConfigManager::setValue(const std::string& key, int64_t value,
                             const std::string& section) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto& sectionData = configs_[section];
    auto* node = sectionData.root.getByPath(key);
    if (node) {
        node->setInt64(value);
    }
}

void ConfigManager::setValue(const std::string& key, double value,
                             const std::string& section) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto& sectionData = configs_[section];
    auto* node = sectionData.root.getByPath(key);
    if (node) {
        node->setDouble(value);
    }
}

void ConfigManager::setValue(const std::string& key, bool value,
                             const std::string& section) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto& sectionData = configs_[section];
    auto* node = sectionData.root.getByPath(key);
    if (node) {
        node->setBool(value);
    }
}

size_t ConfigManager::addListener(const std::string& key,
                                  ConfigChangeListener listener) {
    std::lock_guard<std::mutex> lock(listenerMutex_);

    size_t id = nextListenerId_++;
    listeners_.push_back({key, std::move(listener)});
    return id;
}

void ConfigManager::removeListener(size_t listenerId) {
    std::lock_guard<std::mutex> lock(listenerMutex_);

    if (listenerId > 0 && listenerId <= listeners_.size()) {
        listeners_[listenerId - 1].callback = nullptr;
    }
}

void ConfigManager::enableHotReload(bool enable, uint64_t checkIntervalMs) {
    hotReloadEnabled_ = enable;
    hotReloadIntervalMs_ = checkIntervalMs;
}

void ConfigManager::update() {
    if (!hotReloadEnabled_) {
        return;
    }

    uint64_t now = getFileModifiedTime("");  // 使用当前时间
    if (now - lastCheckTime_ < hotReloadIntervalMs_) {
        return;
    }
    lastCheckTime_ = now;

    // 检查所有配置文件
    std::vector<std::string> sections;
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& pair : configs_) {
            if (!pair.second.filePath.empty()) {
                sections.push_back(pair.first);
            }
        }
    }

    for (const auto& section : sections) {
        reload(section);
    }
}

std::vector<std::string> ConfigManager::getSections() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<std::string> result;
    for (const auto& pair : configs_) {
        result.push_back(pair.first);
    }
    return result;
}

void ConfigManager::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    configs_.clear();
}

std::string ConfigManager::dump(const std::string& section) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::ostringstream oss;
    if (section.empty()) {
        for (const auto& pair : configs_) {
            oss << "=== Section: " << pair.first << " ===\n";
            oss << pair.second.root.toString();
            oss << "\n";
        }
    } else {
        auto it = configs_.find(section);
        if (it != configs_.end()) {
            oss << "=== Section: " << section << " ===\n";
            oss << it->second.root.toString();
        }
    }
    return oss.str();
}

//==============================================================================
// 解析器实现
//==============================================================================

bool ConfigManager::parseIni(const std::string& content, ConfigNode& root) {
    std::istringstream stream(content);
    std::string line;
    ConfigNode* currentSection = &root;

    std::regex sectionRegex(R"(^\s*\[([^\]]+)\]\s*(?:;.*)?$)");
    std::regex keyRegex(R"(^\s*([^=;]+)\s*=\s*([^;]*?)\s*(?:;.*)?$)");
    std::regex arrayRegex(R"(^\s*([^=;]+)\s*=\s*([^;]*?)\s*(?:;.*)?$)");

    while (std::getline(stream, line)) {
        // 移除BOM
        if (!line.empty() && line[0] == '\xEF') {
            line.erase(0, 3);
        }

        // 跳过空行和注释
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        // 检查section
        std::smatch match;
        if (std::regex_match(line, match, sectionRegex)) {
            std::string sectionName = match[1];
            currentSection = &root.getChild(sectionName);
            continue;
        }

        // 检查key=value
        if (std::regex_match(line, match, keyRegex)) {
            std::string key = match[1];
            std::string value = match[2];

            // 去除空白
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // 去除引号
            if (value.size() >= 2 &&
                ((value[0] == '"' && value.back() == '"') ||
                 (value[0] == '\'' && value.back() == '\''))) {
                value = value.substr(1, value.size() - 2);
            }

            // 解析值类型
            if (value == "true" || value == "yes") {
                currentSection->getChild(key).setBool(true);
            } else if (value == "false" || value == "no") {
                currentSection->getChild(key).setBool(false);
            } else if (!value.empty() && std::isdigit(value[0])) {
                try {
                    if (value.find('.') != std::string::npos) {
                        currentSection->getChild(key).setDouble(std::stod(value));
                    } else {
                        currentSection->getChild(key).setInt64(std::stoll(value));
                    }
                } catch (...) {
                    currentSection->getChild(key).setString(value);
                }
            } else {
                currentSection->getChild(key).setString(value);
            }
            continue;
        }
    }

    return true;
}

bool ConfigManager::parseJson(const std::string& content, ConfigNode& root) {
    // 简单JSON解析（生产环境应使用nlohmann/json或其他库）
    // 这里只实现基本功能

    // 尝试使用nlohmann/json
#ifdef HAVE_NLOHMANN_JSON
    try {
        nlohmann::json j = nlohmann::json::parse(content);
        // 递归转换到ConfigNode
        std::function<void(const nlohmann::json&, ConfigNode&)> convert =
            [&](const nlohmann::json& j, ConfigNode& node) {
                if (j.is_boolean()) {
                    node.setBool(j.get<bool>());
                } else if (j.is_number_integer()) {
                    node.setInt64(j.get<int64_t>());
                } else if (j.is_number_float()) {
                    node.setDouble(j.get<double>());
                } else if (j.is_string()) {
                    node.setString(j.get<std::string>());
                } else if (j.is_array()) {
                    std::vector<std::string> arr;
                    for (const auto& item : j) {
                        if (item.is_string()) {
                            arr.push_back(item.get<std::string>());
                        } else {
                            arr.push_back(item.dump());
                        }
                    }
                    node.setArray(arr);
                } else if (j.is_object()) {
                    for (auto it = j.begin(); it != j.end(); ++it) {
                        ConfigNode child;
                        convert(*it, child);
                        node.setChild(it.key(), child);
                    }
                }
            };
        convert(j, root);
        return true;
    } catch (...) {
        return false;
    }
#else
    // 简单的键值对解析 {"key": "value"}
    std::regex kvRegex(R"("\"?([^\"]+)\"?\s*:\s*\"?([^\"]+)\"?)");
    std::string::const_iterator searchStart = content.begin();
    std::smatch match;
    while (std::regex_search(searchStart, content.cend(), match, kvRegex)) {
        if (match.size() >= 3) {
            std::string key = match[1];
            std::string value = match[2];
            root.getChild(key).setString(value);
        }
        searchStart = match.suffix().first;
    }
    return true;
#endif
}

bool ConfigManager::parseXml(const std::string& content, ConfigNode& root) {
    (void)content;
    (void)root;
    // XML解析需要专门的库（如tinyxml2）
    // 这里提供基本实现
    return false;
}

bool ConfigManager::parseLua(const std::string& content, ConfigNode& root) {
    (void)content;
    (void)root;
    // Lua table解析需要Lua解释器
    // 这里提供基本实现
    return false;
}

std::string ConfigManager::serializeIni(const ConfigNode& root) const {
    std::ostringstream oss;

    for (const auto& pair : root.getKeys()) {
        const auto& child = root.getChild(pair);
        if (child.isBool()) {
            oss << pair << "=" << (child.asBool() ? "true" : "false") << "\n";
        } else if (child.isInt64()) {
            oss << pair << "=" << child.asInt64() << "\n";
        } else if (child.isDouble()) {
            oss << pair << "=" << child.asDouble() << "\n";
        } else if (child.isString()) {
            oss << pair << "=" << child.asString() << "\n";
        }
    }

    return oss.str();
}

std::string ConfigManager::serializeJson(const ConfigNode& root) const {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& key : root.getKeys()) {
        if (!first) oss << ",";
        first = false;
        const auto& child = root.getChild(key);
        oss << "\"" << key << "\":";
        if (child.isBool()) {
            oss << (child.asBool() ? "true" : "false");
        } else if (child.isInt64()) {
            oss << child.asInt64();
        } else if (child.isDouble()) {
            oss << child.asDouble();
        } else if (child.isString()) {
            oss << "\"" << child.asString() << "\"";
        }
    }
    oss << "}";
    return oss.str();
}

//==============================================================================
// 辅助函数
//==============================================================================

ConfigFormat ConfigManager::detectFormat(const std::string& filePath) const {
    std::string ext = filePath.substr(filePath.find_last_of('.'));
    if (ext == ".ini" || ext == ".cfg") {
        return ConfigFormat::Ini;
    } else if (ext == ".json") {
        return ConfigFormat::Json;
    } else if (ext == ".xml") {
        return ConfigFormat::Xml;
    } else if (ext == ".lua") {
        return ConfigFormat::Lua;
    }
    return ConfigFormat::Auto;
}

std::string ConfigManager::calculateHash(const std::string& content) const {
    // 简单哈希（生产环境应使用SHA256等）
    size_t hash = std::hash<std::string>{}(content);
    return std::to_string(hash);
}

uint64_t ConfigManager::getFileModifiedTime(const std::string& filePath) const {
    if (filePath.empty()) {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }

    try {
        auto ftime = fs::last_write_time(filePath);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            sctp.time_since_epoch()).count();
    } catch (...) {
        return 0;
    }
}

bool ConfigManager::readFile(const std::string& filePath, std::string& content) const {
    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        content.assign(std::istreambuf_iterator<char>(file),
                       std::istreambuf_iterator<char>());
        return true;
    } catch (...) {
        return false;
    }
}

bool ConfigManager::writeFile(const std::string& filePath, const std::string& content) const {
    try {
        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        file << content;
        return true;
    } catch (...) {
        return false;
    }
}

void ConfigManager::notifyListeners(const std::string& key, const ConfigNode& node) {
    std::lock_guard<std::mutex> lock(listenerMutex_);

    for (const auto& listener : listeners_) {
        if (listener.callback) {
            if (listener.key.empty() || key == listener.key) {
                listener.callback(key, node);
            }
        }
    }
}

} // namespace config
} // namespace core
} // namespace apollo
