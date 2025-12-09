#pragma once

#include <string>
#include <memory>
#include <typeinfo>
#include <functional>

namespace apollo {

/// 配置加载器接口
class IConfigLoader {
public:
    virtual ~IConfigLoader() = default;

    /// 加载配置
    virtual bool Load(const std::string& filePath) = 0;

    /// 重新加载
    virtual bool Reload(const std::string& filePath) = 0;

    /// 校验配置
    virtual bool Validate(const std::string& filePath) = 0;

    /// 获取配置数据
    virtual void* GetData() = 0;

    /// 获取配置类型
    virtual const std::type_info& GetType() = 0;

    /// 获取最后修改时间
    virtual uint64_t GetLastModified() const = 0;
};

/// 模板化配置加载器
template<typename T>
class ConfigLoader : public IConfigLoader {
public:
    using ConfigPtr = std::shared_ptr<T>;
    using ParseFunc = std::function<ConfigPtr(const std::string&)>;
    using ValidateFunc = std::function<bool(const T&)>;

    ConfigLoader(ParseFunc parseFunc, ValidateFunc validateFunc)
        : parseFunc_(parseFunc), validateFunc_(validateFunc) {}

    bool Load(const std::string& filePath) override {
        try {
            // 读取文件
            std::string content;
            if (!ReadFile(filePath, content)) {
                return false;
            }

            // 解析配置
            data_ = parseFunc_(content);
            if (!data_) {
                return false;
            }

            // 计算哈希
            lastHash_ = CalculateHash(content);
            lastModified_ = GetFileModifiedTime(filePath);

            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    bool Reload(const std::string& filePath) override {
        // 检查内容是否变化
        std::string content;
        if (!ReadFile(filePath, content)) {
            return false;
        }

        std::string newHash = CalculateHash(content);
        if (newHash == lastHash_) {
            return true;  // 内容没有变化
        }

        // 创建新配置
        auto newData = parseFunc_(content);
        if (!newData) {
            return false;
        }

        // 原子性替换
        data_ = newData;
        lastHash_ = newHash;
        lastModified_ = GetFileModifiedTime(filePath);

        return true;
    }

    bool Validate(const std::string& filePath) override {
        // 基础校验
        if (!FileExists(filePath)) {
            return false;
        }

        // 加载并校验
        std::string content;
        if (!ReadFile(filePath, content)) {
            return false;
        }

        auto tempData = parseFunc_(content);
        if (!tempData) {
            return false;
        }

        // 执行详细校验
        return validateFunc_(*tempData);
    }

    void* GetData() override {
        return data_.get();
    }

    const std::type_info& GetType() override {
        return typeid(T);
    }

    uint64_t GetLastModified() const override {
        return lastModified_;
    }

    /// 获取配置数据
    ConfigPtr GetConfig() const {
        return data_;
    }

private:
    ConfigPtr data_;
    std::string lastHash_;
    uint64_t lastModified_ = 0;
    ParseFunc parseFunc_;
    ValidateFunc validateFunc_;

    static std::string CalculateHash(const std::string& content);
    static bool ReadFile(const std::string& path, std::string& content);
    static bool FileExists(const std::string& path);
    static uint64_t GetFileModifiedTime(const std::string& path);
};

/// Lua配置加载器
class LuaConfigLoader {
public:
    static bool LoadLuaTable(const std::string& luaFile,
                           const std::string& tableName,
                           std::unordered_map<std::string, std::string>& result);
};

/// JSON配置加载器
class JsonConfigLoader {
public:
    static bool LoadJsonFile(const std::string& jsonFile,
                            std::unordered_map<std::string, std::string>& result);
};

}  // namespace apollo