/**
 * @file test_log.cpp
 * @brief 日志系统测试
 */

#include "apollo/core/log/log.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

using namespace apollo::core::log;

// 测试辅助宏
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

//==============================================================================
// 测试用例
//==============================================================================

/**
 * @brief 测试日志级别
 */
bool test_log_levels() {
    std::cout << "Running: test_log_levels..." << std::endl;

    TEST_ASSERT(static_cast<uint32_t>(LogLevel::Debug) == 0x0001, "Debug level");
    TEST_ASSERT(static_cast<uint32_t>(LogLevel::Info) == 0x0002, "Info level");
    TEST_ASSERT(static_cast<uint32_t>(LogLevel::Warning) == 0x0004, "Warning level");
    TEST_ASSERT(static_cast<uint32_t>(LogLevel::Error) == 0x0008, "Error level");
    TEST_ASSERT(static_cast<uint32_t>(LogLevel::Critical) == 0x0010, "Critical level");

    // 测试位运算
    LogLevel mask = LogLevel::Debug | LogLevel::Info;
    TEST_ASSERT(hasLevel(mask, LogLevel::Debug), "Has Debug level");
    TEST_ASSERT(hasLevel(mask, LogLevel::Info), "Has Info level");
    TEST_ASSERT(!hasLevel(mask, LogLevel::Error), "No Error level");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试日志记录格式化
 */
bool test_log_record() {
    std::cout << "Running: test_log_record..." << std::endl;

    LogRecord record(LogLevel::Info, "Test message", "test_logger", "test.cpp", 42, "testFunc");

    TEST_ASSERT(record.getLevel() == LogLevel::Info, "Level");
    TEST_ASSERT(record.getMessage() == "Test message", "Message");
    TEST_ASSERT(record.getLoggerName() == "test_logger", "Logger name");

    // 测试格式化
    std::string formatted = record.toShortString();
    TEST_ASSERT(formatted.find("INFO") != std::string::npos, "Contains INFO");
    TEST_ASSERT(formatted.find("Test message") != std::string::npos, "Contains message");

    std::cout << "    Formatted: " << formatted << std::endl;
    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试控制台追加器
 */
bool test_console_appender() {
    std::cout << "Running: test_console_appender..." << std::endl;

    ConsoleAppenderConfig config;
    config.useColor = false; // 测试时关闭颜色
    config.useStderr = false;

    auto appender = std::make_shared<ConsoleAppender>(config);
    appender->setLevel(LogLevel::All);

    TEST_ASSERT(appender->isEnabled(LogLevel::Info), "Info enabled");
    TEST_ASSERT(!appender->isEnabled(LogLevel::None), "None disabled");

    // 写入测试
    LogRecord record(LogLevel::Info, "Console test message");
    appender->append(record);
    appender->flush();

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试文件追加器 - 单文件模式
 */
bool test_file_appender_single() {
    std::cout << "Running: test_file_appender_single..." << std::endl;

    FileAppenderConfig config;
    config.baseName = "test_single";
    config.directory = "test_logs";
    config.extension = "log";
    config.rotationMode = LogRotationMode::None;
    config.flushOnWrite = true;

    auto appender = std::make_shared<FileAppender>(config);
    appender->setLevel(LogLevel::All);

    // 写入测试消息
    for (int i = 0; i < 5; ++i) {
        LogRecord record(LogLevel::Info, "Test message " + std::to_string(i));
        appender->append(record);
    }

    appender->flush();

    TEST_ASSERT(appender->getConfig().rotationMode == LogRotationMode::None, "Rotation mode");
    std::cout << "    File: " << appender->getCurrentFilePath() << std::endl;

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试文件追加器 - 按大小分割
 */
bool test_file_appender_by_size() {
    std::cout << "Running: test_file_appender_by_size..." << std::endl;

    FileAppenderConfig config;
    config.baseName = "test_size";
    config.directory = "test_logs";
    config.extension = "log";
    config.rotationMode = LogRotationMode::BySize;
    config.maxFileSize = 512; // 非常小，用于测试
    config.maxFiles = 10;

    auto appender = std::make_shared<FileAppender>(config);
    appender->setLevel(LogLevel::All);

    // 写入足够多的数据以触发分割
    std::string longMessage(200, 'X'); // 200字节的X
    for (int i = 0; i < 10; ++i) {
        LogRecord record(LogLevel::Info, "Size test message " + std::to_string(i) + " " + longMessage);
        appender->append(record);
    }

    appender->flush();

    std::cout << "    Final file: " << appender->getCurrentFilePath() << std::endl;
    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试文件追加器 - 按日期分割
 */
bool test_file_appender_by_date() {
    std::cout << "Running: test_file_appender_by_date..." << std::endl;

    FileAppenderConfig config;
    config.baseName = "test_date";
    config.directory = "test_logs";
    config.extension = "log";
    config.rotationMode = LogRotationMode::ByDate;

    auto appender = std::make_shared<FileAppender>(config);
    appender->setLevel(LogLevel::All);

    // 写入测试消息
    LogRecord record(LogLevel::Info, "Date test message");
    appender->append(record);
    appender->flush();

    std::cout << "    File: " << appender->getCurrentFilePath() << std::endl;

    // 验证文件路径包含日期
    std::string path = appender->getCurrentFilePath();
    // 当前日期应该是 8 位数字
    TEST_ASSERT(path.find("test_date_") != std::string::npos, "File name pattern");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试文件追加器 - 按大小和日期分割
 */
bool test_file_appender_by_both() {
    std::cout << "Running: test_file_appender_by_both..." << std::endl;

    FileAppenderConfig config;
    config.baseName = "test_both";
    config.directory = "test_logs";
    config.extension = "log";
    config.rotationMode = LogRotationMode::ByBoth;
    config.maxFileSize = 512;
    config.maxFiles = 10;

    auto appender = std::make_shared<FileAppender>(config);
    appender->setLevel(LogLevel::All);

    // 写入数据
    std::string longMessage(200, 'A');
    for (int i = 0; i < 10; ++i) {
        LogRecord record(LogLevel::Info, "Both test " + std::to_string(i) + " " + longMessage);
        appender->append(record);
    }

    appender->flush();

    std::cout << "    File: " << appender->getCurrentFilePath() << std::endl;

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试日志器
 */
bool test_logger() {
    std::cout << "Running: test_logger..." << std::endl;

    auto logger = std::make_shared<Logger>("test_logger", LogLevel::All);

    // 添加控制台追加器
    ConsoleAppenderConfig consoleConfig;
    consoleConfig.useColor = false;
    auto consoleAppender = std::make_shared<ConsoleAppender>(consoleConfig);
    logger->addAppender(consoleAppender);

    // 测试各级别日志
    logger->debug("Debug message");
    logger->info("Info message");
    logger->warning("Warning message");
    logger->error("Error message");
    logger->critical("Critical message");

    // 测试级别过滤
    logger->setLevel(LogLevel::Info | LogLevel::Error | LogLevel::Critical);
    TEST_ASSERT(!logger->isEnabled(LogLevel::Debug), "Debug filtered");
    TEST_ASSERT(logger->isEnabled(LogLevel::Info), "Info enabled");

    logger->flush();

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试日志管理器
 */
bool test_log_manager() {
    std::cout << "Running: test_log_manager..." << std::endl;

    // 创建配置
    LogManagerConfig config;
    config.consoleEnabled = true;
    config.consoleConfig.useColor = false;
    config.fileEnabled = true;
    config.fileConfig.baseName = "test_manager";
    config.fileConfig.directory = "test_logs";
    config.fileConfig.rotationMode = LogRotationMode::ByDate;

    // 初始化
    auto& manager = LogManager::instance();
    manager.initialize(config);

    // 获取默认日志器
    auto defaultLogger = manager.getDefaultLogger();
    TEST_ASSERT(defaultLogger != nullptr, "Default logger exists");
    TEST_ASSERT(defaultLogger->getName() == "root", "Default logger name");

    // 使用默认日志器
    defaultLogger->info("Message from default logger");

    // 创建命名日志器
    auto namedLogger = manager.getLogger("network");
    TEST_ASSERT(namedLogger != nullptr, "Named logger exists");
    namedLogger->info("Message from network logger");

    // 测试便捷宏
    APOLLO_LOG_INFO("Info via macro");
    APOLLO_LOG_WARN("Warning via macro");
    APOLLO_LOG_ERROR("Error via macro");

    manager.flushAll();

    // 清理
    manager.shutdown();

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试线程安全性
 */
bool test_thread_safety() {
    std::cout << "Running: test_thread_safety..." << std::endl;

    LogManagerConfig config;
    config.consoleEnabled = false; // 关闭控制台输出避免混乱
    config.fileEnabled = true;
    config.fileConfig.baseName = "test_thread";
    config.fileConfig.directory = "test_logs";
    config.fileConfig.rotationMode = LogRotationMode::ByDate;

    auto& manager = LogManager::instance();
    manager.initialize(config);

    auto logger = manager.getDefaultLogger();

    // 多线程写入
    const int threadCount = 4;
    const int messagesPerThread = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < messagesPerThread; ++i) {
                logger->info("Thread " + std::to_string(t) + " message " + std::to_string(i));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    manager.flushAll();
    manager.shutdown();

    std::cout << "    Wrote " << (threadCount * messagesPerThread) << " messages from " << threadCount << " threads" << std::endl;
    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// 测试运行器
//==============================================================================

int main() {
    std::cout << "====================================" << std::endl;
    std::cout << "=== Apollo Log System Tests ===" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << std::endl;

    int passed = 0;
    int total = 0;

    auto run = [&](const char* name, bool (*test)()) {
        total++;
        if (test()) passed++;
        else {
            std::cout << "  FAILED!" << std::endl;
        }
    };

    // 运行所有测试
    run("test_log_levels", test_log_levels);
    run("test_log_record", test_log_record);
    run("test_console_appender", test_console_appender);
    run("test_file_appender_single", test_file_appender_single);
    run("test_file_appender_by_size", test_file_appender_by_size);
    run("test_file_appender_by_date", test_file_appender_by_date);
    run("test_file_appender_by_both", test_file_appender_by_both);
    run("test_logger", test_logger);
    run("test_log_manager", test_log_manager);
    run("test_thread_safety", test_thread_safety);

    std::cout << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    std::cout << "====================================" << std::endl;

    return (passed == total) ? 0 : 1;
}

int test_log_main() {
    return main();
}
