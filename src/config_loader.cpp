#include "apollo/config_loader.hpp"
#include <fstream>
#include <sstream>
#include <mutex>
#include <filesystem>
#include <cstring>

#ifdef __APPLE__
#include <sys/stat.h>
#endif

namespace apollo {

template<typename T>
std::string ConfigLoader<T>::CalculateHash(const std::string& content) {
    // 简单的哈希计算，实际项目中可以使用MD5或SHA1
    size_t hash = 0;
    for (char c : content) {
        hash = hash * 31 + c;
    }
    return std::to_string(hash);
}

template<typename T>
bool ConfigLoader<T>::ReadFile(const std::string& path, std::string& content) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    content = oss.str();

    return !content.empty();
}

template<typename T>
bool ConfigLoader<T>::FileExists(const std::string& path) {
    return std::filesystem::exists(path);
}

template<typename T>
uint64_t ConfigLoader<T>::GetFileModifiedTime(const std::string& path) {
    try {
        auto ftime = std::filesystem::last_write_time(path);
        return ftime.time_since_epoch().count();
    } catch (...) {
        return 0;
    }
}

// 显式实例化
template class ConfigLoader<std::unordered_map<std::string, std::string>>;
template class ConfigLoader<std::vector<std::unordered_map<std::string, std::string>>>;

bool LuaConfigLoader::LoadLuaTable(const std::string& luaFile,
                                     const std::string& tableName,
                                     std::unordered_map<std::string, std::string>& result) {
    // 简化实现：假设Lua文件是key=value格式的文本
    std::ifstream file(luaFile);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    bool inTargetTable = false;
    std::string currentTable;

    while (std::getline(file, line)) {
        // 简单的解析逻辑
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // 检查表名
        if (line.find(tableName) != std::string::npos) {
            inTargetTable = true;
            continue;
        }

        if (inTargetTable) {
            size_t equalPos = line.find('=');
            if (equalPos != std::string::npos) {
                std::string key = line.substr(0, equalPos);
                std::string value = line.substr(equalPos + 1);

                // 去除空格
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);

                // 去除引号
                if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                    value = value.substr(1, value.size() - 2);
                }

                result[key] = value;
            }
        }
    }

    return true;
}

bool JsonConfigLoader::LoadJsonFile(const std::string& jsonFile,
                                      std::unordered_map<std::string, std::string>& result) {
    // 简化实现：假设JSON文件是扁平的键值对
    std::ifstream file(jsonFile);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // 简单的JSON解析
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);

            // 去除空格和引号
            key.erase(0, key.find_first_not_of(" \t\""));
            key.erase(key.find_last_not_of(" \t\"") + 1);
            value.erase(0, value.find_first_not_of(" \t\""));
            value.erase(value.find_last_not_of(" \t\"") + 1);

            result[key] = value;
        }
    }

    return true;
}

}  // namespace apollo