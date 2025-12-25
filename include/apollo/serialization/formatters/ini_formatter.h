#pragma once

#include "apollo/serialization/serializer.h"
#include "apollo/serialization/reflect.h"
#include <sstream>
#include <fstream>

namespace apollo {
namespace serialization {
namespace formatters {

//==============================================================================
// INI Formatter
//==============================================================================

class IniFormatter {
public:
    struct Options {
        bool useSections = true;       // 使用节 (section)
        bool preserveComments = true;  // 保留注释
        char commentChar = ';';        // 注释字符
        bool boolAsYesNo = false;      // 布尔值使用 yes/no 而非 true/false
    };

    //==========================================================================
    // 序列化
    //==========================================================================

    /// 将结构体序列化为 INI 格式字符串
    template<typename T>
    static std::string serialize(const T& value, const Options& options = {});

    //==========================================================================
    // 反序列化
    //==========================================================================

    /// 从 INI 字符串解析
    template<typename T>
    static T deserialize(const std::string& content, const Options& options = {});

private:
    /// 写入 INI 值
    template<typename T>
    static std::string valueToIniString(const T& value, const Options& options);

    /// 解析 INI 行
    static bool parseIniLine(const std::string& line, std::string& section,
                             std::string& key, std::string& value, char commentChar);

    /// 转义 INI 值
    static std::string escapeValue(const std::string& value);
};

//==============================================================================
// INI 节结构（用于内部表示）
//==============================================================================

struct IniSection {
    std::string name;
    std::unordered_map<std::string, std::string> values;
    std::vector<std::string> comments;  // 行注释

    std::string get(const std::string& key, const std::string& defaultValue = "") const {
        auto it = values.find(key);
        return it != values.end() ? it->second : defaultValue;
    }

    void set(const std::string& key, const std::string& value) {
        values[key] = value;
    }

    bool has(const std::string& key) const {
        return values.find(key) != values.end();
    }
};

class IniDocument {
public:
    std::unordered_map<std::string, IniSection> sections;
    std::vector<std::string> globalComments;

    bool hasSection(const std::string& name) const {
        return sections.find(name) != sections.end();
    }

    IniSection& getSection(const std::string& name) {
        return sections[name];
    }

    IniSection* getSectionPtr(const std::string& name) {
        auto it = sections.find(name);
        return it != sections.end() ? &it->second : nullptr;
    }

    std::string serialize(const IniFormatter::Options& options = {}) const;

    bool parse(const std::string& content, const IniFormatter::Options& options = {});
};

//==============================================================================
// IniFormatter 实现
//==============================================================================

template<typename T>
inline std::string IniFormatter::serialize(const T& value, const Options& options) {
    std::ostringstream oss;

    if constexpr (reflect::hasReflection<T>) {
        reflect::forEachField(value, [&](const auto& field, const auto& obj) {
            const auto& fieldValue = field.get(obj);
            oss << field.name << " = " << valueToIniString(fieldValue, options) << "\r\n";
        });
    }

    return oss.str();
}

template<typename T>
inline T IniFormatter::deserialize(const std::string& content, const Options& options) {
    T value{};
    IniDocument doc;
    doc.parse(content, options);

    if constexpr (reflect::hasReflection<T>) {
        reflect::forEachField(value, [&](auto& field, auto& obj) {
            // 从全局节查找
            auto* globalSection = doc.getSectionPtr("");
            if (globalSection && globalSection->has(field.name)) {
                const std::string& strValue = globalSection->get(field.name);
                using FieldType = typename decltype(field)::FieldType;

                try {
                    if constexpr (std::is_same_v<FieldType, std::string>) {
                        field.set(obj, strValue);
                    } else if constexpr (std::is_same_v<FieldType, int> ||
                                       std::is_same_v<FieldType, int32_t>) {
                        field.set(obj, static_cast<int>(std::stoi(strValue)));
                    } else if constexpr (std::is_same_v<FieldType, uint32_t>) {
                        field.set(obj, static_cast<uint32_t>(std::stoul(strValue)));
                    } else if constexpr (std::is_same_v<FieldType, int64_t>) {
                        field.set(obj, static_cast<int64_t>(std::stoll(strValue)));
                    } else if constexpr (std::is_same_v<FieldType, uint64_t>) {
                        field.set(obj, static_cast<uint64_t>(std::stoull(strValue)));
                    } else if constexpr (std::is_same_v<FieldType, float>) {
                        field.set(obj, std::stof(strValue));
                    } else if constexpr (std::is_same_v<FieldType, double>) {
                        field.set(obj, std::stod(strValue));
                    } else if constexpr (std::is_same_v<FieldType, bool>) {
                        std::string v = strValue;
                        for (char& c : v) c = std::tolower(c);
                        field.set(obj, v == "true" || v == "1" || v == "yes");
                    }
                } catch (const std::exception&) {
                    // 转换失败，保持默认值
                }
            }
        });
    }

    return value;
}

template<typename T>
inline std::string IniFormatter::valueToIniString(const T& value, const Options& options) {
    std::ostringstream oss;

    if constexpr (std::is_same_v<T, std::string>) {
        oss << value;
    } else if constexpr (std::is_same_v<T, bool>) {
        if (options.boolAsYesNo) {
            oss << (value ? "yes" : "no");
        } else {
            oss << (value ? "true" : "false");
        }
    } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
        oss << value;
    } else if constexpr (std::is_floating_point_v<T>) {
        oss << value;
    } else {
        oss << value;
    }

    return oss.str();
}

inline std::string IniFormatter::escapeValue(const std::string& value) {
    // 检查是否需要引号
    bool needsQuotes = value.empty() ||
                       value.find(' ') != std::string::npos ||
                       value.find('\t') != std::string::npos ||
                       value.find('\r') != std::string::npos ||
                       value.find('\n') != std::string::npos;

    if (!needsQuotes) {
        return value;
    }

    std::string result = "\"";
    for (char c : value) {
        if (c == '"' || c == '\\') {
            result += '\\';
        }
        result += c;
    }
    result += "\"";
    return result;
}

//==============================================================================
// IniDocument 实现
//==============================================================================

inline std::string IniDocument::serialize(const IniFormatter::Options& options) const {
    std::ostringstream oss;

    // 全局注释
    for (const auto& comment : globalComments) {
        oss << options.commentChar << " " << comment << "\r\n";
    }

    // 各节
    for (const auto& [sectionName, section] : sections) {
        if (!sectionName.empty()) {
            oss << "\r\n[" << sectionName << "]\r\n";
        }

        for (const auto& comment : section.comments) {
            oss << options.commentChar << " " << comment << "\r\n";
        }

        for (const auto& [key, value] : section.values) {
            oss << key << " = " << value << "\r\n";
        }
    }

    return oss.str();
}

inline bool IniDocument::parse(const std::string& content, const IniFormatter::Options& options) {
    std::istringstream iss(content);
    std::string line;
    std::string currentSection;

    while (std::getline(iss, line)) {
        // 去除 BOM
        if (!line.empty() && static_cast<unsigned char>(line[0]) == 0xEF) {
            line.erase(0, 3);
        }

        std::string section, key, value;
        if (parseIniLine(line, section, key, value, options.commentChar)) {
            if (!section.empty()) {
                currentSection = section;
                if (!hasSection(section)) {
                    sections[section] = IniSection{section, {}, {}};
                }
            } else if (!key.empty()) {
                if (currentSection.empty()) {
                    currentSection = "";
                    if (!hasSection("")) {
                        sections[""] = IniSection{"", {}, {}};
                    }
                }
                sections[currentSection].set(key, value);
            }
        }
    }

    return true;
}

inline bool IniFormatter::parseIniLine(const std::string& line, std::string& section,
                                       std::string& key, std::string& value,
                                       char commentChar) {
    std::string trimmed = line;

    // 去除首尾空白
    size_t start = 0;
    while (start < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[start]))) {
        ++start;
    }
    if (start < trimmed.size()) {
        trimmed = trimmed.substr(start);
    }

    // 空行
    if (trimmed.empty()) {
        return true;
    }

    // 注释行
    if (trimmed[0] == commentChar || trimmed[0] == '#' || trimmed[0] == ';') {
        return true;
    }

    // 节 [Section]
    if (trimmed[0] == '[') {
        size_t end = trimmed.find(']');
        if (end != std::string::npos) {
            section = trimmed.substr(1, end - 1);
            return true;
        }
        return false;
    }

    // 键值对 key = value
    size_t eqPos = trimmed.find('=');
    if (eqPos != std::string::npos) {
        key = trimmed.substr(0, eqPos);
        value = trimmed.substr(eqPos + 1);

        // 去除空白
        size_t keyEnd = key.find_last_not_of(" \t");
        if (keyEnd != std::string::npos) {
            key = key.substr(0, keyEnd + 1);
        }

        size_t valueStart = value.find_first_not_of(" \t");
        if (valueStart != std::string::npos) {
            value = value.substr(valueStart);

            // 去除行尾注释
            size_t commentPos = value.find(commentChar);
            if (commentPos != std::string::npos) {
                value = value.substr(0, commentPos);
            }
            valueStart = value.find_last_not_of(" \t");
            if (valueStart != std::string::npos) {
                value = value.substr(0, valueStart + 1);
            }
        } else {
            value.clear();
        }

        return true;
    }

    return true;
}

} // namespace formatters

//==============================================================================
// 全局 INI 便捷函数
//==============================================================================

template<typename T>
inline std::string toIni(const T& value) {
    return formatters::IniFormatter::serialize(value);
}

template<typename T>
inline T fromIni(const std::string& content) {
    return formatters::IniFormatter::deserialize<T>(content);
}

template<typename T>
inline T fromIniFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw SerializationException("Cannot open file: " + filePath);
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return fromIni<T>(content);
}

} // namespace serialization
} // namespace apollo
