#pragma once

#include <string>
#include <vector>
#include <functional>
#include <type_traits>
#include <optional>

namespace apollo {
namespace serialization {

//==============================================================================
// 格式枚举
//==============================================================================

enum class Format {
    Auto,       // 自动检测（根据文件扩展名）
    Json,       // JSON 格式
    Yaml,       // YAML 格式
    Ini,        // INI 格式
    Csv         // CSV 格式
};

//==============================================================================
// 序列化异常
//==============================================================================

class SerializationException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;

    SerializationException(const std::string& msg, size_t line, size_t column)
        : std::runtime_error(msg + " at line " + std::to_string(line) + ", column " + std::to_string(column)),
          line_(line), column_(column) {}

    size_t line() const { return line_; }
    size_t column() const { return column_; }

private:
    size_t line_ = 0;
    size_t column_ = 0;
};

//==============================================================================
// 字段描述符（用于反射）
//==============================================================================

struct FieldDescriptor {
    const char* name;           // 字段名
    size_t offset;              // 在结构体中的偏移量
    const char* description;    // 描述
    bool required;              // 是否必需
    bool primaryKey;            // 是否为主键（用于 CSV 表格）

    // 类型信息
    enum class Type {
        Bool,
        Int8, Int16, Int32, Int64,
        UInt8, UInt16, UInt32, UInt64,
        Float, Double,
        String,
        Array
    };
    Type type;
};

//==============================================================================
// 序列化选项
//==============================================================================

struct SerializeOptions {
    bool pretty = true;         // 美化输出（JSON/YAML）
    bool sortKeys = false;      // 排序键名
    int indent = 2;             // 缩进空格数
    bool includeComments = true; // 包含注释（如果支持）

    // CSV 特定选项
    char csvDelimiter = ',';    // CSV 分隔符
    bool csvHasHeader = true;   // CSV 是否有标题行
    char csvQuote = '"';        // CSV 引号字符
};

//==============================================================================
// 统一序列化器接口
//==============================================================================

template<typename T>
class Serializer {
public:
    //==========================================================================
    // 序列化
    //==========================================================================

    /// 序列化为字符串
    static std::string serialize(const T& value, Format format, const SerializeOptions& options = {});

    /// 序列化到文件
    static bool serializeToFile(const T& value, const std::string& filePath,
                                const SerializeOptions& options = {});

    //==========================================================================
    // 反序列化
    //==========================================================================

    /// 从字符串反序列化
    static T deserialize(const std::string& content, Format format);

    /// 从文件反序列化
    static T deserializeFromFile(const std::string& filePath);

    /// 从字符串反序列化（带默认值）
    static T deserializeOr(const std::string& content, Format format, const T& defaultValue);

    //==========================================================================
    // 格式特定便捷函数
    //==========================================================================

    // JSON
    static std::string toJson(const T& value, bool pretty = true);
    static T fromJson(const std::string& content);
    static T fromJsonFile(const std::string& filePath);

    // YAML
    static std::string toYaml(const T& value);
    static T fromYaml(const std::string& content);
    static T fromYamlFile(const std::string& filePath);

    // INI
    static std::string toIni(const T& value);
    static T fromIni(const std::string& content);
    static T fromIniFile(const std::string& filePath);

    // CSV（仅支持容器类型）
    static std::string toCsv(const std::vector<T>& values, bool hasHeader = true);
    static std::vector<T> fromCsv(const std::string& content);
    static std::vector<T> fromCsvFile(const std::string& filePath);
};

//==============================================================================
// 全局便捷函数
//==============================================================================

namespace detail {

template<typename T>
struct IsContainer : std::false_type {};

template<typename T, typename A>
struct IsContainer<std::vector<T, A>> : std::true_type {
    using value_type = T;
};

} // namespace detail

// 序列化为 JSON
template<typename T>
std::string toJson(const T& value, bool pretty = true) {
    return Serializer<T>::toJson(value, pretty);
}

// 从 JSON 解析
template<typename T>
T fromJson(const std::string& content) {
    return Serializer<T>::fromJson(content);
}

// 序列化为 YAML
template<typename T>
std::string toYaml(const T& value) {
    return Serializer<T>::toYaml(value);
}

// 从 YAML 解析
template<typename T>
T fromYaml(const std::string& content) {
    return Serializer<T>::fromYaml(content);
}

// 序列化为 INI
template<typename T>
std::string toIni(const T& value) {
    return Serializer<T>::toIni(value);
}

// 从 INI 解析
template<typename T>
T fromIni(const std::string& content) {
    return Serializer<T>::fromIni(content);
}

// 序列化为 CSV（容器类型）
template<typename T>
std::string toCsv(const T& container, bool hasHeader = true) {
    return Serializer<typename T::value_type>::toCsv(container, hasHeader);
}

// 从 CSV 解析为容器
template<typename T>
std::vector<T> fromCsv(const std::string& content) {
    return Serializer<T>::fromCsv(content);
}

// 从文件加载（自动检测格式）
template<typename T>
T loadFile(const std::string& filePath) {
    return Serializer<T>::deserializeFromFile(filePath);
}

// 保存到文件（自动检测格式）
template<typename T>
bool saveFile(const T& value, const std::string& filePath, bool pretty = true) {
    return Serializer<T>::serializeToFile(value, filePath, {pretty});
}

//==============================================================================
// 格式检测
//==============================================================================

inline Format detectFormat(const std::string& filePath) {
    size_t dotPos = filePath.rfind('.');
    if (dotPos == std::string::npos) {
        return Format::Json; // 默认 JSON
    }

    std::string ext = filePath.substr(dotPos + 1);
    // 转小写
    for (char& c : ext) {
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
    }

    if (ext == "json") return Format::Json;
    if (ext == "yaml" || ext == "yml") return Format::Yaml;
    if (ext == "ini" || ext == "cfg" || ext == "conf") return Format::Ini;
    if (ext == "csv") return Format::Csv;

    return Format::Json; // 默认
}

//==============================================================================
// 字符串工具
//==============================================================================

namespace string_utils {

// 去除首尾空白
inline std::string trim(const std::string& str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }

    if (start == str.size()) {
        return "";
    }

    size_t end = str.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(str[end]))) {
        --end;
    }

    return str.substr(start, end - start + 1);
}

// 分割字符串
inline std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    size_t start = 0;
    size_t pos = str.find(delimiter);

    while (pos != std::string::npos) {
        result.push_back(str.substr(start, pos - start));
        start = pos + 1;
        pos = str.find(delimiter, start);
    }

    result.push_back(str.substr(start));
    return result;
}

// 转小写
inline std::string toLower(const std::string& str) {
    std::string result = str;
    for (char& c : result) {
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
    }
    return result;
}

// 转大写
inline std::string toUpper(const std::string& str) {
    std::string result = str;
    for (char& c : result) {
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
    }
    return result;
}

// 检查是否以 prefix 开头
inline bool startsWith(const std::string& str, const std::string& prefix) {
    if (prefix.size() > str.size()) {
        return false;
    }
    return str.compare(0, prefix.size(), prefix) == 0;
}

// 检查是否以 suffix 结尾
inline bool endsWith(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) {
        return false;
    }
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace string_utils

} // namespace serialization
} // namespace apollo
