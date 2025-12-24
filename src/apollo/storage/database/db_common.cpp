/**
 * @file db_common.cpp
 * @brief 数据库通用实现
 */

#include "apollo/storage/database/db.h"
#include <algorithm>

namespace apollo {
namespace storage {
namespace database {

//==============================================================================
// DbRow 实现
//==============================================================================

const DbValue& DbRow::operator[](size_t index) const {
    static DbValue nullValue;
    return (index < values_.size()) ? values_[index] : nullValue;
}

DbValue& DbRow::operator[](size_t index) {
    if (index >= values_.size()) {
        values_.resize(index + 1);
    }
    return values_[index];
}

const DbValue& DbRow::operator[](const std::string& columnName) const {
    static DbValue nullValue;
    auto it = columnMap_.find(columnName);
    if (it != columnMap_.end() && it->second < values_.size()) {
        return values_[it->second];
    }
    return nullValue;
}

DbValue& DbRow::operator[](const std::string& columnName) {
    auto it = columnMap_.find(columnName);
    if (it != columnMap_.end()) {
        return values_[it->second];
    }
    // 添加新列
    size_t index = values_.size();
    columnMap_[columnName] = index;
    columnNames_.push_back(columnName);
    values_.emplace_back();
    return values_.back();
}

std::optional<std::string> DbRow::getString(size_t index) const {
    if (index >= values_.size()) return std::nullopt;
    if (auto p = std::get_if<std::string>(&values_[index])) {
        return *p;
    }
    if (auto p = std::get_if<int64_t>(&values_[index])) {
        return std::to_string(*p);
    }
    if (auto p = std::get_if<int32_t>(&values_[index])) {
        return std::to_string(*p);
    }
    if (auto p = std::get_if<double>(&values_[index])) {
        return std::to_string(*p);
    }
    if (auto p = std::get_if<bool>(&values_[index])) {
        return *p ? "1" : "0";
    }
    return std::nullopt;
}

std::optional<std::string> DbRow::getString(const std::string& columnName) const {
    auto it = columnMap_.find(columnName);
    if (it == columnMap_.end()) return std::nullopt;
    return getString(it->second);
}

std::optional<int32_t> DbRow::getInt32(size_t index) const {
    if (index >= values_.size()) return std::nullopt;
    if (auto p = std::get_if<int32_t>(&values_[index])) return *p;
    if (auto p = std::get_if<int64_t>(&values_[index])) {
        return static_cast<int32_t>(*p);
    }
    if (auto p = std::get_if<std::string>(&values_[index])) {
        try { return static_cast<int32_t>(std::stoll(*p)); } catch (...) {}
    }
    return std::nullopt;
}

std::optional<int32_t> DbRow::getInt32(const std::string& columnName) const {
    auto it = columnMap_.find(columnName);
    if (it == columnMap_.end()) return std::nullopt;
    return getInt32(it->second);
}

std::optional<int64_t> DbRow::getInt64(size_t index) const {
    if (index >= values_.size()) return std::nullopt;
    if (auto p = std::get_if<int64_t>(&values_[index])) return *p;
    if (auto p = std::get_if<int32_t>(&values_[index])) {
        return static_cast<int64_t>(*p);
    }
    if (auto p = std::get_if<std::string>(&values_[index])) {
        try { return std::stoll(*p); } catch (...) {}
    }
    return std::nullopt;
}

std::optional<int64_t> DbRow::getInt64(const std::string& columnName) const {
    auto it = columnMap_.find(columnName);
    if (it == columnMap_.end()) return std::nullopt;
    return getInt64(it->second);
}

std::optional<double> DbRow::getDouble(size_t index) const {
    if (index >= values_.size()) return std::nullopt;
    if (auto p = std::get_if<double>(&values_[index])) return *p;
    if (auto p = std::get_if<std::string>(&values_[index])) {
        try { return std::stod(*p); } catch (...) {}
    }
    return std::nullopt;
}

std::optional<double> DbRow::getDouble(const std::string& columnName) const {
    auto it = columnMap_.find(columnName);
    if (it == columnMap_.end()) return std::nullopt;
    return getDouble(it->second);
}

std::optional<bool> DbRow::getBool(size_t index) const {
    if (index >= values_.size()) return std::nullopt;
    if (auto p = std::get_if<bool>(&values_[index])) return *p;
    if (auto p = std::get_if<int32_t>(&values_[index])) return *p != 0;
    if (auto p = std::get_if<int64_t>(&values_[index])) return *p != 0;
    if (auto p = std::get_if<std::string>(&values_[index])) {
        return *p == "1" || *p == "true" || *p == "TRUE";
    }
    return std::nullopt;
}

std::optional<bool> DbRow::getBool(const std::string& columnName) const {
    auto it = columnMap_.find(columnName);
    if (it == columnMap_.end()) return std::nullopt;
    return getBool(it->second);
}

std::vector<std::string> DbRow::columnNames() const {
    return columnNames_;
}

void DbRow::addColumn(const std::string& name, const DbValue& value) {
    size_t index = values_.size();
    columnMap_[name] = index;
    columnNames_.push_back(name);
    values_.push_back(value);
}

void DbRow::setColumnMapping(const std::map<std::string, size_t>& mapping) {
    columnMap_ = mapping;
    columnNames_.resize(mapping.size());
    for (const auto& pair : mapping) {
        if (pair.second < columnNames_.size()) {
            columnNames_[pair.second] = pair.first;
        }
    }
}

} // namespace database
} // namespace storage
} // namespace apollo
