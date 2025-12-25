#pragma once

#include "apollo/serialization/serializer.h"
#include "apollo/serialization/reflect.h"
#include <unordered_map>
#include <sstream>
#include <fstream>

namespace apollo {
namespace serialization {
namespace formatters {

//==============================================================================
// CSV Formatter
//==============================================================================

class CsvFormatter {
public:
    struct Options {
        char delimiter = ',';       // 分隔符
        char quote = '"';           // 引号字符
        bool hasHeader = true;      // 是否有标题行
        bool trimWhitespace = true; // 是否修剪空白
        size_t skipRows = 0;        // 跳过的行数
        size_t idColumn = 0;        // ID 列索引（用于 DataTable）
    };

    //==========================================================================
    // 序列化
    //==========================================================================

    /// 将结构体向量序列化为 CSV
    template<typename T>
    static std::string serialize(const std::vector<T>& values, const Options& options = {});

    //==========================================================================
    // 反序列化
    //==========================================================================

    /// 从 CSV 字符串解析为结构体向量
    template<typename T>
    static std::vector<T> deserialize(const std::string& content, const Options& options = {});

private:
    /// 解析 CSV 行
    static std::vector<std::string> parseRow(const std::string& line, char delimiter, char quote);

    /// 转义 CSV 字段
    static std::string escapeField(const std::string& field, char delimiter, char quote);

    /// 修剪空白
    static std::string trim(const std::string& str);

    /// 检查是否需要引号
    static bool needsQuoting(const std::string& field, char delimiter, char quote);
};

//==============================================================================
// DataTable - 游戏配置表
//==============================================================================

class DataTable {
public:
    DataTable() = default;

    //==========================================================================
    // 加载数据
    //==========================================================================

    /// 从 CSV 文件加载
    bool loadFromCsv(const std::string& filePath, const CsvFormatter::Options& options = {});

    /// 从字符串加载
    bool loadFromCsvString(const std::string& content, const CsvFormatter::Options& options = {});

    /// 从 JSON 文件加载（对象数组）
    bool loadFromJson(const std::string& filePath);

    //==========================================================================
    // 数据访问
    //==========================================================================

    /// 获取所有行（原始数据）
    const std::vector<std::vector<std::string>>& rows() const { return rows_; }

    /// 获取表头
    const std::vector<std::string>& headers() const { return headers_; }

    /// 按列名获取列索引
    std::optional<size_t> getColumnIndex(const std::string& columnName) const;

    /// 获取行数
    size_t rowCount() const { return rows_.size(); }

    /// 获取列数
    size_t columnCount() const { return headers_.size(); }

    //==========================================================================
    // 类型安全访问
    //==========================================================================

    /// 类型化的表视图
    template<typename T>
    class Table {
    public:
        Table(DataTable* dataTable) : dataTable_(dataTable) {
            load();
        }

        /// 重新加载数据
        void load() {
            clear();
            if (!dataTable_) return;

            rows_.reserve(dataTable_->rowCount());
            for (size_t i = 0; i < dataTable_->rowCount(); ++i) {
                auto row = dataTable_->getRow<T>(i);
                if (row) {
                    rows_.push_back(std::move(*row));
                    // 建立索引
                    if (row->id > 0) {
                        idIndex_[row->id] = &rows_.back();
                    }
                }
            }
        }

        /// 按 ID 查找（假设有 id 字段）
        T* getById(uint64_t id) {
            auto it = idIndex_.find(id);
            return it != idIndex_.end() ? it->second : nullptr;
        }

        /// 按名称查找（假设有 name 字段）
        T* getByName(const std::string& name) {
            auto it = nameIndex_.find(name);
            return it != nameIndex_.end() ? it->second : nullptr;
        }

        /// 获取所有数据
        std::vector<T>& all() { return rows_; }
        const std::vector<T>& all() const { return rows_; }

        /// 获取数据数量
        size_t size() const { return rows_.size(); }

        /// 清空数据
        void clear() {
            rows_.clear();
            idIndex_.clear();
            nameIndex_.clear();
        }

        /// 检查是否存在
        bool contains(uint64_t id) const {
            return idIndex_.find(id) != idIndex_.end();
        }

        /// 获取第一个元素
        T* first() {
            return rows_.empty() ? nullptr : &rows_.front();
        }

        /// 获取最后一个元素
        T* last() {
            return rows_.empty() ? nullptr : &rows_.back();
        }

        /// 迭代器支持
        auto begin() { return rows_.begin(); }
        auto end() { return rows_.end(); }
        auto begin() const { return rows_.begin(); }
        auto end() const { return rows_.end(); }

    private:
        DataTable* dataTable_ = nullptr;
        std::vector<T> rows_;
        std::unordered_map<uint64_t, T*> idIndex_;
        std::unordered_map<std::string, T*> nameIndex_;
    };

    /// 创建类型化表
    template<typename T>
    Table<T> table() {
        return Table<T>(this);
    }

    //==========================================================================
    // 行数据访问（通用）
    //==========================================================================

    /// 获取指定行作为结构体
    template<typename T>
    std::optional<T> getRow(size_t rowIndex) const;

    /// 获取单元格字符串值
    std::optional<std::string> getCell(size_t row, size_t column) const;
    std::optional<std::string> getCell(size_t row, const std::string& columnName) const;

    /// 获取单元格数值
    template<typename T>
    std::optional<T> getValue(size_t row, size_t column) const;
    template<typename T>
    std::optional<T> getValue(size_t row, const std::string& columnName) const;

    //==========================================================================
    // 数据验证
    //==========================================================================

    /// 检查是否有指定列
    bool hasColumn(const std::string& columnName) const;

    /// 检查数据完整性
    bool validate() const;

    /// 打印表信息
    void printInfo() const;

    //==========================================================================
    // 清空
    //==========================================================================

    void clear() {
        headers_.clear();
        rows_.clear();
        columnIndices_.clear();
    }

private:
    std::vector<std::string> headers_;
    std::vector<std::vector<std::string>> rows_;
    std::unordered_map<std::string, size_t> columnIndices_;

    void buildColumnIndices();
};

//==============================================================================
// CsvFormatter 实现
//==============================================================================

template<typename T>
inline std::string CsvFormatter::serialize(const std::vector<T>& values, const Options& options) {
    std::ostringstream oss;

    // 写入标题行
    if (options.hasHeader) {
        std::vector<std::string> headers;
        if constexpr (reflect::hasReflection<T>) {
            reflect::forEachField(T(), [&](const auto& field, const auto&) {
                headers.push_back(field.name);
            });
        }

        for (size_t i = 0; i < headers.size(); ++i) {
            if (i > 0) oss << options.delimiter;
            oss << escapeField(headers[i], options.delimiter, options.quote);
        }
        oss << "\r\n";
    }

    // 写入数据行
    for (const auto& item : values) {
        std::vector<std::string> fields;
        if constexpr (reflect::hasReflection<T>) {
            reflect::forEachField(item, [&](const auto& field, const auto& obj) {
                std::ostringstream fieldss;
                fieldss << field.get(obj);
                fields.push_back(fieldss.str());
            });
        }

        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) oss << options.delimiter;
            oss << escapeField(fields[i], options.delimiter, options.quote);
        }
        oss << "\r\n";
    }

    return oss.str();
}

template<typename T>
inline std::vector<T> CsvFormatter::deserialize(const std::string& content, const Options& options) {
    std::vector<T> result;
    std::istringstream iss(content);
    std::string line;
    size_t lineNum = 0;

    // 读取标题行
    std::vector<std::string> headers;
    if (options.hasHeader && std::getline(iss, line)) {
        ++lineNum;
        headers = parseRow(line, options.delimiter, options.quote);
        if (options.trimWhitespace) {
            for (auto& h : headers) h = trim(h);
        }
    }

    // 读取数据行
    while (std::getline(iss, line)) {
        ++lineNum;

        // 跳过空行
        if (line.empty() || (options.trimWhitespace && trim(line).empty())) {
            continue;
        }

        // 跳过注释行
        if (line[0] == '#' || line[0] == ';') {
            continue;
        }

        auto fields = parseRow(line, options.delimiter, options.quote);

        if (options.trimWhitespace) {
            for (auto& f : fields) f = trim(f);
        }

        if constexpr (reflect::hasReflection<T>) {
            T item{};
            bool valid = true;

            reflect::forEachField(item, [&](auto& field, auto& obj) {
                // 查找字段对应的列
                size_t colIndex = 0;
                if (options.hasHeader && !headers.empty()) {
                    for (size_t i = 0; i < headers.size(); ++i) {
                        if (headers[i] == field.name) {
                            colIndex = i;
                            break;
                        }
                    }
                } else {
                    // 按顺序匹配
                    // 需要维护当前字段索引，这里简化处理
                }

                if (colIndex < fields.size()) {
                    using FieldType = typename decltype(field)::FieldType;
                    const std::string& value = fields[colIndex];

                    try {
                        if constexpr (std::is_same_v<FieldType, std::string>) {
                            field.set(obj, value);
                        } else if constexpr (std::is_same_v<FieldType, int> ||
                                           std::is_same_v<FieldType, int32_t>) {
                            field.set(obj, static_cast<int>(std::stoi(value)));
                        } else if constexpr (std::is_same_v<FieldType, uint32_t>) {
                            field.set(obj, static_cast<uint32_t>(std::stoul(value)));
                        } else if constexpr (std::is_same_v<FieldType, int64_t>) {
                            field.set(obj, static_cast<int64_t>(std::stoll(value)));
                        } else if constexpr (std::is_same_v<FieldType, uint64_t>) {
                            field.set(obj, static_cast<uint64_t>(std::stoull(value)));
                        } else if constexpr (std::is_same_v<FieldType, float>) {
                            field.set(obj, std::stof(value));
                        } else if constexpr (std::is_same_v<FieldType, double>) {
                            field.set(obj, std::stod(value));
                        } else if constexpr (std::is_same_v<FieldType, bool>) {
                            std::string v = value;
                            for (char& c : v) c = tolower(c);
                            field.set(obj, v == "true" || v == "1" || v == "yes");
                        }
                    } catch (const std::exception&) {
                        // 转换失败，保持默认值
                    }
                }
            });

            if (valid) {
                result.push_back(std::move(item));
            }
        }
    }

    return result;
}

inline std::vector<std::string> CsvFormatter::parseRow(const std::string& line, char delimiter, char quote) {
    std::vector<std::string> result;
    std::string current;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (c == quote) {
            // 处理转义的引号
            if (i + 1 < line.size() && line[i + 1] == quote) {
                current += quote;
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (c == delimiter && !inQuotes) {
            result.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }

    result.push_back(current);
    return result;
}

inline std::string CsvFormatter::escapeField(const std::string& field, char delimiter, char quote) {
    if (needsQuoting(field, delimiter, quote)) {
        std::string result;
        result += quote;
        for (char c : field) {
            if (c == quote) {
                result += quote;  // 转义引号
            }
            result += c;
        }
        result += quote;
        return result;
    }
    return field;
}

inline std::string CsvFormatter::trim(const std::string& str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    if (start == str.size()) return "";

    size_t end = str.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(str[end]))) {
        --end;
    }
    return str.substr(start, end - start + 1);
}

inline bool CsvFormatter::needsQuoting(const std::string& field, char delimiter, char quote) {
    if (field.empty()) return false;
    if (field.find(delimiter) != std::string::npos) return true;
    if (field.find(quote) != std::string::npos) return true;
    if (field.find('\n') != std::string::npos) return true;
    if (field.find('\r') != std::string::npos) return true;
    return false;
}

//==============================================================================
// DataTable 实现
//==============================================================================

inline bool DataTable::loadFromCsv(const std::string& filePath, const CsvFormatter::Options& options) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return loadFromCsvString(content, options);
}

inline bool DataTable::loadFromCsvString(const std::string& content, const CsvFormatter::Options& options) {
    clear();

    std::istringstream iss(content);
    std::string line;
    size_t lineNum = 0;

    // 读取标题行
    if (options.hasHeader && std::getline(iss, line)) {
        ++lineNum;
        headers_ = CsvFormatter::parseRow(line, options.delimiter, options.quote);
        if (options.trimWhitespace) {
            for (auto& h : headers_) h = CsvFormatter::trim(h);
        }
        buildColumnIndices();
    }

    // 读取数据行
    while (std::getline(iss, line)) {
        ++lineNum;

        // 跳过空行
        if (line.empty() || (options.trimWhitespace && CsvFormatter::trim(line).empty())) {
            continue;
        }

        // 跳过注释行
        if (!line.empty() && (line[0] == '#' || line[0] == ';')) {
            continue;
        }

        rows_.push_back(CsvFormatter::parseRow(line, options.delimiter, options.quote));

        if (options.trimWhitespace) {
            for (auto& f : rows_.back()) f = CsvFormatter::trim(f);
        }
    }

    return true;
}

inline bool DataTable::loadFromJson(const std::string& filePath) {
    // TODO: 实现 JSON 数组加载
    return false;
}

inline std::optional<size_t> DataTable::getColumnIndex(const std::string& columnName) const {
    auto it = columnIndices_.find(columnName);
    if (it != columnIndices_.end()) {
        return it->second;
    }
    return std::nullopt;
}

template<typename T>
inline std::optional<T> DataTable::getRow(size_t rowIndex) const {
    if (rowIndex >= rows_.size()) {
        return std::nullopt;
    }

    T item{};
    const auto& row = rows_[rowIndex];

    if constexpr (reflect::hasReflection<T>) {
        reflect::forEachField(item, [&](auto& field, auto& obj) {
            auto colIt = columnIndices_.find(field.name);
            if (colIt != columnIndices_.end() && colIt->second < row.size()) {
                const std::string& value = row[colIt->second];

                using FieldType = typename decltype(field)::FieldType;
                try {
                    if constexpr (std::is_same_v<FieldType, std::string>) {
                        field.set(obj, value);
                    } else if constexpr (std::is_same_v<FieldType, int> ||
                                       std::is_same_v<FieldType, int32_t>) {
                        field.set(obj, static_cast<int>(std::stoi(value)));
                    } else if constexpr (std::is_same_v<FieldType, uint32_t>) {
                        field.set(obj, static_cast<uint32_t>(std::stoul(value)));
                    } else if constexpr (std::is_same_v<FieldType, int64_t>) {
                        field.set(obj, static_cast<int64_t>(std::stoll(value)));
                    } else if constexpr (std::is_same_v<FieldType, uint64_t>) {
                        field.set(obj, static_cast<uint64_t>(std::stoull(value)));
                    } else if constexpr (std::is_same_v<FieldType, float>) {
                        field.set(obj, std::stof(value));
                    } else if constexpr (std::is_same_v<FieldType, double>) {
                        field.set(obj, std::stod(value));
                    } else if constexpr (std::is_same_v<FieldType, bool>) {
                        std::string v = value;
                        for (char& c : v) c = std::tolower(c);
                        field.set(obj, v == "true" || v == "1" || v == "yes");
                    }
                } catch (const std::exception&) {
                    // 保持默认值
                }
            }
        });
    }

    return item;
}

inline std::optional<std::string> DataTable::getCell(size_t row, size_t column) const {
    if (row >= rows_.size() || column >= headers_.size()) {
        return std::nullopt;
    }
    if (column >= rows_[row].size()) {
        return "";
    }
    return rows_[row][column];
}

inline std::optional<std::string> DataTable::getCell(size_t row, const std::string& columnName) const {
    auto col = getColumnIndex(columnName);
    if (!col) return std::nullopt;
    return getCell(row, *col);
}

template<typename T>
inline std::optional<T> DataTable::getValue(size_t row, size_t column) const {
    auto cell = getCell(row, column);
    if (!cell) return std::nullopt;

    try {
        if constexpr (std::is_same_v<T, std::string>) {
            return *cell;
        } else if constexpr (std::is_integral_v<T>) {
            return static_cast<T>(std::stoll(*cell));
        } else if constexpr (std::is_floating_point_v<T>) {
            return static_cast<T>(std::stod(*cell));
        } else if constexpr (std::is_same_v<T, bool>) {
            std::string v = *cell;
            for (char& c : v) c = std::tolower(c);
            return v == "true" || v == "1" || v == "yes";
        }
    } catch (...) {
        return std::nullopt;
    }
}

template<typename T>
inline std::optional<T> DataTable::getValue(size_t row, const std::string& columnName) const {
    auto col = getColumnIndex(columnName);
    if (!col) return std::nullopt;
    return getValue<T>(row, *col);
}

inline bool DataTable::hasColumn(const std::string& columnName) const {
    return columnIndices_.find(columnName) != columnIndices_.end();
}

inline bool DataTable::validate() const {
    // 检查每行的列数是否一致
    if (rows_.empty()) return true;

    size_t expectedCols = headers_.empty() ? rows_[0].size() : headers_.size();
    for (const auto& row : rows_) {
        if (row.size() != expectedCols) {
            return false;
        }
    }
    return true;
}

inline void DataTable::printInfo() const {
    std::cout << "DataTable Info:" << std::endl;
    std::cout << "  Headers: " << headers_.size() << " [";
    for (size_t i = 0; i < headers_.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << headers_[i];
    }
    std::cout << "]" << std::endl;
    std::cout << "  Rows: " << rows_.size() << std::endl;
}

inline void DataTable::buildColumnIndices() {
    columnIndices_.clear();
    for (size_t i = 0; i < headers_.size(); ++i) {
        columnIndices_[headers_[i]] = i;
    }
}

} // namespace formatters

//==============================================================================
// 全局 CSV 便捷函数
//==============================================================================

template<typename T>
inline std::string toCsv(const std::vector<T>& values, bool hasHeader = true) {
    return formatters::CsvFormatter::serialize(values, {',', '"', hasHeader});
}

template<typename T>
inline std::vector<T> fromCsv(const std::string& content) {
    return formatters::CsvFormatter::deserialize<T>(content);
}

template<typename T>
inline std::vector<T> fromCsvFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw SerializationException("Cannot open file: " + filePath);
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return fromCsv<T>(content);
}

} // namespace serialization
} // namespace apollo
