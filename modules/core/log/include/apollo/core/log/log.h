#pragma once

/**
 * @file log.h
 * @brief Apollo 日志系统统一头文件
 *
 * 特性：
 * - 多级别日志（Debug, Info, Warning, Error, Critical）
 * - 多种追加器（控制台、文件）
 * - 日志文件分割（按大小、按日期）
 * - 线程安全
 * - 便捷的日志宏
 *
 * 基本用法：
 * @code
 * #include "apollo/core/log/log.h"
 *
 * int main() {
 *     // 初始化日志系统
 *     apollo::core::log::LogManagerConfig config;
 *     config.consoleEnabled = true;
 *     config.fileEnabled = true;
 *     config.fileConfig.baseName = "logs/app";
 *     config.fileConfig.rotationMode = apollo::core::log::LogRotationMode::ByBoth;
 *     apollo::core::log::LogManager::instance().initialize(config);
 *
 *     // 使用宏记录日志
 *     APOLLO_LOG_INFO("Application started");
 *     APOLLO_LOG_ERROR_F("Error code: %d", errorCode);
 *
 *     // 或使用日志器
 *     auto logger = apollo::core::log::LogManager::instance().getDefaultLogger();
 *     logger->info("Direct log message");
 *
 *     return 0;
 * }
 * @endcode
 */

#include "apollo/core/log/log_level.h"
#include "apollo/core/log/log_record.h"
#include "apollo/core/log/appender.h"
#include "apollo/core/log/logger.h"
#include "apollo/core/log/logger.hpp"
#include "apollo/core/log/console_appender.h"
#include "apollo/core/log/file_appender.h"
#include "apollo/core/log/log_manager.h"
