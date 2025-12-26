#pragma once

#include "apollo/serialization/serializer.h"
#include "apollo/serialization/reflect.h"
#include <fstream>
#include <sstream>
#include <iostream>

// 前向声明其他 formatter
namespace apollo {
namespace serialization {
namespace formatters {
class YamlFormatter;
class IniFormatter;
class CsvFormatter;
}
}
}

#ifdef HAVE_NLOHMANN_JSON
    #include <nlohmann/json.hpp>
#else
    // 如果没有 nlohmann/json，使用简化实现或报错
    #ifndef APOLLO_JSON_MINIMAL
        #define APOLLO_JSON_MINIMAL
    #endif
    #include <nlohmann/json.hpp>
#endif

namespace apollo {
namespace serialization {
namespace formatters {

//==============================================================================
// JSON Formatter
//==============================================================================

class JsonFormatter {
public:
    //==========================================================================
    // 序列化
    //==========================================================================

    /// 将任意类型序列化为 JSON
    template<typename T>
    static std::string serialize(const T& value, const SerializeOptions& options = {});

    /// 将具有反射信息的结构体序列化为 JSON
    template<typename T>
    static nlohmann::json toJson(const T& value, const SerializeOptions& options = {});

    //==========================================================================
    // 反序列化
    //==========================================================================

    /// 从 JSON 字符串反序列化
    template<typename T>
    static T deserialize(const std::string& content);

    /// 从 nlohmann::json 反序列化到结构体
    template<typename T>
    static void fromJson(const nlohmann::json& j, T& value);

    //==========================================================================
    // 格式化选项
    //==========================================================================

    static std::string dump(const nlohmann::json& j, int indent = 2);

private:
    template<typename T>
    static nlohmann::json valueToJson(const T& value);

    // 特化：支持反射的结构体
    template<typename T>
    static std::enable_if_t<reflect::hasReflection<T>, nlohmann::json>
    structToJson(const T& value);

    // 特化：STL 容器
    template<typename T>
    static nlohmann::json containerToJson(const T& container);

    // 从 JSON 设置字段值
    template<typename T>
    static void setFieldValue(T& obj, const reflect::reflect::detail::auto& field,
                              const nlohmann::json& j);
};

//==============================================================================
// 实现
//==============================================================================

template<typename T>
inline std::string JsonFormatter::serialize(const T& value, const SerializeOptions& options) {
    nlohmann::json j = toJson(value, options);
    return dump(j, options.indent);
}

template<typename T>
inline nlohmann::json JsonFormatter::toJson(const T& value, const SerializeOptions& options) {
    if constexpr (reflect::hasReflection<T>) {
        return structToJson(value);
    } else {
        return nlohmann::json(value);
    }
}

template<typename T>
inline T JsonFormatter::deserialize(const std::string& content) {
    nlohmann::json j = nlohmann::json::parse(content);
    T value{};
    fromJson(j, value);
    return value;
}

template<typename T>
inline void JsonFormatter::fromJson(const nlohmann::json& j, T& value) {
    if constexpr (reflect::hasReflection<T>) {
        reflect::forEachField(value, [&](auto& field, auto& obj) {
            if (j.contains(field.name)) {
                using FieldType = typename decltype(field)::FieldType;
                if constexpr (std::is_same_v<FieldType, std::string>) {
                    field.set(obj, j[field.name].get<std::string>());
                } else if constexpr (std::is_same_v<FieldType, int>) {
                    field.set(obj, j[field.name].get<int>());
                } else if constexpr (std::is_same_v<FieldType, int32_t>) {
                    field.set(obj, j[field.name].get<int32_t>());
                } else if constexpr (std::is_same_v<FieldType, uint32_t>) {
                    field.set(obj, j[field.name].get<uint32_t>());
                } else if constexpr (std::is_same_v<FieldType, int64_t>) {
                    field.set(obj, j[field.name].get<int64_t>());
                } else if constexpr (std::is_same_v<FieldType, uint64_t>) {
                    field.set(obj, j[field.name].get<uint64_t>());
                } else if constexpr (std::is_same_v<FieldType, float>) {
                    field.set(obj, j[field.name].get<float>());
                } else if constexpr (std::is_same_v<FieldType, double>) {
                    field.set(obj, j[field.name].get<double>());
                } else if constexpr (std::is_same_v<FieldType, bool>) {
                    field.set(obj, j[field.name].get<bool>());
                }
            }
        });
    } else {
        // 直接赋值
        value = j.get<T>();
    }
}

inline std::string JsonFormatter::dump(const nlohmann::json& j, int indent) {
    return j.dump(indent);
}

template<typename T>
inline std::enable_if_t<reflect::hasReflection<T>, nlohmann::json>
JsonFormatter::structToJson(const T& value) {
    nlohmann::json j;
    reflect::forEachField(value, [&](const auto& field, const auto& obj) {
        using FieldType = typename decltype(field)::FieldType;
        const auto& fieldValue = field.get(obj);

        if constexpr (std::is_same_v<FieldType, std::string>) {
            j[field.name] = fieldValue;
        } else if constexpr (std::is_integral_v<FieldType> && !std::is_same_v<FieldType, bool>) {
            j[field.name] = fieldValue;
        } else if constexpr (std::is_same_v<FieldType, bool>) {
            j[field.name] = fieldValue;
        } else if constexpr (std::is_floating_point_v<FieldType>) {
            j[field.name] = fieldValue;
        } else if constexpr (reflect::hasReflection<FieldType>) {
            j[field.name] = structToJson(fieldValue);
        } else {
            j[field.name] = nlohmann::json(fieldValue);
        }
    });
    return j;
}

//==============================================================================
// Serializer 特化（统一入口）
//==============================================================================

template<typename T>
class Serializer {
public:
    static std::string serialize(const T& value, Format format, const SerializeOptions& options = {}) {
        switch (format) {
            case Format::Json:
                return JsonFormatter::serialize(value, options);
            case Format::Yaml:
                return formatters::YamlFormatter::serialize(value, options);
            case Format::Ini:
                return formatters::IniFormatter::serialize(value, {});
            default:
                throw SerializationException("Unsupported format for serialize");
        }
    }

    static bool serializeToFile(const T& value, const std::string& filePath,
                                const SerializeOptions& options = {}) {
        Format format = detectFormat(filePath);
        std::string content = serialize(value, format, options);
        std::ofstream file(filePath);
        if (!file.is_open()) {
            return false;
        }
        file << content;
        return file.good();
    }

    static T deserialize(const std::string& content, Format format) {
        switch (format) {
            case Format::Json:
            case Format::Auto:
                return JsonFormatter::deserialize<T>(content);
            case Format::Yaml:
                return formatters::YamlFormatter::deserialize<T>(content);
            case Format::Ini:
                return formatters::IniFormatter::deserialize<T>(content, {});
            default:
                throw SerializationException("Unsupported format for deserialize");
        }
    }

    static T deserializeFromFile(const std::string& filePath) {
        Format format = detectFormat(filePath);
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw SerializationException("Cannot open file: " + filePath);
        }
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        return deserialize(content, format);
    }

    // JSON 便捷函数
    static std::string toJson(const T& value, bool pretty = true) {
        return JsonFormatter::serialize(value, {pretty, false, pretty ? 2 : 0});
    }

    static T fromJson(const std::string& content) {
        return JsonFormatter::deserialize<T>(content);
    }

    static T fromJsonFile(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw SerializationException("Cannot open file: " + filePath);
        }
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        return fromJson(content);
    }

    // YAML 便捷函数
    static std::string toYaml(const T& value) {
        return formatters::YamlFormatter::serialize(value, {});
    }

    static T fromYaml(const std::string& content) {
        return formatters::YamlFormatter::deserialize<T>(content);
    }

    static T fromYamlFile(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw SerializationException("Cannot open file: " + filePath);
        }
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        return fromYaml(content);
    }

    // INI 便捷函数
    static std::string toIni(const T& value) {
        return formatters::IniFormatter::serialize(value, {});
    }

    static T fromIni(const std::string& content) {
        return formatters::IniFormatter::deserialize<T>(content, {});
    }

    static T fromIniFile(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw SerializationException("Cannot open file: " + filePath);
        }
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        return fromIni(content);
    }

    // CSV 便捷函数（仅容器类型）
    static std::string toCsv(const std::vector<T>& values, bool hasHeader = true) {
        return formatters::CsvFormatter::serialize(values, {',', '"', hasHeader});
    }

    static std::vector<T> fromCsv(const std::string& content) {
        return formatters::CsvFormatter::deserialize<T>(content, {});
    }

    static std::vector<T> fromCsvFile(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw SerializationException("Cannot open file: " + filePath);
        }
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        return fromCsv(content);
    }
};

} // namespace formatters

//==============================================================================
// 全局便捷函数实现（JSON）
//==============================================================================

template<typename T>
inline std::string toJson(const T& value, bool pretty) {
    return formatters::JsonFormatter::serialize(value, {pretty, false, pretty ? 2 : 0});
}

template<typename T>
inline T fromJson(const std::string& content) {
    return formatters::JsonFormatter::deserialize<T>(content);
}

} // namespace serialization
} // namespace apollo
