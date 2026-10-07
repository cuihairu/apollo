#pragma once

#include "apollo/core/log/appender.h"
#include <fstream>
#include <mutex>
#include <string>
#include <chrono>

namespace apollo {
namespace core {
namespace log {

/**
 * @brief 日志分割模式
 */
enum class LogRotationMode {
    None,       ///< 不分割，单文件
    BySize,     ///< 按文件大小分割
    ByDate,     ///< 按日期分割（每天一个文件）
    ByBoth      ///< 同时按大小和日期分割
};

/**
 * @brief 日期分割类型
 */
enum class LogDateDivide {
    Daily,      ///< 按天分割
    Monthly     ///< 按月分割
};

/**
 * @brief 文件追加器配置
 */
struct FileAppenderConfig {
    std::string baseName = "app";           ///< 基础文件名（不含扩展名）
    std::string extension = "log";          ///< 文件扩展名
    std::string directory = ".";            ///< 日志目录
    size_t maxFileSize = 10 * 1024 * 1024; ///< 单文件最大大小（字节）
    int maxFiles = 100;                     ///< 最多保留文件数量
    bool flushOnWrite = true;               ///< 写入后立即刷新
    bool appendMode = true;                 ///< 追加模式而非覆盖
    LogRotationMode rotationMode = LogRotationMode::ByBoth; ///< 分割模式
    LogDateDivide dateDivide = LogDateDivide::Daily; ///< 日期分割类型

    static FileAppenderConfig daily() {
        FileAppenderConfig cfg;
        cfg.rotationMode = LogRotationMode::ByDate;
        return cfg;
    }

    static FileAppenderConfig bySize(size_t maxSize) {
        FileAppenderConfig cfg;
        cfg.rotationMode = LogRotationMode::BySize;
        cfg.maxFileSize = maxSize;
        return cfg;
    }

    static FileAppenderConfig single() {
        FileAppenderConfig cfg;
        cfg.rotationMode = LogRotationMode::None;
        return cfg;
    }
};

/**
 * @brief 文件日志追加器
 *
 * 支持按文件大小和日期分割日志文件
 * - 按大小分割：当文件超过指定大小时，创建新文件（带 _1, _2 等后缀）
 * - 按日期分割：每天创建新文件（带 _YYYYMMDD 后缀）
 * - 同时分割：日期后缀 + 大小序号（如 app_20241224_1.log）
 */
class FileAppender : public IAppender {
public:
    /**
     * @brief 构造函数
     */
    explicit FileAppender(const FileAppenderConfig& config = FileAppenderConfig{});
    explicit FileAppender(const std::string& filePath);

    /**
     * @brief 析构函数
     */
    ~FileAppender() override;

    // 禁止拷贝和移动
    FileAppender(const FileAppender&) = delete;
    FileAppender& operator=(const FileAppender&) = delete;
    FileAppender(FileAppender&&) = delete;
    FileAppender& operator=(FileAppender&&) = delete;

    /**
     * @brief 追加日志记录
     */
    void append(const LogRecord& record) override;

    /**
     * @brief 刷新缓冲区
     */
    void flush() override;

    /**
     * @brief 获取当前文件路径
     */
    std::string getCurrentFilePath() const;

    /**
     * @brief 获取配置
     */
    const FileAppenderConfig& getConfig() const { return config_; }

    /**
     * @brief 设置配置
     */
    void setConfig(const FileAppenderConfig& config);

    /**
     * @brief 强制执行日志分割
     */
    void rotate();

private:
    FileAppenderConfig config_;
    mutable std::mutex mutex_;
    std::ofstream fileStream_;

    // 当前文件状态
    std::string currentDate_;      ///< 当前日期字符串（YYYYMMDD）
    size_t currentFileIndex_ = 0;  ///< 当前文件序号
    size_t currentFileSize_ = 0;   ///< 当前文件大小

    /**
     * @brief 打开文件
     */
    bool openFile(const std::string& filePath);

    /**
     * @brief 关闭当前文件
     */
    void closeFile();

    /**
     * @brief 检查是否需要分割
     */
    bool checkRotation(const LogRecord& record);

    /**
     * @brief 执行日志分割
     */
    void doRotation();

    /**
     * @brief 生成文件路径
     */
    std::string generateFilePath(const std::string& date, size_t index) const;

    /**
     * @brief 获取当前日期字符串
     */
    std::string getCurrentDate() const;

    /**
     * @brief 获取当前月份字符串
     */
    std::string getCurrentMonth() const;

    /**
     * @brief 清理过期日志文件
     */
    void cleanupOldFiles();

    /**
     * @brief 获取日志文件列表（按修改时间排序）
     */
    std::vector<std::string> getLogFiles() const;

    /**
     * @brief 解析文件路径中的日期和序号
     */
    bool parseFileInfo(const std::string& fileName, std::string& date, size_t& index) const;

    /**
     * @brief 创建目录（如果不存在）
     */
    bool ensureDirectoryExists(const std::string& dir);
};

/**
 * @brief 文件追加器智能指针类型
 */
using FileAppenderPtr = std::shared_ptr<FileAppender>;

} // namespace log
} // namespace core
} // namespace apollo
