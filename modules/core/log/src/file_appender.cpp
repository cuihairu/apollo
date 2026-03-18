/**
 * @file file_appender.cpp
 * @brief 文件日志追加器实现
 */

#include "apollo/core/log/file_appender.h"
#include "apollo/core/log/log_record.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>
#include <regex>

namespace apollo {
namespace core {
namespace log {

namespace fs = std::filesystem;

//==============================================================================
// FileAppender 实现
//==============================================================================

FileAppender::FileAppender(const FileAppenderConfig& config)
    : config_(config)
{
    // 确保目录存在
    ensureDirectoryExists(config_.directory);

    // 生成初始文件路径并打开
    std::string filePath = generateFilePath(getCurrentDate(), 0);
    openFile(filePath);
}

FileAppender::FileAppender(const std::string& filePath)
    : FileAppender()
{
    // 从文件路径解析配置
    fs::path path(filePath);
    config_.directory = path.parent_path().string();
    if (config_.directory.empty()) {
        config_.directory = ".";
    }
    config_.baseName = path.stem().string();
    config_.extension = path.extension().string();
    if (!config_.extension.empty() && config_.extension[0] == '.') {
        config_.extension = config_.extension.substr(1);
    }
}

FileAppender::~FileAppender() {
    closeFile();
}

void FileAppender::append(const LogRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查级别
    if (!isEnabled(record.getLevel())) {
        return;
    }

    // 检查是否需要分割
    if (checkRotation(record)) {
        doRotation();
    }

    if (!fileStream_.is_open()) {
        return;
    }

    // 格式化并写入
    std::string message = format(record);
    fileStream_ << message << std::endl;
    currentFileSize_ += message.length() + 1; // +1 for newline

    if (config_.flushOnWrite) {
        fileStream_.flush();
    }
}

void FileAppender::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fileStream_.is_open()) {
        fileStream_.flush();
    }
}

std::string FileAppender::getCurrentFilePath() const {
    return generateFilePath(currentDate_, currentFileIndex_);
}

void FileAppender::setConfig(const FileAppenderConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool wasOpen = fileStream_.is_open();
    closeFile();
    config_ = config;
    ensureDirectoryExists(config_.directory);
    if (wasOpen) {
        openFile(generateFilePath(getCurrentDate(), 0));
    }
}

void FileAppender::rotate() {
    std::lock_guard<std::mutex> lock(mutex_);
    doRotation();
}

bool FileAppender::openFile(const std::string& filePath) {
    closeFile();

    fileStream_.open(filePath, std::ios::out | std::ios::app);
    if (!fileStream_.is_open()) {
        std::cerr << "Failed to open log file: " << filePath << std::endl;
        return false;
    }

    // 获取当前文件大小
    fileStream_.seekp(0, std::ios::end);
    currentFileSize_ = static_cast<size_t>(fileStream_.tellp());

    return true;
}

void FileAppender::closeFile() {
    if (fileStream_.is_open()) {
        fileStream_.close();
    }
}

bool FileAppender::checkRotation(const LogRecord& record) {
    if (!fileStream_.is_open()) {
        return true;
    }

    // 检查日期分割
    if (config_.rotationMode == LogRotationMode::ByDate ||
        config_.rotationMode == LogRotationMode::ByBoth) {

        std::string newDate = (config_.dateDivide == LogDateDivide::Monthly)
            ? getCurrentMonth()
            : getCurrentDate();

        if (newDate != currentDate_) {
            return true;
        }
    }

    // 检查大小分割
    if (config_.rotationMode == LogRotationMode::BySize ||
        config_.rotationMode == LogRotationMode::ByBoth) {

        std::string message = format(record);
        if (currentFileSize_ + message.length() + 1 > config_.maxFileSize) {
            return true;
        }
    }

    return false;
}

void FileAppender::doRotation() {
    // 关闭当前文件
    closeFile();

    // 更新日期和序号
    std::string newDate = (config_.dateDivide == LogDateDivide::Monthly)
        ? getCurrentMonth()
        : getCurrentDate();

    if (newDate != currentDate_) {
        // 日期变更，重置序号
        currentDate_ = newDate;
        currentFileIndex_ = 0;
    } else {
        // 同一天，序号递增
        currentFileIndex_++;
    }

    // 打开新文件
    std::string newFilePath = generateFilePath(currentDate_, currentFileIndex_);
    openFile(newFilePath);

    // 清理过期文件
    cleanupOldFiles();
}

std::string FileAppender::generateFilePath(const std::string& date, size_t index) const {
    std::ostringstream oss;
    oss << config_.directory;
    if (!config_.directory.empty() && config_.directory.back() != '/' &&
        config_.directory.back() != '\\') {
        oss << "/";
    }

    oss << config_.baseName;

    // 添加日期后缀
    if (config_.rotationMode == LogRotationMode::ByDate ||
        config_.rotationMode == LogRotationMode::ByBoth) {
        if (!date.empty()) {
            oss << "_" << date;
        }
    }

    // 添加序号后缀
    if (config_.rotationMode == LogRotationMode::BySize ||
        (config_.rotationMode == LogRotationMode::ByBoth && index > 0)) {
        oss << "_" << index;
    }

    // 添加扩展名
    if (!config_.extension.empty()) {
        oss << "." << config_.extension;
    }

    return oss.str();
}

std::string FileAppender::getCurrentDate() const {
    auto now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm;

#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (tm.tm_year + 1900)
        << std::setw(2) << (tm.tm_mon + 1)
        << std::setw(2) << tm.tm_mday;
    return oss.str();
}

std::string FileAppender::getCurrentMonth() const {
    auto now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm;

#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (tm.tm_year + 1900)
        << std::setw(2) << (tm.tm_mon + 1);
    return oss.str();
}

void FileAppender::cleanupOldFiles() {
    if (config_.maxFiles <= 0) {
        return;
    }

    auto files = getLogFiles();
    if (files.size() <= static_cast<size_t>(config_.maxFiles)) {
        return;
    }

    // 删除最旧的文件
    size_t toDelete = files.size() - config_.maxFiles;
    for (size_t i = 0; i < toDelete; ++i) {
        try {
            fs::remove(files[i]);
        } catch (const std::exception& e) {
            std::cerr << "Failed to delete old log file: " << files[i]
                      << ", error: " << e.what() << std::endl;
        }
    }
}

std::vector<std::string> FileAppender::getLogFiles() const {
    std::vector<std::string> result;

    try {
        // 构建文件名模式
        std::string pattern = config_.baseName + "_";

        // 遍历目录
        for (const auto& entry : fs::directory_iterator(config_.directory)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            std::string fileName = entry.path().filename().string();

            // 检查是否匹配日志文件模式
            if (fileName.find(pattern) != 0) {
                continue;
            }

            // 检查扩展名
            if (!config_.extension.empty()) {
                std::string ext = "." + config_.extension;
                if (fileName.size() < ext.size() ||
                    fileName.substr(fileName.size() - ext.size()) != ext) {
                    continue;
                }
            }

            result.push_back(entry.path().string());
        }

        // 按修改时间排序（旧的在前）
        std::sort(result.begin(), result.end(), [](const std::string& a, const std::string& b) {
            return fs::last_write_time(a) < fs::last_write_time(b);
        });

    } catch (const std::exception& e) {
        std::cerr << "Error listing log files: " << e.what() << std::endl;
    }

    return result;
}

bool FileAppender::parseFileInfo(const std::string& fileName, std::string& date, size_t& index) const {
    // 构建正则表达式: basename_YYYYMMDD_index.ext 或 basename_YYYYMMDD.ext
    std::string pattern = config_.baseName + "_";
    if (fileName.find(pattern) != 0) {
        return false;
    }

    std::string rest = fileName.substr(pattern.size());

    // 移除扩展名
    if (!config_.extension.empty()) {
        std::string ext = "." + config_.extension;
        if (rest.size() < ext.size()) {
            return false;
        }
        if (rest.substr(rest.size() - ext.size()) != ext) {
            return false;
        }
        rest = rest.substr(0, rest.size() - ext.size());
    }

    // 尝试解析日期和序号
    std::regex dateIndexRegex(R"((\d{8})(?:_(\d+))?)");
    std::regex monthIndexRegex(R"((\d{6})(?:_(\d+))?)"); // 月度分割

    std::smatch match;
    if (config_.dateDivide == LogDateDivide::Monthly &&
        std::regex_match(rest, match, monthIndexRegex)) {
        date = match[1].str();
        index = match[2].matched ? std::stoull(match[2]) : 0;
        return true;
    } else if (std::regex_match(rest, match, dateIndexRegex)) {
        date = match[1].str();
        index = match[2].matched ? std::stoull(match[2]) : 0;
        return true;
    }

    return false;
}

bool FileAppender::ensureDirectoryExists(const std::string& dir) {
    if (dir.empty() || dir == ".") {
        return true;
    }

    try {
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to create directory: " << dir
                  << ", error: " << e.what() << std::endl;
        return false;
    }
}

} // namespace log
} // namespace core
} // namespace apollo
