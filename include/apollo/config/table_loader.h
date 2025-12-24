#pragma once

#include <string>
#include <unordered_map>
#include <map>
#include <vector>
#include <cstdint>
#include <type_traits>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace apollo {
namespace config {

/**
 * @brief 字段信息
 */
struct FieldInfo {
    std:: stringValue;
    int32_t rowIndex = -1;
    int32_t columnIndex = -1;

    FieldInfo() = default;
    FieldInfo(std::string value, int32_t row = -1, int32_t col = -1)
        : stringValue(std::move(value)), rowIndex(row), columnIndex(col) {}
};

/**
 * @brief 加载结果
 */
struct LoadResult {
    bool success = false;
    std::string message;
    std::string fieldName;
    int32_t rowIndex = -1;
    int32_t columnIndex = -1;
    std::string context;

    LoadResult() = default;

    LoadResult(bool ok, std::string msg = {}, std::string field = {},
               int32_t row = -1, int32_t col = -1)
        : success(ok), message(std::move(msg)), fieldName(std::move(field)),
          rowIndex(row), columnIndex(col) {}

    static LoadResult error(std::string msg, std::string field = {},
                            int32_t row = -1, int32_t col = -1) {
        return LoadResult(false, std::move(msg), std::move(field), row, col);
    }

    static LoadResult ok(std::string msg = {}) {
        return LoadResult(true, std::move(msg));
    }

    LoadResult& withContext(std::string ctx) {
        context = std::move(ctx);
        return *this;
    }

    LoadResult& withColumn(int32_t col) {
        columnIndex = col;
        return *this;
    }
};

/**
 * @brief 表格加载器基类
 *
 * 用于解析格式化的文本表格数据
 * 格式: 每行一条记录，字段用 "*|*" 分隔
 * - 第1行: 描述
 * - 第2行: 字段名
 * - 第3行起: 数据行
 */
class TableLoader {
public:
    TableLoader() = default;
    virtual ~TableLoader() = default;

    /**
     * @brief 从文件加载数据
     * @param filePath 文件路径
     * @return 是否成功
     */
    bool loadFile(const std::string& filePath);

    /**
     * @brief 从字符串加载数据
     * @param content 文件内容
     * @return 是否成功
     */
    bool loadContent(const std::string& content);

    /**
     * @brief 行读取完成后回调(子类重写)
     * @return 返回false将停止加载
     */
    virtual bool onRowRead() { return true; }

    /**
     * @brief 清理数据
     */
    virtual void cleanup() {
        fieldMap_.clear();
        headerMap_.clear();
    }

    /**
     * @brief 获取字段字符串值
     * @param fieldName 字段名
     * @param check 是否检查存在性
     * @return 字段值
     */
    std::string getFieldString(const std::string& fieldName, bool check = true);

    /**
     * @brief 获取字段值(类型安全)
     * @tparam T 目标类型
     * @param fieldName 字段名
     * @return 字段值
     */
    template<typename T>
    T getField(const std::string& fieldName);

    /**
     * @brief 获取当前位置描述
     * @param fieldName 字段名
     * @return 位置描述字符串
     */
    std::string getPosition(const std::string& fieldName) const;

    /**
     * @brief 停止加载并抛出异常
     * @param reason 停止原因
     */
    [[noreturn]] void stop(const std::string& reason = "");

    /**
     * @brief 获取表名/文件名
     */
    const std::string& getTableName() const { return tableName_; }

    /**
     * @brief 获取总行数
     */
    int32_t getRowCount() const { return rowCount_; }

    /**
     * @brief 获取总列数
     */
    int32_t getColumnCount() const { return columnCount_; }

protected:
    int32_t currentRow_ = 0;
    int32_t currentCol_ = 0;

private:
    bool parseLine(char* data, int32_t index);
    void removeLineEnding(char* data);

    std::string tableName_;
    std::unordered_map<std::string, FieldInfo> fieldMap_;
    std::map<int32_t, std::string> headerMap_;
    int32_t rowCount_ = 0;
    int32_t columnCount_ = 0;
};

// ========== 模板方法实现 ==========

template<typename T>
T TableLoader::getField(const std::string& fieldName) {
    auto it = fieldMap_.find(fieldName);
    if (it == fieldMap_.end()) {
        stop("Field not found: [" + fieldName + "]");
    }

    currentRow_ = it->second.rowIndex;
    currentCol_ = it->second.columnIndex;

    const std::string& strValue = it->second.stringValue;

    if constexpr (std::is_same_v<T, std::string>) {
        return strValue;
    }
    else if constexpr (std::is_same_v<T, bool>) {
        return strValue == "1" || strValue == "true";
    }
    else if constexpr (std::is_floating_point_v<T>) {
        try {
            if constexpr (std::is_same_v<T, float>) {
                return static_cast<T>(std::stof(strValue));
            } else {
                return static_cast<T>(std::stod(strValue));
            }
        } catch (...) {
            stop("Field [" + fieldName + "] is not a valid float/double: " + strValue);
        }
    }
    else if constexpr (std::is_integral_v<T>) {
        try {
            if constexpr (std::is_signed_v<T>) {
                if constexpr (sizeof(T) <= 4) {
                    return static_cast<T>(std::stoi(strValue));
                } else {
                    return static_cast<T>(std::stoll(strValue));
                }
            } else {
                if constexpr (sizeof(T) <= 4) {
                    return static_cast<T>(std::stoul(strValue));
                } else {
                    return static_cast<T>(std::stoull(strValue));
                }
            }
        } catch (...) {
            stop("Field [" + fieldName + "] is not a valid integer: " + strValue);
        }
    }
    else {
        stop("Field [" + fieldName + "] has unsupported type");
    }

    return T{};
}

/**
 * @brief 字符串工具函数
 */
namespace string_utils {

/**
 * @brief 去除字符串两端空白
 */
inline std::string trim(std::string str) {
    auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    });

    if (start == str.end()) {
        return "";
    }

    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base();

    return std::string(start, end);
}

/**
 * @brief 按分隔符分割字符串
 * @param str 输入字符串
 * @param delimiter 分隔符
 * @return 分割结果列表
 */
inline std::vector<std::string> split(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> result;

    if (str.empty()) {
        return result;
    }

    size_t start = 0;
    size_t end = str.find(delimiter);

    while (end != std::string::npos) {
        result.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }

    result.push_back(str.substr(start));
    return result;
}

/**
 * @brief 判断字符串是否以指定前缀开头
 */
inline bool startsWith(const std::string& str, const std::string& prefix) {
    if (prefix.size() > str.size()) {
        return false;
    }
    return str.compare(0, prefix.size(), prefix) == 0;
}

/**
 * @brief 判断字符串是否以指定后缀结尾
 */
inline bool endsWith(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) {
        return false;
    }
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

/**
 * @brief 转换为小写
 */
inline std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return str;
}

/**
 * @brief 转换为大写
 */
inline std::string toUpper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return str;
}

} // namespace string_utils

} // namespace config
} // namespace apollo
