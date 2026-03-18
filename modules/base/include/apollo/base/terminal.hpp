#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#endif

namespace apollo::base {

// Terminal colors (ANSI escape codes)
namespace color {
    constexpr const char* reset() { return "\033[0m"; }
    constexpr const char* bold() { return "\033[1m"; }
    constexpr const char* dim() { return "\033[2m"; }
    constexpr const char* italic() { return "\033[3m"; }
    constexpr const char* underline() { return "\033[4m"; }

    // Foreground colors
    constexpr const char* black() { return "\033[30m"; }
    constexpr const char* red() { return "\033[31m"; }
    constexpr const char* green() { return "\033[32m"; }
    constexpr const char* yellow() { return "\033[33m"; }
    constexpr const char* blue() { return "\033[34m"; }
    constexpr const char* magenta() { return "\033[35m"; }
    constexpr const char* cyan() { return "\033[36m"; }
    constexpr const char* white() { return "\033[37m"; }

    // Bright foreground colors
    constexpr const char* bright_black() { return "\033[90m"; }
    constexpr const char* bright_red() { return "\033[91m"; }
    constexpr const char* bright_green() { return "\033[92m"; }
    constexpr const char* bright_yellow() { return "\033[93m"; }
    constexpr const char* bright_blue() { return "\033[94m"; }
    constexpr const char* bright_magenta() { return "\033[95m"; }
    constexpr const char* bright_cyan() { return "\033[96m"; }
    constexpr const char* bright_white() { return "\033[97m"; }

    // Background colors
    constexpr const char* bg_black() { return "\033[40m"; }
    constexpr const char* bg_red() { return "\033[41m"; }
    constexpr const char* bg_green() { return "\033[42m"; }
    constexpr const char* bg_yellow() { return "\033[43m"; }
    constexpr const char* bg_blue() { return "\033[44m"; }
    constexpr const char* bg_magenta() { return "\033[45m"; }
    constexpr const char* bg_cyan() { return "\033[46m"; }
    constexpr const char* bg_white() { return "\033[47m"; }
}

// Terminal utilities
class Terminal {
public:
    struct Size {
        int rows = 0;
        int cols = 0;
    };

    // Check if stdout is a terminal
    static bool is_tty() {
#ifdef _WIN32
        return _isatty(_fileno(stdout));
#else
        return isatty(STDOUT_FILENO);
#endif
    }

    // Check if terminal supports ANSI colors
    static bool supports_color() {
#ifdef _WIN32
        // Windows 10+ supports ANSI colors
        DWORD mode = 0;
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h == INVALID_HANDLE_VALUE) {
            return false;
        }
        return GetConsoleMode(h, &mode) && (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
        // On Unix-like systems, check TERM and COLORTERM
        const char* term = std::getenv("TERM");
        if (!term) {
            return false;
        }
        const char* colorterm = std::getenv("COLORTERM");
        return colorterm != nullptr;
#endif
    }

    // Enable ANSI color support on Windows
    static bool enable_color() {
#ifdef _WIN32
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h == INVALID_HANDLE_VALUE) {
            return false;
        }
        DWORD mode = 0;
        if (!GetConsoleMode(h, &mode)) {
            return false;
        }
        return SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
        return true;
#endif
    }

    // Get terminal size
    static Size get_size() {
        Size size;
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (GetConsoleScreenBufferInfo(h, &csbi)) {
            size.cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            size.rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        }
#else
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
            size.rows = w.ws_row;
            size.cols = w.ws_col;
        }
#endif
        return size;
    }

    // Clear terminal
    static void clear() {
        if (supports_color()) {
            printf("\033[2J\033[H");
        }
    }

    // Clear line
    static void clear_line() {
        if (supports_color()) {
            printf("\033[2K");
        }
    }

    // Move cursor
    static void move_cursor(int row, int col) {
        if (supports_color()) {
            printf("\033[%d;%dH", row, col);
        }
    }

    // Move cursor up
    static void cursor_up(int n = 1) {
        if (supports_color()) {
            printf("\033[%dA", n);
        }
    }

    // Move cursor down
    static void cursor_down(int n = 1) {
        if (supports_color()) {
            printf("\033[%dB", n);
        }
    }

    // Move cursor left
    static void cursor_left(int n = 1) {
        if (supports_color()) {
            printf("\033[%dD", n);
        }
    }

    // Move cursor right
    static void cursor_right(int n = 1) {
        if (supports_color()) {
            printf("\033[%dC", n);
        }
    }

    // Save cursor position
    static void save_cursor() {
        if (supports_color()) {
            printf("\033[s");
        }
    }

    // Restore cursor position
    static void restore_cursor() {
        if (supports_color()) {
            printf("\033[u");
        }
    }

    // Hide cursor
    static void hide_cursor() {
        if (supports_color()) {
            printf("\033[?25l");
        }
    }

    // Show cursor
    static void show_cursor() {
        if (supports_color()) {
            printf("\033[?25h");
        }
    }

    // Set foreground color (RGB)
    static std::string rgb(uint8_t r, uint8_t g, uint8_t b) {
        if (supports_color()) {
            char buffer[32];
#ifdef _WIN32
            sprintf_s(buffer, sizeof(buffer), "\033[38;2;%d;%d;%dm", r, g, b);
#else
            snprintf(buffer, sizeof(buffer), "\033[38;2;%d;%d;%dm", r, g, b);
#endif
            return std::string(buffer);
        }
        return "";
    }

    // Set background color (RGB)
    static std::string bg_rgb(uint8_t r, uint8_t g, uint8_t b) {
        if (supports_color()) {
            char buffer[32];
#ifdef _WIN32
            sprintf_s(buffer, sizeof(buffer), "\033[48;2;%d;%d;%dm", r, g, b);
#else
            snprintf(buffer, sizeof(buffer), "\033[48;2;%d;%d;%dm", r, g, b);
#endif
            return std::string(buffer);
        }
        return "";
    }

    // Format string with color
    static std::string colorize(std::string_view text, std::string_view color_code) {
        if (supports_color()) {
            std::string result;
            result.reserve(color_code.size() + text.size() + 5);
            result.append(color_code);
            result.append(text);
            result.append("\033[0m");
            return result;
        }
        return std::string(text);
    }

    // Convenience colorization methods
    static std::string red(std::string_view text) {
        return colorize(text, color::red());
    }

    static std::string green(std::string_view text) {
        return colorize(text, color::green());
    }

    static std::string yellow(std::string_view text) {
        return colorize(text, color::yellow());
    }

    static std::string blue(std::string_view text) {
        return colorize(text, color::blue());
    }

    static std::string magenta(std::string_view text) {
        return colorize(text, color::magenta());
    }

    static std::string cyan(std::string_view text) {
        return colorize(text, color::cyan());
    }

    static std::string bold(std::string_view text) {
        return colorize(text, color::bold());
    }

    // Progress bar
    static std::string progress_bar(double percent, int width = 40) {
        percent = percent < 0.0 ? 0.0 : (percent > 1.0 ? 1.0 : percent);
        const int filled = static_cast<int>(percent * width);
        const int empty = width - filled;

        std::string bar;
        bar.reserve(width + 4);
        bar.append("[");
        bar.append(filled, '=');
        bar.append(1, '>');
        bar.append(empty, ' ');
        bar.append("]");
        return bar;
    }

    // Spinner characters for loading indicators
    static const char* spinner_char(int index) {
        static const char* spinners[] = {"-", "\\", "|", "/"};
        return spinners[index % 4];
    }
};

} // namespace apollo::base
