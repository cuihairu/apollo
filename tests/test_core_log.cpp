/**
 * @file test_core_log.cpp
 * @brief Core log module unit tests
 */

#include "apollo/core/log/log_level.h"
#include "apollo/core/log/log_record.h"
#include "apollo/core/log/appender.h"
#include "apollo/core/log/console_appender.h"
#include "apollo/core/log/file_appender.h"
#include "apollo/core/log/logger.hpp"
#include "apollo/core/log/log_manager.h"
#include <iostream>
#include <sstream>
#include <string_view>
#include <fstream>
#include <filesystem>

using namespace apollo::core::log;

namespace {

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

//==============================================================================
// LogLevel Tests
//==============================================================================

bool test_log_level_values() {
    std::cout << "Running: test_log_level_values..." << std::endl;

    TEST_ASSERT(static_cast<uint32_t>(LogLevel::None) == 0x0000, "None value");
    TEST_ASSERT(static_cast<uint32_t>(LogLevel::Debug) == 0x0001, "Debug value");
    TEST_ASSERT(static_cast<uint32_t>(LogLevel::Info) == 0x0002, "Info value");
    TEST_ASSERT(static_cast<uint32_t>(LogLevel::Warning) == 0x0004, "Warning value");
    TEST_ASSERT(static_cast<uint32_t>(LogLevel::Error) == 0x0008, "Error value");
    TEST_ASSERT(static_cast<uint32_t>(LogLevel::Critical) == 0x0010, "Critical value");
    TEST_ASSERT(static_cast<uint32_t>(LogLevel::All) == 0xFFFF, "All value");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_level_to_string() {
    std::cout << "Running: test_log_level_to_string..." << std::endl;

    TEST_ASSERT(std::string_view(toString(LogLevel::Debug)) == "DEBUG", "Debug to string");
    TEST_ASSERT(std::string_view(toString(LogLevel::Info)) == "INFO", "Info to string");
    TEST_ASSERT(std::string_view(toString(LogLevel::Warning)) == "WARN", "Warning to string");
    TEST_ASSERT(std::string_view(toString(LogLevel::Error)) == "ERROR", "Error to string");
    TEST_ASSERT(std::string_view(toString(LogLevel::Critical)) == "CRITICAL", "Critical to string");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_level_from_string() {
    std::cout << "Running: test_log_level_from_string..." << std::endl;

    TEST_ASSERT(fromString("DEBUG") == LogLevel::Debug, "DEBUG from string");
    TEST_ASSERT(fromString("INFO") == LogLevel::Info, "INFO from string");
    TEST_ASSERT(fromString("WARN") == LogLevel::Warning, "WARN from string");
    TEST_ASSERT(fromString("WARNING") == LogLevel::Warning, "WARNING from string");
    TEST_ASSERT(fromString("ERROR") == LogLevel::Error, "ERROR from string");
    TEST_ASSERT(fromString("CRITICAL") == LogLevel::Critical, "CRITICAL from string");
    TEST_ASSERT(fromString("FATAL") == LogLevel::Critical, "FATAL from string");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_level_bitwise_or() {
    std::cout << "Running: test_log_level_bitwise_or..." << std::endl;

    LogLevel combined = LogLevel::Debug | LogLevel::Info;
    TEST_ASSERT(static_cast<uint32_t>(combined) == 0x0003, "OR combines correctly");

    LogLevel allLevels = LogLevel::Debug | LogLevel::Info | LogLevel::Warning |
                         LogLevel::Error | LogLevel::Critical;
    TEST_ASSERT(static_cast<uint32_t>(allLevels) == 0x001F, "OR all levels");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_level_bitwise_and() {
    std::cout << "Running: test_log_level_bitwise_and..." << std::endl;

    LogLevel mask = LogLevel::Debug | LogLevel::Info;
    LogLevel result = mask & LogLevel::Debug;
    TEST_ASSERT(static_cast<uint32_t>(result) == 0x0001, "AND filters correctly");

    result = mask & LogLevel::Error;
    TEST_ASSERT(static_cast<uint32_t>(result) == 0x0000, "AND excludes non-matching");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_has_level() {
    std::cout << "Running: test_has_level..." << std::endl;

    LogLevel mask = LogLevel::Debug | LogLevel::Info;
    TEST_ASSERT(hasLevel(mask, LogLevel::Debug), "Has Debug level");
    TEST_ASSERT(hasLevel(mask, LogLevel::Info), "Has Info level");
    TEST_ASSERT(!hasLevel(mask, LogLevel::Error), "Does not have Error level");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// LogRecord Tests
//==============================================================================

bool test_log_record_default_constructor() {
    std::cout << "Running: test_log_record_default_constructor..." << std::endl;

    LogRecord record;
    TEST_ASSERT(record.getLevel() == LogLevel::Info, "Default level is Info");
    TEST_ASSERT(record.getMessage().empty(), "Default message is empty");
    TEST_ASSERT(record.getLoggerName().empty(), "Default logger name is empty");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_record_constructor() {
    std::cout << "Running: test_log_record_constructor..." << std::endl;

    LogRecord record(LogLevel::Error, "Test message", "TestLogger",
                     "test.cpp", 42, "testFunction");

    TEST_ASSERT(record.getLevel() == LogLevel::Error, "Level set correctly");
    TEST_ASSERT(record.getMessage() == "Test message", "Message set correctly");
    TEST_ASSERT(record.getLoggerName() == "TestLogger", "Logger name set correctly");
    TEST_ASSERT(record.getFile() == "test.cpp", "File set correctly");
    TEST_ASSERT(record.getLine() == 42, "Line set correctly");
    TEST_ASSERT(record.getFunction() == "testFunction", "Function set correctly");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_record_setters() {
    std::cout << "Running: test_log_record_setters..." << std::endl;

    LogRecord record;
    record.setLevel(LogLevel::Critical);
    record.setMessage("Critical error");
    record.setLoggerName("CriticalLogger");
    record.setLocation("critical.cpp", 100, "criticalFunc");

    TEST_ASSERT(record.getLevel() == LogLevel::Critical, "Level updated");
    TEST_ASSERT(record.getMessage() == "Critical error", "Message updated");
    TEST_ASSERT(record.getLoggerName() == "CriticalLogger", "Logger name updated");
    TEST_ASSERT(record.getFile() == "critical.cpp", "File updated");
    TEST_ASSERT(record.getLine() == 100, "Line updated");
    TEST_ASSERT(record.getFunction() == "criticalFunc", "Function updated");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_record_format_default() {
    std::cout << "Running: test_log_record_format_default..." << std::endl;

    LogRecord record(LogLevel::Info, "Test message", "TestLogger");
    std::string formatted = record.format();

    TEST_ASSERT(!formatted.empty(), "Format produces output");
    TEST_ASSERT(formatted.find("INFO") != std::string::npos, "Contains level");
    TEST_ASSERT(formatted.find("TestLogger") != std::string::npos, "Contains logger name");
    TEST_ASSERT(formatted.find("Test message") != std::string::npos, "Contains message");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_record_format_custom() {
    std::cout << "Running: test_log_record_format_custom..." << std::endl;

    LogRecord record(LogLevel::Error, "Error message", "ErrorLogger",
                     "error.cpp", 123, "errorFunc");
    std::string formatted = record.format("[%l] %v");

    TEST_ASSERT(formatted.find("ERROR") != std::string::npos, "Contains level");
    TEST_ASSERT(formatted.find("Error message") != std::string::npos, "Contains message");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_record_to_short_string() {
    std::cout << "Running: test_log_record_to_short_string..." << std::endl;

    LogRecord record(LogLevel::Warning, "Warning msg");
    std::string shortStr = record.toShortString();

    TEST_ASSERT(!shortStr.empty(), "Short string not empty");
    TEST_ASSERT(shortStr.find("WARN") != std::string::npos, "Contains level");
    TEST_ASSERT(shortStr.find("Warning msg") != std::string::npos, "Contains message");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_record_to_full_string() {
    std::cout << "Running: test_log_record_to_full_string..." << std::endl;

    LogRecord record(LogLevel::Debug, "Debug msg", "DebugLogger",
                     "debug.cpp", 456, "debugFunc");
    std::string fullStr = record.toFullString();

    TEST_ASSERT(!fullStr.empty(), "Full string not empty");
    TEST_ASSERT(fullStr.find("DEBUG") != std::string::npos, "Contains level");
    TEST_ASSERT(fullStr.find("Debug msg") != std::string::npos, "Contains message");
    TEST_ASSERT(fullStr.find("debug.cpp") != std::string::npos, "Contains file");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_record_timestamp() {
    std::cout << "Running: test_log_record_timestamp..." << std::endl;

    // Create two records with a small delay
    LogRecord record1(LogLevel::Info, "First");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    LogRecord record2(LogLevel::Info, "Second");

    auto time1 = record1.getTimestamp().time_since_epoch();
    auto time2 = record2.getTimestamp().time_since_epoch();

    TEST_ASSERT(time2 > time1, "Timestamp increases");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// IAppender Tests
//==============================================================================

class TestAppender : public IAppender {
public:
    std::vector<std::string> records;
    int flushCount = 0;
    bool enabled = true;

    void append(const LogRecord& record) override {
        // Check level filter using IAppender's isEnabled
        if (enabled && isEnabled(record.getLevel())) {
            records.push_back(format(record));
        }
    }

    void flush() override {
        flushCount++;
    }
};

bool test_appender_level_filter() {
    std::cout << "Running: test_appender_level_filter..." << std::endl;

    TestAppender appender;
    appender.setLevel(LogLevel::Error);

    LogRecord debugRecord(LogLevel::Debug, "Debug");
    LogRecord infoRecord(LogLevel::Info, "Info");
    LogRecord errorRecord(LogLevel::Error, "Error");

    appender.append(debugRecord);
    appender.append(infoRecord);
    appender.append(errorRecord);

    TEST_ASSERT(appender.records.size() == 1, "Only error level passes filter");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_appender_pattern() {
    std::cout << "Running: test_appender_pattern..." << std::endl;

    TestAppender appender;
    appender.setPattern("%l - %v");

    LogRecord record(LogLevel::Info, "Test");
    appender.append(record);

    TEST_ASSERT(appender.records.size() == 1, "Record appended");
    TEST_ASSERT(appender.records[0].find("INFO - Test") != std::string::npos,
                "Pattern applied correctly");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_appender_flush() {
    std::cout << "Running: test_appender_flush..." << std::endl;

    TestAppender appender;
    appender.flush();

    TEST_ASSERT(appender.flushCount == 1, "Flush called");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// ConsoleAppender Tests
//==============================================================================

bool test_console_appender_construction() {
    std::cout << "Running: test_console_appender_construction..." << std::endl;

    ConsoleAppender appender;
    TEST_ASSERT(appender.getLevel() == LogLevel::All, "Default level is All");

    ConsoleAppenderConfig config;
    config.useColor = false;
    config.useStderr = true;
    ConsoleAppender appender2(config);
    TEST_ASSERT(appender2.getConfig().useColor == false, "Config applied");
    TEST_ASSERT(appender2.getConfig().useStderr == true, "Stderr config applied");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_console_appender_append() {
    std::cout << "Running: test_console_appender_append..." << std::endl;

    ConsoleAppender appender;
    LogRecord record(LogLevel::Info, "Console test");

    // Just verify it doesn't crash
    appender.append(record);
    appender.flush();

    TEST_ASSERT(true, "Append and flush work");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_console_appender_set_config() {
    std::cout << "Running: test_console_appender_set_config..." << std::endl;

    ConsoleAppender appender;

    ConsoleAppenderConfig newConfig;
    newConfig.useColor = false;
    newConfig.flushOnWrite = true;

    appender.setConfig(newConfig);

    TEST_ASSERT(appender.getConfig().useColor == false, "Config updated");
    TEST_ASSERT(appender.getConfig().flushOnWrite == true, "Flush config updated");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// FileAppender Tests
//==============================================================================

bool test_file_appender_construction() {
    std::cout << "Running: test_file_appender_construction..." << std::endl;

    FileAppenderConfig config;
    config.baseName = "test";
    config.extension = "log";
    config.rotationMode = LogRotationMode::None;

    FileAppender appender(config);

    TEST_ASSERT(appender.getConfig().baseName == "test", "Base name set");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_file_appender_write() {
    std::cout << "Running: test_file_appender_write..." << std::endl;

    std::string testDir = "test_logs";
    std::string testFile = "test_logs/test_write.log";

    // Clean up any existing test file
    try {
        std::filesystem::remove_all(testDir);
    } catch (...) {}

    try {
        FileAppenderConfig config;
        config.directory = testDir;
        config.baseName = "test_write";
        config.extension = "log";
        config.rotationMode = LogRotationMode::None;

        {
            FileAppender appender(config);
            LogRecord record(LogLevel::Info, "Test message to file");
            appender.append(record);
            appender.flush();
        }

        // Verify file was created and contains message
        std::ifstream file(testFile);
        TEST_ASSERT(file.good(), "File created");

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        file.close();

        TEST_ASSERT(content.find("Test message to file") != std::string::npos,
                    "Message written to file");
        TEST_ASSERT(content.find("INFO") != std::string::npos, "Level in file");

        // Clean up
        std::filesystem::remove_all(testDir);
    } catch (...) {
        // Clean up on error
        try {
            std::filesystem::remove_all(testDir);
        } catch (...) {}
        throw;
    }

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_file_appender_rotation_by_size() {
    std::cout << "Running: test_file_appender_rotation_by_size..." << std::endl;

    std::string testDir = "test_logs_rotation";
    try {
        std::filesystem::remove_all(testDir);
    } catch (...) {}

    try {
        FileAppenderConfig config;
        config.directory = testDir;
        config.baseName = "rotation_test";
        config.extension = "log";
        config.rotationMode = LogRotationMode::BySize;
        config.maxFileSize = 200;  // Small size for testing
        config.maxFiles = 3;

        {
            FileAppender appender(config);

            // Write enough data to trigger rotation
            for (int i = 0; i < 10; ++i) {
                LogRecord record(LogLevel::Info,
                    "This is a test message that should be long enough to trigger rotation " +
                    std::to_string(i));
                appender.append(record);
            }
            appender.flush();
        }

        // Check that multiple files were created
        int fileCount = 0;
        for (const auto& entry : std::filesystem::directory_iterator(testDir)) {
            if (entry.path().extension() == ".log") {
                fileCount++;
            }
        }

        TEST_ASSERT(fileCount > 1, "Multiple files created due to rotation");

        // Clean up
        std::filesystem::remove_all(testDir);
    } catch (...) {
        // Clean up on error
        try {
            std::filesystem::remove_all(testDir);
        } catch (...) {}
        throw;
    }

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_file_appender_get_current_file_path() {
    std::cout << "Running: test_file_appender_get_current_file_path..." << std::endl;

    std::string testDir = "test_logs_path";
    try {
        std::filesystem::remove_all(testDir);
    } catch (...) {}

    FileAppenderConfig config;
    config.directory = testDir;
    config.baseName = "path_test";
    config.extension = "log";
    config.rotationMode = LogRotationMode::None;

    try {
        FileAppender appender(config);

        // Write something first to ensure file is opened
        LogRecord record(LogLevel::Info, "Test");
        appender.append(record);

        std::string path = appender.getCurrentFilePath();

        TEST_ASSERT(!path.empty(), "Path not empty");
        TEST_ASSERT(path.find("path_test") != std::string::npos, "Path contains base name");

        // Clean up
        try {
            std::filesystem::remove_all(testDir);
        } catch (...) {}
    } catch (...) {
        // Clean up on error
        try {
            std::filesystem::remove_all(testDir);
        } catch (...) {}
        throw;
    }

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// LogManager Tests
//==============================================================================

bool test_log_manager_singleton() {
    std::cout << "Running: test_log_manager_singleton..." << std::endl;

    auto& instance1 = LogManager::instance();
    auto& instance2 = LogManager::instance();

    TEST_ASSERT(&instance1 == &instance2, "Same instance returned");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_manager_initialize_default() {
    std::cout << "Running: test_log_manager_initialize_default..." << std::endl;

    auto& manager = LogManager::instance();
    manager.initialize();

    auto logger = manager.getDefaultLogger();
    TEST_ASSERT(logger != nullptr, "Default logger created");

    manager.shutdown();

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_manager_create_logger() {
    std::cout << "Running: test_log_manager_create_logger..." << std::endl;

    auto& manager = LogManager::instance();
    manager.initialize();

    auto logger = manager.createLogger("TestLogger", LogLevel::Debug);
    TEST_ASSERT(logger != nullptr, "Logger created");
    TEST_ASSERT(logger->getName() == "TestLogger", "Logger name set");
    TEST_ASSERT(logger->getLevel() == LogLevel::Debug, "Logger level set");

    manager.shutdown();

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_manager_get_logger() {
    std::cout << "Running: test_log_manager_get_logger..." << std::endl;

    auto& manager = LogManager::instance();
    manager.initialize();

    // Create a logger
    manager.createLogger("MyLogger", LogLevel::Info);

    // Get the same logger
    auto logger = manager.getLogger("MyLogger");
    TEST_ASSERT(logger != nullptr, "Logger retrieved");
    TEST_ASSERT(logger->getName() == "MyLogger", "Correct logger retrieved");

    manager.shutdown();

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_manager_has_logger() {
    std::cout << "Running: test_log_manager_has_logger..." << std::endl;

    auto& manager = LogManager::instance();
    manager.initialize();

    manager.createLogger("ExistingLogger");

    TEST_ASSERT(manager.hasLogger("ExistingLogger"), "Logger exists");
    TEST_ASSERT(!manager.hasLogger("NonExistentLogger"), "Non-existent logger");

    manager.shutdown();

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_manager_remove_logger() {
    std::cout << "Running: test_log_manager_remove_logger..." << std::endl;

    auto& manager = LogManager::instance();
    manager.initialize();

    manager.createLogger("ToRemove");
    TEST_ASSERT(manager.hasLogger("ToRemove"), "Logger exists");

    manager.removeLogger("ToRemove");
    TEST_ASSERT(!manager.hasLogger("ToRemove"), "Logger removed");

    manager.shutdown();

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_manager_get_logger_names() {
    std::cout << "Running: test_log_manager_get_logger_names..." << std::endl;

    auto& manager = LogManager::instance();
    manager.initialize();

    manager.createLogger("Logger1");
    manager.createLogger("Logger2");
    manager.createLogger("Logger3");

    auto names = manager.getLoggerNames();

    TEST_ASSERT(names.size() >= 3, "At least 3 loggers");

    bool has1 = false, has2 = false, has3 = false;
    for (const auto& name : names) {
        if (name == "Logger1") has1 = true;
        if (name == "Logger2") has2 = true;
        if (name == "Logger3") has3 = true;
    }

    TEST_ASSERT(has1 && has2 && has3, "All loggers present in list");

    manager.shutdown();

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_manager_default_level() {
    std::cout << "Running: test_log_manager_default_level..." << std::endl;

    auto& manager = LogManager::instance();
    manager.initialize();

    manager.setDefaultLevel(LogLevel::Warning);
    TEST_ASSERT(manager.getDefaultLevel() == LogLevel::Warning, "Default level set");

    manager.shutdown();

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// LogManagerConfig Tests
//==============================================================================

bool test_log_manager_config_default() {
    std::cout << "Running: test_log_manager_config_default..." << std::endl;

    auto config = LogManagerConfig::createDefault();

    TEST_ASSERT(config.consoleEnabled, "Console enabled by default");
    TEST_ASSERT(!config.fileEnabled, "File disabled by default");
    TEST_ASSERT(config.defaultLevel == LogLevel::Info, "Default level is Info");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_manager_config_console_only() {
    std::cout << "Running: test_log_manager_config_console_only..." << std::endl;

    auto config = LogManagerConfig::createConsoleOnly(LogLevel::Debug);

    TEST_ASSERT(config.consoleEnabled, "Console enabled");
    TEST_ASSERT(!config.fileEnabled, "File disabled");
    TEST_ASSERT(config.defaultLevel == LogLevel::Debug, "Level is Debug");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_manager_config_file_only() {
    std::cout << "Running: test_log_manager_config_file_only..." << std::endl;

    auto config = LogManagerConfig::createFileOnly("test.log", LogLevel::Error);

    TEST_ASSERT(!config.consoleEnabled, "Console disabled");
    TEST_ASSERT(config.fileEnabled, "File enabled");
    TEST_ASSERT(config.fileConfig.baseName == "test.log", "File path set");
    TEST_ASSERT(config.defaultLevel == LogLevel::Error, "Level is Error");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_log_manager_config_combined() {
    std::cout << "Running: test_log_manager_config_combined..." << std::endl;

    auto config = LogManagerConfig::createCombined("combined.log", LogLevel::Warning);

    TEST_ASSERT(config.consoleEnabled, "Console enabled");
    TEST_ASSERT(config.fileEnabled, "File enabled");
    TEST_ASSERT(config.fileConfig.baseName == "combined.log", "File path set");
    TEST_ASSERT(config.defaultLevel == LogLevel::Warning, "Level is Warning");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// FileAppenderConfig Tests
//==============================================================================

bool test_file_appender_config_defaults() {
    std::cout << "Running: test_file_appender_config_defaults..." << std::endl;

    FileAppenderConfig config;

    TEST_ASSERT(config.baseName == "app", "Default base name");
    TEST_ASSERT(config.extension == "log", "Default extension");
    TEST_ASSERT(config.directory == ".", "Default directory");
    TEST_ASSERT(config.maxFileSize == 10 * 1024 * 1024, "Default max file size");
    TEST_ASSERT(config.maxFiles == 100, "Default max files");
    TEST_ASSERT(config.flushOnWrite == true, "Default flush on write");
    TEST_ASSERT(config.appendMode == true, "Default append mode");
    TEST_ASSERT(config.rotationMode == LogRotationMode::ByBoth, "Default rotation mode");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_file_appender_config_daily() {
    std::cout << "Running: test_file_appender_config_daily..." << std::endl;

    auto config = FileAppenderConfig::daily();

    TEST_ASSERT(config.rotationMode == LogRotationMode::ByDate, "Daily rotation mode");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_file_appender_config_by_size() {
    std::cout << "Running: test_file_appender_config_by_size..." << std::endl;

    auto config = FileAppenderConfig::bySize(1024);

    TEST_ASSERT(config.rotationMode == LogRotationMode::BySize, "Size rotation mode");
    TEST_ASSERT(config.maxFileSize == 1024, "Custom max file size");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_file_appender_config_single() {
    std::cout << "Running: test_file_appender_config_single..." << std::endl;

    auto config = FileAppenderConfig::single();

    TEST_ASSERT(config.rotationMode == LogRotationMode::None, "No rotation");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// LogRotationMode Tests
//==============================================================================

bool test_log_rotation_mode_values() {
    std::cout << "Running: test_log_rotation_mode_values..." << std::endl;

    TEST_ASSERT(static_cast<int>(LogRotationMode::None) == 0, "None value");
    TEST_ASSERT(static_cast<int>(LogRotationMode::BySize) == 1, "BySize value");
    TEST_ASSERT(static_cast<int>(LogRotationMode::ByDate) == 2, "ByDate value");
    TEST_ASSERT(static_cast<int>(LogRotationMode::ByBoth) == 3, "ByBoth value");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// LogDateDivide Tests
//==============================================================================

bool test_log_date_divide_values() {
    std::cout << "Running: test_log_date_divide_values..." << std::endl;

    TEST_ASSERT(static_cast<int>(LogDateDivide::Daily) == 0, "Daily value");
    TEST_ASSERT(static_cast<int>(LogDateDivide::Monthly) == 1, "Monthly value");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// ConsoleColor Tests
//==============================================================================

bool test_console_color_values() {
    std::cout << "Running: test_console_color_values..." << std::endl;

    TEST_ASSERT(static_cast<int>(ConsoleColor::Default) == 0, "Default value");
    TEST_ASSERT(static_cast<int>(ConsoleColor::Red) == 2, "Red value");
    TEST_ASSERT(static_cast<int>(ConsoleColor::Green) == 3, "Green value");
    TEST_ASSERT(static_cast<int>(ConsoleColor::Yellow) == 4, "Yellow value");
    TEST_ASSERT(static_cast<int>(ConsoleColor::Blue) == 5, "Blue value");

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

//==============================================================================
// Main Test Runner
//==============================================================================

int main() {
    std::cout << "=== Apollo Core Log Module Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    // LogLevel tests
    run(test_log_level_values);
    run(test_log_level_to_string);
    run(test_log_level_from_string);
    run(test_log_level_bitwise_or);
    run(test_log_level_bitwise_and);
    run(test_has_level);

    // LogRecord tests
    run(test_log_record_default_constructor);
    run(test_log_record_constructor);
    run(test_log_record_setters);
    run(test_log_record_format_default);
    run(test_log_record_format_custom);
    run(test_log_record_to_short_string);
    run(test_log_record_to_full_string);
    run(test_log_record_timestamp);

    // IAppender tests
    run(test_appender_level_filter);
    run(test_appender_pattern);
    run(test_appender_flush);

    // ConsoleAppender tests
    run(test_console_appender_construction);
    run(test_console_appender_append);
    run(test_console_appender_set_config);

    // FileAppender tests
    run(test_file_appender_construction);
    run(test_file_appender_write);
    run(test_file_appender_rotation_by_size);
    run(test_file_appender_get_current_file_path);

    // LogManager tests
    run(test_log_manager_singleton);
    run(test_log_manager_initialize_default);
    run(test_log_manager_create_logger);
    run(test_log_manager_get_logger);
    run(test_log_manager_has_logger);
    run(test_log_manager_remove_logger);
    run(test_log_manager_get_logger_names);
    run(test_log_manager_default_level);

    // LogManagerConfig tests
    run(test_log_manager_config_default);
    run(test_log_manager_config_console_only);
    run(test_log_manager_config_file_only);
    run(test_log_manager_config_combined);

    // FileAppenderConfig tests
    run(test_file_appender_config_defaults);
    run(test_file_appender_config_daily);
    run(test_file_appender_config_by_size);
    run(test_file_appender_config_single);

    // LogRotationMode tests
    run(test_log_rotation_mode_values);

    // LogDateDivide tests
    run(test_log_date_divide_values);

    // ConsoleColor tests
    run(test_console_color_values);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return (total == passed) ? 0 : 1;
}
