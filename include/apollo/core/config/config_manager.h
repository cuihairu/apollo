#pragma once

#include "apollo/core/config/config_value.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <shared_mutex>
#include <unordered_map>

namespace apollo {
namespace core {
namespace config {

/**
 * @brief 配置格式类型
 */
enum class ConfigFormat {
    Auto,       // 自动检测
    Ini,        // INI格式
    Json,       // JSON格式
    Xml,        // XML格式
    Lua         // Lua格式（仅table）
};

/**
 * @brief 配置变更监听器
 */
using ConfigChangeListener = std::function<void(const std::string& key, const ConfigNode& newValue)>;

/**
 * @brief 配置管理器
 *
 * 功能：
 * - 加载多种格式配置文件（INI, JSON, XML, Lua）
 * - 层级化配置访问
 * - 配置热重载
 * - 配置变更监听
 * - 线程安全
 */
class ConfigManager {
public:
    /**
     * @brief 获取单例实例
     */
    static ConfigManager& instance();

    // 禁止拷贝和移动
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    ConfigManager(ConfigManager&&) = delete;
    ConfigManager& operator=(ConfigManager&&) = delete;

    /**
     * @brief 加载配置文件
     *
     * @param filePath 文件路径
     * @param format 配置格式（Auto表示自动检测）
     * @param section 配置节名称（用于区分不同配置）
     * @return 成功返回true
     */
    bool loadFile(const std::string& filePath,
                  ConfigFormat format = ConfigFormat::Auto,
                  const std::string& section = "default");

    /**
     * @brief 加载配置字符串
     *
     * @param content 配置内容
     * @param format 配置格式
     * @param section 配置节名称
     * @return 成功返回true
     */
    bool loadString(const std::string& content,
                    ConfigFormat format,
                    const std::string& section = "default");

    /**
     * @brief 重新加载配置文件
     *
     * @param section 配置节名称（空表示重新加载所有）
     * @return 成功返回true
     */
    bool reload(const std::string& section = "");

    /**
     * @brief 保存配置到文件
     *
     * @param filePath 文件路径
     * @param format 保存格式
     * @param section 配置节名称
     * @return 成功返回true
     */
    bool saveFile(const std::string& filePath,
                  ConfigFormat format = ConfigFormat::Ini,
                  const std::string& section = "default");

    /**
     * @brief 检查配置是否存在
     */
    bool has(const std::string& key, const std::string& section = "default") const;

    /**
     * @brief 获取配置节点
     */
    const ConfigNode* get(const std::string& key,
                           const std::string& section = "default") const;

    /**
     * @brief 获取配置值（简化接口）
     */
    template<typename T>
    T getValue(const std::string& key,
               const T& defaultValue = T{},
               const std::string& section = "default") const;

    std::string getString(const std::string& key,
                          const std::string& defaultValue = "",
                          const std::string& section = "default") const;

    int getInt(const std::string& key,
               int defaultValue = 0,
               const std::string& section = "default") const;

    int64_t getInt64(const std::string& key,
                     int64_t defaultValue = 0,
                     const std::string& section = "default") const;

    double getDouble(const std::string& key,
                     double defaultValue = 0.0,
                     const std::string& section = "default") const;

    bool getBool(const std::string& key,
                 bool defaultValue = false,
                 const std::string& section = "default") const;

    std::vector<std::string> getArray(const std::string& key,
                                      const std::string& section = "default") const;

    /**
     * @brief 设置配置值
     */
    void setValue(const std::string& key, const std::string& value,
                  const std::string& section = "default");

    void setValue(const std::string& key, int64_t value,
                  const std::string& section = "default");

    void setValue(const std::string& key, double value,
                  const std::string& section = "default");

    void setValue(const std::string& key, bool value,
                  const std::string& section = "default");

    /**
     * @brief 注册配置变更监听器
     *
     * @param key 监听的配置键（空表示监听所有）
     * @param listener 监听器回调
     * @return 监听器ID
     */
    size_t addListener(const std::string& key, ConfigChangeListener listener);

    /**
     * @brief 移除监听器
     */
    void removeListener(size_t listenerId);

    /**
     * @brief 启用/禁用配置热重载
     */
    void enableHotReload(bool enable, uint64_t checkIntervalMs = 1000);

    /**
     * @brief 更新（需定期调用以检查文件变化）
     */
    void update();

    /**
     * @brief 获取所有配置节名称
     */
    std::vector<std::string> getSections() const;

    /**
     * @brief 清空所有配置
     */
    void clear();

    /**
     * @brief 转储配置（调试用）
     */
    std::string dump(const std::string& section = "") const;

private:
    ConfigManager() = default;
    ~ConfigManager() = default;

    // 配置节：名称 -> 文件路径 -> 根节点
    struct ConfigSection {
        std::string filePath;
        ConfigFormat format;
        ConfigNode root;
        uint64_t lastModified = 0;
        std::string lastHash;
    };

    using ConfigMap = std::unordered_map<std::string, ConfigSection>;
    ConfigMap configs_;
    mutable std::shared_mutex mutex_;

    // 监听器
    struct ConfigListener {
        std::string key;  // 空表示监听所有
        ConfigChangeListener callback;
    };
    std::vector<ConfigListener> listeners_;
    std::mutex listenerMutex_;
    size_t nextListenerId_ = 1;

    // 热重载
    bool hotReloadEnabled_ = false;
    uint64_t hotReloadIntervalMs_ = 1000;
    uint64_t lastCheckTime_ = 0;

    // 解析器
    bool parseIni(const std::string& content, ConfigNode& root);
    bool parseJson(const std::string& content, ConfigNode& root);
    bool parseXml(const std::string& content, ConfigNode& root);
    bool parseLua(const std::string& content, ConfigNode& root);

    // 序列化
    std::string serializeIni(const ConfigNode& root) const;
    std::string serializeJson(const ConfigNode& root) const;

    // 辅助函数
    ConfigFormat detectFormat(const std::string& filePath) const;
    std::string calculateHash(const std::string& content) const;
    uint64_t getFileModifiedTime(const std::string& filePath) const;
    bool readFile(const std::string& filePath, std::string& content) const;
    bool writeFile(const std::string& filePath, const std::string& content) const;

    void notifyListeners(const std::string& key, const ConfigNode& node);
};

} // namespace config
} // namespace core
} // namespace apollo

//==============================================================================
// 便捷宏
//==============================================================================

#define APOLLO_CONFIG_INT(key, def) \
    apollo::core::config::ConfigManager::instance().getInt(key, def)

#define APOLLO_CONFIG_STR(key, def) \
    apollo::core::config::ConfigManager::instance().getString(key, def)

#define APOLLO_CONFIG_BOOL(key, def) \
    apollo::core::config::ConfigManager::instance().getBool(key, def)

#define APOLLO_CONFIG_DOUBLE(key, def) \
    apollo::core::config::ConfigManager::instance().getDouble(key, def)
