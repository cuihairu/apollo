/**
 * @file test_terminal.cpp
 * @brief Terminal utilities unit tests
 */

#include "apollo/base/terminal.hpp"
#include <iostream>
#include <string>

using namespace apollo::base;

namespace {

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

//==============================================================================
// Color Code Tests
//==============================================================================

bool test_color_codes() {
    std::cout << "Running: test_color_codes..." << std::endl;

    using namespace color;

    // Check that color codes return non-null pointers
    TEST_ASSERT(reset() != nullptr, "Reset code");
    TEST_ASSERT(bold() != nullptr, "Bold code");
    TEST_ASSERT(red() != nullptr, "Red code");
    TEST_ASSERT(green() != nullptr, "Green code");
    TEST_ASSERT(blue() != nullptr, "Blue code");

    // Check codes start with escape sequence
    TEST_ASSERT(reset()[0] == '\033', "Reset is ANSI code");
    TEST_ASSERT(red()[0] == '\033', "Red is ANSI code");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_background_colors() {
    std::cout << "Running: test_background_colors..." << std::endl;

    using namespace color;

    TEST_ASSERT(bg_black() != nullptr, "Bg black code");
    TEST_ASSERT(bg_red() != nullptr, "Bg red code");
    TEST_ASSERT(bg_green() != nullptr, "Bg green code");

    // Background codes should be different from foreground
    TEST_ASSERT(bg_red() != red(), "Bg red differs from fg red");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_bright_colors() {
    std::cout << "Running: test_bright_colors..." << std::endl;

    using namespace color;

    TEST_ASSERT(bright_red() != nullptr, "Bright red code");
    TEST_ASSERT(bright_green() != nullptr, "Bright green code");
    TEST_ASSERT(bright_blue() != nullptr, "Bright blue code");

    // Bright colors should differ from normal colors
    TEST_ASSERT(bright_red() != red(), "Bright red differs from red");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Terminal Detection Tests
//==============================================================================

bool test_is_tty() {
    std::cout << "Running: test_is_tty..." << std::endl;

    // Just check that the function runs and returns a bool
    bool is_tty_result = Terminal::is_tty();
    (void)is_tty_result; // Suppress unused warning

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_supports_color() {
    std::cout << "Running: test_supports_color..." << std::endl;

    bool supports = Terminal::supports_color();
    (void)supports; // May be true or false depending on environment

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_enable_color() {
    std::cout << "Running: test_enable_color..." << std::endl;

    // Should always return true or at least not crash
    Terminal::enable_color();

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Terminal Size Tests
//==============================================================================

bool test_get_size() {
    std::cout << "Running: test_get_size..." << std::endl;

    Terminal::Size size = Terminal::get_size();

    // Size should be non-negative (may be 0 if not a terminal)
    TEST_ASSERT(size.rows >= 0, "Rows non-negative");
    TEST_ASSERT(size.cols >= 0, "Cols non-negative");

    std::cout << "  PASSED (size: " << size.rows << "x" << size.cols << ")" << std::endl;
    return true;
}

//==============================================================================
// Terminal Control Tests
//==============================================================================

bool test_clear() {
    std::cout << "Running: test_clear..." << std::endl;

    // Just check it doesn't crash
    Terminal::clear();

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_clear_line() {
    std::cout << "Running: test_clear_line..." << std::endl;

    Terminal::clear_line();

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_move_cursor() {
    std::cout << "Running: test_move_cursor..." << std::endl;

    Terminal::move_cursor(1, 1);
    Terminal::cursor_up(1);
    Terminal::cursor_down(1);
    Terminal::cursor_left(1);
    Terminal::cursor_right(1);

    // Multiple steps
    Terminal::cursor_up(5);
    Terminal::cursor_down(3);

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_cursor_save_restore() {
    std::cout << "Running: test_cursor_save_restore..." << std::endl;

    Terminal::save_cursor();
    Terminal::restore_cursor();

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_cursor_visibility() {
    std::cout << "Running: test_cursor_visibility..." << std::endl;

    Terminal::hide_cursor();
    Terminal::show_cursor();

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Color Formatting Tests
//==============================================================================

bool test_rgb() {
    std::cout << "Running: test_rgb..." << std::endl;

    std::string c1 = Terminal::rgb(255, 0, 0);
    std::string c2 = Terminal::rgb(0, 255, 0);
    std::string c3 = Terminal::rgb(0, 0, 255);

    // Should return something (may be empty if no color support)
    // Just verify no crash

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_bg_rgb() {
    std::cout << "Running: test_bg_rgb..." << std::endl;

    std::string c = Terminal::bg_rgb(255, 128, 64);

    // Just verify no crash

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_colorize() {
    std::cout << "Running: test_colorize..." << std::endl;

    std::string colored = Terminal::colorize("test", color::red());

    // Result should contain the text
    TEST_ASSERT(colored.find("test") != std::string::npos, "Contains text");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_convenience_colors() {
    std::cout << "Running: test_convenience_colors..." << std::endl;

    std::string r = Terminal::red("text");
    std::string g = Terminal::green("text");
    std::string y = Terminal::yellow("text");
    std::string b = Terminal::blue("text");
    std::string m = Terminal::magenta("text");
    std::string c = Terminal::cyan("text");
    std::string bold = Terminal::bold("text");

    // All should contain the text
    TEST_ASSERT(r.find("text") != std::string::npos, "Red contains text");
    TEST_ASSERT(g.find("text") != std::string::npos, "Green contains text");
    TEST_ASSERT(y.find("text") != std::string::npos, "Yellow contains text");
    TEST_ASSERT(b.find("text") != std::string::npos, "Blue contains text");
    TEST_ASSERT(m.find("text") != std::string::npos, "Magenta contains text");
    TEST_ASSERT(c.find("text") != std::string::npos, "Cyan contains text");
    TEST_ASSERT(bold.find("text") != std::string::npos, "Bold contains text");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Progress Bar Tests
//==============================================================================

bool test_progress_bar() {
    std::cout << "Running: test_progress_bar..." << std::endl;

    std::string bar0 = Terminal::progress_bar(0.0);
    TEST_ASSERT(bar0.find("[") == 0, "Bar starts with bracket");

    std::string bar50 = Terminal::progress_bar(0.5);
    TEST_ASSERT(bar50.find("[") == 0, "50% bar format");

    std::string bar100 = Terminal::progress_bar(1.0);
    TEST_ASSERT(bar100.find("[") == 0, "100% bar format");

    // Clamping
    std::string bar_neg = Terminal::progress_bar(-0.5);
    std::string bar_over = Terminal::progress_bar(1.5);

    // Custom width
    std::string bar20 = Terminal::progress_bar(0.5, 20);

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Spinner Tests
//==============================================================================

bool test_spinner_char() {
    std::cout << "Running: test_spinner_char..." << std::endl;

    const char* s0 = Terminal::spinner_char(0);
    const char* s1 = Terminal::spinner_char(1);
    const char* s2 = Terminal::spinner_char(2);
    const char* s3 = Terminal::spinner_char(3);
    const char* s4 = Terminal::spinner_char(4); // Should wrap to 0

    TEST_ASSERT(s0 != nullptr, "Spinner 0");
    TEST_ASSERT(s1 != nullptr, "Spinner 1");
    TEST_ASSERT(s2 != nullptr, "Spinner 2");
    TEST_ASSERT(s3 != nullptr, "Spinner 3");
    TEST_ASSERT(s4 == s0, "Spinner wraps around");

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

//==============================================================================
// Main Test Runner
//==============================================================================

int main() {
    std::cout << "=== Apollo Terminal Utilities Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    // Color code tests
    run(test_color_codes);
    run(test_background_colors);
    run(test_bright_colors);

    // Terminal detection tests
    run(test_is_tty);
    run(test_supports_color);
    run(test_enable_color);

    // Terminal size tests
    run(test_get_size);

    // Terminal control tests
    run(test_clear);
    run(test_clear_line);
    run(test_move_cursor);
    run(test_cursor_save_restore);
    run(test_cursor_visibility);

    // Color formatting tests
    run(test_rgb);
    run(test_bg_rgb);
    run(test_colorize);
    run(test_convenience_colors);

    // Progress bar tests
    run(test_progress_bar);

    // Spinner tests
    run(test_spinner_char);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return (total == passed) ? 0 : 1;
}
