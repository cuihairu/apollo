#pragma once

#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <optional>
#include <vector>
#include <sstream>
#include <iostream>
#include <cctype>

namespace apollo {
namespace serialization {
namespace reflect {

//==============================================================================
// 字段包装器 - 存储字段元信息
//==============================================================================

template<typename Class, typename Type>
struct Field {
    using ClassType = Class;
    using FieldType = Type;

    const char* name;
    Type Class::* pointer;      // 成员指针
    const char* description;
    bool required = false;
    bool primaryKey = false;

    // 获取字段值
    const Type& get(const Class& obj) const {
        return obj.*pointer;
    }

    // 设置字段值
    void set(Class& obj, const Type& value) const {
        obj.*pointer = value;
    }

    // 类型信息
    static constexpr bool isBool = std::is_same_v<Type, bool>;
    static constexpr bool isInteger = std::is_integral_v<Type> && !isBool;
    static constexpr bool isFloating = std::is_floating_point_v<Type>;
    static constexpr bool isString = std::is_same_v<Type, std::string>;
};

// 特化：const char* 字段
template<typename Class>
struct Field<Class, const char*> {
    using ClassType = Class;
    using FieldType = const char*;

    const char* name;
    const char* Class::* pointer;
    const char* description;
    bool required = false;
    bool primaryKey = false;

    const char* get(const Class& obj) const {
        return obj.*pointer;
    }

    void set(Class& obj, const std::string& value) const {
        obj.*pointer = value.c_str();
    }
};

//==============================================================================
// 字段创建辅助函数
//==============================================================================

template<typename Class, typename Type>
constexpr auto makeField(const char* name, Type Class::* ptr,
                         const char* desc = "", bool required = false, bool pk = false) {
    return Field<Class, Type>{name, ptr, desc, required, pk};
}

// 字符串指针特化
template<typename Class>
constexpr auto makeField(const char* name, const char* Class::* ptr,
                         const char* desc = "", bool required = false, bool pk = false) {
    return Field<Class, const char*>{name, ptr, desc, required, pk};
}

//==============================================================================
// 反射特征 - 存储结构体的所有字段信息
//==============================================================================

template<typename T>
struct Reflect {
    // 默认：无反射信息
    static constexpr bool hasReflection = false;
    static constexpr const char* typeName = "unknown";
    static constexpr size_t fieldCount = 0;
    using Fields = std::tuple<>;

    static constexpr auto fields = std::make_tuple();
};

//==============================================================================
// 遍历字段的辅助工具
//==============================================================================

namespace detail {

template<typename T, typename Func, size_t... Is>
constexpr void forEachField(T& obj, Func&& func, std::index_sequence<Is...>) {
    constexpr auto& fields = Reflect<T>::fields;
    (func(std::get<Is>(fields), obj), ...);
}

template<typename T, typename Func, size_t... Is>
constexpr void forEachField(const T& obj, Func&& func, std::index_sequence<Is...>) {
    constexpr auto& fields = Reflect<T>::fields;
    (func(std::get<Is>(fields), obj), ...);
}

template<typename T, typename Func, size_t... Is>
constexpr bool anyField(T& obj, Func&& func, std::index_sequence<Is...>) {
    constexpr auto& fields = Reflect<T>::fields;
    return (func(std::get<Is>(fields), obj) || ...);
}

} // namespace detail

template<typename T>
constexpr size_t fieldCount = Reflect<T>::fieldCount;

template<typename T>
constexpr bool hasReflection = Reflect<T>::hasReflection;

/// 遍历结构体的所有字段（可修改）
template<typename T, typename Func>
void forEachField(T& obj, Func&& func) {
    if constexpr (hasReflection<T>) {
        detail::forEachField(obj, std::forward<Func>(func),
            std::make_index_sequence<fieldCount<T>>{});
    }
}

/// 遍历结构体的所有字段（只读）
template<typename T, typename Func>
void forEachField(const T& obj, Func&& func) {
    if constexpr (hasReflection<T>) {
        detail::forEachField(obj, std::forward<Func>(func),
            std::make_index_sequence<fieldCount<T>>{});
    }
}

/// 检查是否任意字段满足条件
template<typename T, typename Func>
bool anyField(T& obj, Func&& func) {
    if constexpr (hasReflection<T>) {
        return detail::anyField(obj, std::forward<Func>(func),
            std::make_index_sequence<fieldCount<T>>{});
    }
    return false;
}

/// 通过字段名获取值
template<typename T>
std::optional<std::reference_wrapper<const std::string>>
getFieldValue(const T& obj, const std::string& name) {
    std::optional<std::string> result;
    forEachField(obj, [&](const auto& field, const auto& o) {
        if (field.name == name) {
            // 使用 stringstream 转换任意类型为字符串
            std::ostringstream ss;
            ss << field.get(o);
            result = ss.str();
        }
    });
    if (result) {
        return std::cref(*result);
    }
    return std::nullopt;
}

/// 通过字段名设置值
template<typename T>
bool setFieldValue(T& obj, const std::string& name, const std::string& value) {
    bool found = false;
    forEachField(obj, [&](auto& field, auto& o) {
        if (field.name == name) {
            using FieldType = typename decltype(field)::FieldType;
            if constexpr (std::is_same_v<FieldType, std::string>) {
                field.set(o, value);
                found = true;
            } else if constexpr (std::is_same_v<FieldType, int> ||
                               std::is_same_v<FieldType, int32_t>) {
                field.set(o, static_cast<int>(std::stoi(value)));
                found = true;
            } else if constexpr (std::is_same_v<FieldType, uint32_t>) {
                field.set(o, static_cast<uint32_t>(std::stoul(value)));
                found = true;
            } else if constexpr (std::is_same_v<FieldType, int64_t>) {
                field.set(o, static_cast<int64_t>(std::stoll(value)));
                found = true;
            } else if constexpr (std::is_same_v<FieldType, uint64_t>) {
                field.set(o, static_cast<uint64_t>(std::stoull(value)));
                found = true;
            } else if constexpr (std::is_same_v<FieldType, float>) {
                field.set(o, std::stof(value));
                found = true;
            } else if constexpr (std::is_same_v<FieldType, double>) {
                field.set(o, std::stod(value));
                found = true;
            } else if constexpr (std::is_same_v<FieldType, bool>) {
                std::string v = value;
                for (char& c : v) c = tolower(c);
                field.set(o, v == "true" || v == "1" || v == "yes");
                found = true;
            }
        }
    });
    return found;
}

//==============================================================================
// 便捷宏定义
//==============================================================================

/// 定义结构体的反射信息
// 使用示例：
// APOLLO_REFLECT(Person,
//     APOLLO_FIELD(name, "姓名"),
//     APOLLO_FIELD(age, "年龄"),
//     APOLLO_FIELD(score, "分数")
// )

#define APOLLO_REFLECT(Type, ...) \
    template<> struct apollo::serialization::reflect::Reflect<Type> { \
        static constexpr bool hasReflection = true; \
        static constexpr const char* typeName = #Type; \
        static constexpr size_t fieldCount = std::tuple_size_v<decltype(std::make_tuple(__VA_ARGS__))>; \
        using Fields = decltype(std::make_tuple(__VA_ARGS__)); \
        static constexpr Fields fields = std::make_tuple(__VA_ARGS__); \
    };

/// 定义一个字段
#define APOLLO_FIELD(Member, ...) \
    ::apollo::serialization::reflect::makeField(#Member, &Type::Member, ##__VA_ARGS__)

/// 定义主键字段
#define APOLLO_PRIMARY_KEY(Member) \
    ::apollo::serialization::reflect::makeField(#Member, &Type::Member, "", false, true)

/// 定义必需字段
#define APOLLO_REQUIRED_FIELD(Member, ...) \
    ::apollo::serialization::reflect::makeField(#Member, &Type::Member, ##__VA_ARGS__, true)

/// 定义字段（带描述）
#define APOLLO_FIELD_DESC(Member, Desc) \
    ::apollo::serialization::reflect::makeField(#Member, &Type::Member, Desc)

//==============================================================================
// 类型名称获取
//==============================================================================

template<typename T>
constexpr const char* getTypeName() {
    return Reflect<T>::typeName;
}

//==============================================================================
// 结构体信息获取
//==============================================================================

template<typename T>
struct StructInfo {
    static std::string getFieldsDescription() {
        if constexpr (!hasReflection<T>) {
            return "<no reflection>";
        }

        std::string result;
        forEachField(static_cast<const T&>(T()), [&](const auto& field, const auto&) {
            if (!result.empty()) result += ", ";
            result += field.name;
            if (field.description && field.description[0] != '\0') {
                result += "(";
                result += field.description;
                result += ")";
            }
        });
        return result;
    }

    static std::vector<std::string> getFieldNames() {
        std::vector<std::string> names;
        if constexpr (hasReflection<T>) {
            names.reserve(fieldCount<T>);
            forEachField(static_cast<const T&>(T()), [&](const auto& field, const auto&) {
                names.push_back(field.name);
            });
        }
        return names;
    }

    static size_t getFieldCount() {
        return fieldCount<T>;
    }
};

} // namespace reflect
} // namespace serialization
} // namespace apollo

//==============================================================================
// 便捷全局宏
//==============================================================================

#define APOLLO_STRUCT_NAME(Type) ::apollo::serialization::reflect::getTypeName<Type>()
#define APOLLO_FIELD_COUNT(Type) ::apollo::serialization::reflect::fieldCount<Type>
#define APOLLO_HAS_REFLECT(Type) ::apollo::serialization::reflect::hasReflection<Type>

// 遍历字段宏（lambda）
#define APOLLO_FOR_EACH_FIELD(obj, lambda) \
    ::apollo::serialization::reflect::forEachField(obj, lambda)

#define APOLLO_FOR_EACH_FIELD_CONST(obj, lambda) \
    ::apollo::serialization::reflect::forEachField(obj, lambda)
