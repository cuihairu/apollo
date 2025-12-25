#pragma once

#include "apollo/serialization/serializer.h"
#include "apollo/serialization/reflect.h"

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
// Serializer 特化
//==============================================================================

template<typename T>
class Serializer<T> {
public:
    static std::string serialize(const T& value, Format format, const SerializeOptions& options = {}) {
        if (format == Format::Json) {
            return JsonFormatter::serialize(value, options);
        }
        throw SerializationException("JSON formatter only supports JSON format");
    }

    static bool serializeToFile(const T& value, const std::string& filePath,
                                const SerializeOptions& options = {}) {
        std::string content = serialize(value, Format::Json, options);
        // TODO: 写入文件
        return true;
    }

    static T deserialize(const std::string& content, Format format) {
        if (format == Format::Json || format == Format::Auto) {
            return JsonFormatter::deserialize<T>(content);
        }
        throw SerializationException("Unsupported format");
    }

    static T deserializeFromFile(const std::string& filePath) {
        // TODO: 从文件读取
        return T{};
    }

    // JSON 便捷函数
    static std::string toJson(const T& value, bool pretty = true) {
        return JsonFormatter::serialize(value, {pretty, false, pretty ? 2 : 0});
    }

    static T fromJson(const std::string& content) {
        return JsonFormatter::deserialize<T>(content);
    }

    static T fromJsonFile(const std::string& filePath) {
        return deserializeFromFile(filePath);
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
