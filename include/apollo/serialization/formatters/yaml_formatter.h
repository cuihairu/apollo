#pragma once

#include "apollo/serialization/serializer.h"
#include "apollo/serialization/reflect.h"
#include <sstream>

// YAML 支持 - 需要 yaml-cpp
#ifdef APOLLO_HAS_YAML
    #include <yaml-cpp/yaml.h>
    #define APOLLO_YAML_ENABLED
#endif

namespace apollo {
namespace serialization {
namespace formatters {

//==============================================================================
// YAML Formatter
//==============================================================================

class YamlFormatter {
public:
    //==========================================================================
    // 序列化
    //==========================================================================

    /// 将任意类型序列化为 YAML
    template<typename T>
    static std::string serialize(const T& value, const SerializeOptions& options = {});

#ifdef APOLLO_YAML_ENABLED
    /// 将结构体转换为 YAML::Node
    template<typename T>
    static YAML::Node toYamlNode(const T& value);
#endif

    //==========================================================================
    // 反序列化
    //==========================================================================

    /// 从 YAML 字符串解析
    template<typename T>
    static T deserialize(const std::string& content);

#ifdef APOLLO_YAML_ENABLED
    /// 从 YAML::Node 填充结构体
    template<typename T>
    static void fromYamlNode(const YAML::Node& node, T& value);
#endif

private:
#ifndef APOLLO_YAML_ENABLED
    // 简化版 YAML 生成（不依赖 yaml-cpp）
    template<typename T>
    static std::string simpleSerialize(const T& value, int indent = 0);
#endif
};

//==============================================================================
// 实现（有 yaml-cpp）
//==============================================================================

#ifdef APOLLO_YAML_ENABLED

template<typename T>
inline std::string YamlFormatter::serialize(const T& value, const SerializeOptions& options) {
    YAML::Node node = toYamlNode(value);
    YAML::Emitter emitter;
    emitter.SetIndent(options.indent);
    emitter << node;
    return emitter.c_str();
}

template<typename T>
inline YAML::Node YamlFormatter::toYamlNode(const T& value) {
    YAML::Node node;

    if constexpr (reflect::hasReflection<T>) {
        reflect::forEachField(value, [&](const auto& field, const auto& obj) {
            using FieldType = typename decltype(field)::FieldType;
            const auto& fieldValue = field.get(obj);

            if constexpr (std::is_same_v<FieldType, std::string>) {
                node[field.name] = fieldValue;
            } else if constexpr (std::is_integral_v<FieldType> && !std::is_same_v<FieldType, bool>) {
                node[field.name] = fieldValue;
            } else if constexpr (std::is_same_v<FieldType, bool>) {
                node[field.name] = fieldValue;
            } else if constexpr (std::is_floating_point_v<FieldType>) {
                node[field.name] = fieldValue;
            } else if constexpr (reflect::hasReflection<FieldType>) {
                // 嵌套结构体
                node[field.name] = toYamlNode(fieldValue);
            } else {
                // 其他类型尝试转换
                std::ostringstream ss;
                ss << fieldValue;
                node[field.name] = ss.str();
            }
        });
    } else {
        // 没有反射信息，尝试直接转换
        node = value;
    }

    return node;
}

template<typename T>
inline T YamlFormatter::deserialize(const std::string& content) {
    YAML::Node node = YAML::Load(content);
    T value{};
    fromYamlNode(node, value);
    return value;
}

template<typename T>
inline void YamlFormatter::fromYamlNode(const YAML::Node& node, T& value) {
    if constexpr (reflect::hasReflection<T>) {
        reflect::forEachField(value, [&](auto& field, auto& obj) {
            if (node[field.name]) {
                using FieldType = typename decltype(field)::FieldType;
                const YAML::Node& fieldNode = node[field.name];

                try {
                    if constexpr (std::is_same_v<FieldType, std::string>) {
                        field.set(obj, fieldNode.as<std::string>());
                    } else if constexpr (std::is_same_v<FieldType, int>) {
                        field.set(obj, fieldNode.as<int>());
                    } else if constexpr (std::is_same_v<FieldType, int32_t>) {
                        field.set(obj, fieldNode.as<int32_t>());
                    } else if constexpr (std::is_same_v<FieldType, uint32_t>) {
                        field.set(obj, fieldNode.as<uint32_t>());
                    } else if constexpr (std::is_same_v<FieldType, int64_t>) {
                        field.set(obj, fieldNode.as<int64_t>());
                    } else if constexpr (std::is_same_v<FieldType, uint64_t>) {
                        field.set(obj, fieldNode.as<uint64_t>());
                    } else if constexpr (std::is_same_v<FieldType, float>) {
                        field.set(obj, fieldNode.as<float>());
                    } else if constexpr (std::is_same_v<FieldType, double>) {
                        field.set(obj, fieldNode.as<double>());
                    } else if constexpr (std::is_same_v<FieldType, bool>) {
                        field.set(obj, fieldNode.as<bool>());
                    } else if constexpr (reflect::hasReflection<FieldType>) {
                        // 嵌套结构体
                        FieldType nested;
                        fromYamlNode(fieldNode, nested);
                        field.set(obj, nested);
                    }
                } catch (const YAML::Exception& e) {
                    // 转换失败，保持默认值
                }
            }
        });
    }
}

#else // !APOLLO_YAML_ENABLED

//==============================================================================
// 简化版 YAML 实现（无外部依赖）
//==============================================================================

template<typename T>
inline std::string YamlFormatter::serialize(const T& value, const SerializeOptions& options) {
    return simpleSerialize(value, options.indent);
}

template<typename T>
inline std::string YamlFormatter::simpleSerialize(const T& value, int indent) {
    std::ostringstream oss;
    std::string ind(indent, ' ');

    if constexpr (reflect::hasReflection<T>) {
        oss << reflect::getTypeName<T>() << ":\n";
        reflect::forEachField(value, [&](const auto& field, const auto& obj) {
            oss << ind << field.name << ": ";

            using FieldType = typename decltype(field)::FieldType;
            const auto& fieldValue = field.get(obj);

            if constexpr (std::is_same_v<FieldType, std::string>) {
                oss << "\"" << fieldValue << "\"\n";
            } else if constexpr (std::is_same_v<FieldType, bool>) {
                oss << (fieldValue ? "true" : "false") << "\n";
            } else if constexpr (std::is_floating_point_v<FieldType>) {
                oss << fieldValue << "\n";
            } else if constexpr (std::is_integral_v<FieldType>) {
                oss << fieldValue << "\n";
            } else if constexpr (reflect::hasReflection<FieldType>) {
                oss << "\n";
                oss << simpleSerialize(fieldValue, indent + 2);
            } else {
                oss << fieldValue << "\n";
            }
        });
    }

    return oss.str();
}

template<typename T>
inline T YamlFormatter::deserialize(const std::string& content) {
    // 简化版：不支持完整解析，建议使用 yaml-cpp
    throw SerializationException("YAML parsing requires yaml-cpp library. Please enable APOLLO_ENABLE_YAML option.");
}

#endif // APOLLO_YAML_ENABLED

} // namespace formatters

//==============================================================================
// 全局 YAML 便捷函数
//==============================================================================

template<typename T>
inline std::string toYaml(const T& value) {
    return formatters::YamlFormatter::serialize(value);
}

template<typename T>
inline T fromYaml(const std::string& content) {
    return formatters::YamlFormatter::deserialize<T>(content);
}

template<typename T>
inline T fromYamlFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw SerializationException("Cannot open file: " + filePath);
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return fromYaml<T>(content);
}

} // namespace serialization
} // namespace apollo
