/**
 * @file console_appender.cpp
 * @brief 控制台日志追加器实现
 */

#include "apollo/core/log/console_appender.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#endif

namespace apollo {
namespace core {
namespace log {

//==============================================================================
// ConsoleAppender 实现
//==============================================================================

ConsoleAppender::ConsoleAppender(const ConsoleAppenderConfig& config)
    : config_(config)
{
#ifdef _WIN32
    // Windows: 启用虚拟终端处理（支持 ANSI 颜色代码）
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }

    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hErr, &mode)) {
            SetConsoleMode(hErr, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif
}

void ConsoleAppender::append(const LogRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查级别
    if (!isEnabled(record.getLevel())) {
        return;
    }

    auto& stream = getStream();

    // 设置颜色
    if (config_.useColor) {
        setColor(getColorForLevel(record.getLevel()));
    }

    // 格式化并输出
    stream << format(record) << std::endl;

    // 重置颜色
    if (config_.useColor) {
        resetColor();
    }

    if (config_.flushOnWrite) {
        stream.flush();
    }
}

void ConsoleAppender::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout.flush();
    std::cerr.flush();
}

void ConsoleAppender::setColor(ConsoleColor color) {
#ifdef _WIN32
    // Windows 使用 ANSI 转义序列（需要 Win10+ 或已启用虚拟终端）
    static const char* ansiColors[] = {
        "\033[0m",      // Default
        "\033[30m",     // Black
        "\033[31m",     // Red
        "\033[32m",     // Green
        "\033[33m",     // Yellow
        "\033[34m",     // Blue
        "\033[35m",     // Magenta
        "\033[36m",     // Cyan
        "\033[37m",     // White
    };
    auto& stream = getStream();
    stream << ansiColors[static_cast<int>(color)];
#else
    // Linux/Mac 使用 ANSI 转义序列
    static const char* ansiColors[] = {
        "\033[0m",      // Default
        "\033[30m",     // Black
        "\033[31m",     // Red
        "\033[32m",     // Green
        "\033[33m",     // Yellow
        "\033[34m",     // Blue
        "\033[35m",     // Magenta
        "\033[36m",     // Cyan
        "\033[37m",     // White
    };
    auto& stream = getStream();
    stream << ansiColors[static_cast<int>(color)];
#endif
}

void ConsoleAppender::resetColor() {
    auto& stream = getStream();
    stream << "\033[0m";
}

ConsoleColor ConsoleAppender::getColorForLevel(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug:    return config_.debugColor;
        case LogLevel::Info:     return config_.infoColor;
        case LogLevel::Warning:  return config_.warningColor;
        case LogLevel::Error:    return config_.errorColor;
        case LogLevel::Critical: return config_.criticalColor;
        default:                 return ConsoleColor::Default;
    }
}

std::ostream& ConsoleAppender::getStream() {
    return config_.useStderr ? std::cerr : std::cout;
}

} // namespace log
} // namespace core
} // namespace apollo
