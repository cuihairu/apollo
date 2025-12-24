#include "apollo/config/table_loader.h"
#include <cstdlib>

namespace apollo {
namespace config {

// ========== TableLoader ==========

bool TableLoader::loadFile(const std::string& filePath) {
    std::ifstream fin(filePath, std::ios::in);
    if (fin.fail()) {
        std::cerr << "ERROR: Cannot open file: " << filePath << std::endl;
        return false;
    }

    tableName_ = filePath;
    rowCount_ = 0;
    columnCount_ = 0;

    std::string line;
    while (std::getline(fin, line)) {
        // 移除行尾的 \r (Windows CRLF 兼容)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // 转换为可修改的 char* 用于解析
        std::vector<char> lineBuf(line.begin(), line.end());
        lineBuf.push_back('\0');

        if (!parseLine(lineBuf.data(), rowCount_)) {
            std::cerr << "ERROR: Failed to parse line " << rowCount_ << " in file: " << filePath << std::endl;
            fin.close();
            return false;
        }

        rowCount_++;
    }

    fin.close();

    std::cout << "Successfully loaded " << rowCount_ << " lines from: " << filePath << std::endl;
    return true;
}

bool TableLoader::loadContent(const std::string& content) {
    tableName_ = "<string>";
    rowCount_ = 0;
    columnCount_ = 0;

    // 按行分割
    size_t start = 0;
    size_t end = content.find('\n');

    while (end != std::string::npos) {
        std::string line = content.substr(start, end - start);

        // 移除行尾的 \r (Windows CRLF 兼容)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // 转换为可修改的 char* 用于解析
        std::vector<char> lineBuf(line.begin(), line.end());
        lineBuf.push_back('\0');

        if (!parseLine(lineBuf.data(), rowCount_)) {
            std::cerr << "ERROR: Failed to parse line " << rowCount_ << std::endl;
            return false;
        }

        rowCount_++;
        start = end + 1;
        end = content.find('\n', start);
    }

    // 处理最后一行(没有 \n 结尾的)
    if (start < content.length()) {
        std::string line = content.substr(start);

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::vector<char> lineBuf(line.begin(), line.end());
        lineBuf.push_back('\0');

        if (!parseLine(lineBuf.data(), rowCount_)) {
            std::cerr << "ERROR: Failed to parse line " << rowCount_ << std::endl;
            return false;
        }

        rowCount_++;
    }

    return true;
}

std::string TableLoader::getFieldString(const std::string& fieldName, bool check) {
    auto it = fieldMap_.find(fieldName);
    if (it == fieldMap_.end()) {
        if (!check) return "";
        stop("Field not found: [" + fieldName + "]");
    }

    currentRow_ = it->second.rowIndex;
    currentCol_ = it->second.columnIndex;

    return it->second.stringValue;
}

std::string TableLoader::getPosition(const std::string& fieldName) const {
    auto it = fieldMap_.find(fieldName);
    if (it == fieldMap_.end()) {
        return "Field not found: " + fieldName;
    }

    return tableName_ + " row:" + std::to_string(it->second.rowIndex) +
           " col:" + std::to_string(it->second.columnIndex);
}

void TableLoader::stop(const std::string& reason) {
    std::ostringstream oss;
    oss << "TableLoader::Stop";
    if (!tableName_.empty()) {
        oss << " file=" << tableName_;
    }
    oss << " row=" << currentRow_ << " col=" << currentCol_;
    if (!reason.empty()) {
        oss << " reason=" << reason;
    }

    std::string message = oss.str();
    std::cerr << "*** ERROR: " << message << " ***" << std::endl;

    throw std::runtime_error(message);
}

bool TableLoader::parseLine(char* data, int32_t index) {
    // 跳过前两行 (描述 + 字段名)
    if (index < 2) {
        return true;
    }

    int32_t colIndex = 0;
    char* ptr = nullptr;
    const char* delimiter = "*|*";

    while (true) {
        // 查找分隔符
        char* found = nullptr;
        if (ptr == nullptr) {
            found = strstr(data, delimiter);
        } else {
            found = strstr(ptr, delimiter);
        }

        std::string fieldValue;

        if (found == nullptr) {
            // 最后一个字段
            if (ptr == nullptr) {
                fieldValue = data;
            } else {
                fieldValue = ptr;
            }
            ptr = nullptr;
        } else {
            // 中间字段
            if (ptr == nullptr) {
                fieldValue = std::string(data, found - data);
            } else {
                fieldValue = std::string(ptr, found - ptr);
            }
            ptr = found + 3;  // 跳过分隔符 "*|*"
        }

        // 去除空白
        fieldValue = string_utils::trim(fieldValue);

        if (index == 2) {
            // 字段名行
            headerMap_[colIndex] = fieldValue;
            fieldMap_[fieldValue] = FieldInfo{"", -1, colIndex};
        } else {
            // 数据行
            auto headerIt = headerMap_.find(colIndex);
            if (headerIt != headerMap_.end()) {
                auto fieldIt = fieldMap_.find(headerIt->second);
                if (fieldIt != fieldMap_.end()) {
                    fieldIt->second.stringValue = fieldValue;
                    fieldIt->second.rowIndex = index;
                    fieldIt->second.columnIndex = colIndex;
                }
            }
        }

        colIndex++;

        if (ptr == nullptr) {
            break;
        }
    }

    // 更新列数
    if (colIndex > columnCount_) {
        columnCount_ = colIndex;
    }

    // 第3行起，触发回调
    if (index >= 3) {
        try {
            if (!onRowRead()) {
                std::cerr << "ERROR: onRowRead failed at line " << index
                          << " in file: " << tableName_ << std::endl;
                return false;
            }
        } catch (const std::exception& e) {
            std::cerr << "ERROR: Exception in onRowRead at line " << index
                      << " in file " << tableName_ << ": " << e.what() << std::endl;
            return false;
        } catch (...) {
            std::cerr << "ERROR: Unknown exception in onRowRead at line " << index
                      << " in file " << tableName_ << std::endl;
            return false;
        }
    }

    return true;
}

void TableLoader::removeLineEnding(char* data) {
    int32_t len = static_cast<int32_t>(strlen(data));
    if (len == 0) return;

    if (data[len - 1] == '\r') {
        data[len - 1] = '\0';
    } else if (data[len - 1] == '\n') {
        data[len - 1] = '\0';
        if (len > 1 && data[len - 2] == '\r') {
            data[len - 2] = '\0';
        }
    }
}

} // namespace config
} // namespace apollo
