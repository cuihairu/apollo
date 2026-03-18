/**
 * @file test_time.cpp
 * @brief Time utilities unit tests
 */

#include "apollo/base/time.hpp"
#include <chrono>
#include <iostream>
#include <thread>

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
// Time Class Tests
//==============================================================================

bool test_time_now() {
    std::cout << "Running: test_time_now..." << std::endl;

    Time::Timestamp t1 = Time::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    Time::Timestamp t2 = Time::now();

    TEST_ASSERT(t2 > t1, "Time is monotonic");
    TEST_ASSERT(t2 - t1 >= 10, "At least 10ms elapsed");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_time_now_microseconds() {
    std::cout << "Running: test_time_now_microseconds..." << std::endl;

    int64_t t1 = Time::now_microseconds();
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    int64_t t2 = Time::now_microseconds();

    TEST_ASSERT(t2 > t1, "Microsecond time is monotonic");
    TEST_ASSERT(t2 - t1 >= 100, "At least 100us elapsed");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_time_now_nanoseconds() {
    std::cout << "Running: test_time_now_nanoseconds..." << std::endl;

    int64_t t1 = Time::now_nanoseconds();
    std::this_thread::sleep_for(std::chrono::nanoseconds(1000));
    int64_t t2 = Time::now_nanoseconds();

    TEST_ASSERT(t2 > t1, "Nanosecond time is monotonic");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_unix_time() {
    std::cout << "Running: test_unix_time..." << std::endl;

    time_t t = Time::unix_time();
    TEST_ASSERT(t > 0, "Unix time positive");

    // Compare with standard library
    time_t sys_time = std::time(nullptr);
    TEST_ASSERT(std::abs(t - sys_time) < 2, "Close to std::time");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_time_format() {
    std::cout << "Running: test_time_format..." << std::endl;

    time_t t = 1234567890;  // 2009-02-13 23:31:30 UTC
    std::string formatted = Time::format(t, "%Y-%m-%d");

    // Just check it's not empty and has expected parts
    TEST_ASSERT(!formatted.empty(), "Format not empty");
    TEST_ASSERT(formatted.find('-') != std::string::npos, "Contains separators");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_time_format_now() {
    std::cout << "Running: test_time_format_now..." << std::endl;

    std::string formatted = Time::format_now();
    TEST_ASSERT(!formatted.empty(), "Format now not empty");
    TEST_ASSERT(formatted.find(' ') != std::string::npos || formatted.find('-') != std::string::npos,
                "Contains date/time separators");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_time_date_str() {
    std::cout << "Running: test_time_date_str..." << std::endl;

    std::string date = Time::date_str(Time::unix_time());
    TEST_ASSERT(!date.empty(), "Date string not empty");
    TEST_ASSERT(date.length() == 10, "Date format YYYY-MM-DD length"); // "YYYY-MM-DD"

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_time_time_str() {
    std::cout << "Running: test_time_time_str..." << std::endl;

    std::string time = Time::time_str(Time::unix_time());
    TEST_ASSERT(!time.empty(), "Time string not empty");
    TEST_ASSERT(time.find(':') != std::string::npos, "Contains time separators");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_time_datetime_str() {
    std::cout << "Running: test_time_datetime_str..." << std::endl;

    std::string datetime = Time::datetime_str(Time::unix_time());
    TEST_ASSERT(!datetime.empty(), "DateTime string not empty");
    TEST_ASSERT(datetime.find('_') != std::string::npos, "Contains underscore separator");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_time_to_string() {
    std::cout << "Running: test_time_to_string..." << std::endl;

    Time::Timestamp ms = 1234567890123;
    std::string str = Time::to_string(ms);

    TEST_ASSERT(!str.empty(), "To string not empty");
    TEST_ASSERT(str.find('.') != std::string::npos, "Contains decimal point");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Timer Class Tests
//==============================================================================

bool test_timer_elapsed() {
    std::cout << "Running: test_timer_elapsed..." << std::endl;

    Timer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    double elapsed = timer.elapsed();
    TEST_ASSERT(elapsed >= 0.05, "At least 50ms elapsed");
    TEST_ASSERT(elapsed < 0.2, "Less than 200ms");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_timer_elapsed_millis() {
    std::cout << "Running: test_timer_elapsed_millis..." << std::endl;

    Timer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    double millis = timer.elapsed_millis();
    TEST_ASSERT(millis >= 50.0, "At least 50ms");
    TEST_ASSERT(millis < 200.0, "Less than 200ms");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_timer_elapsed_micros() {
    std::cout << "Running: test_timer_elapsed_micros..." << std::endl;

    Timer timer;
    std::this_thread::sleep_for(std::chrono::microseconds(1000));

    double micros = timer.elapsed_micros();
    TEST_ASSERT(micros >= 1000.0, "At least 1000us");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_timer_reset() {
    std::cout << "Running: test_timer_reset..." << std::endl;

    Timer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    double t1 = timer.elapsed_millis();

    timer.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    double t2 = timer.elapsed_millis();

    TEST_ASSERT(t2 < t1, "Time after reset is less");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// ScopeTimer Tests
//==============================================================================

bool test_scope_timer_output() {
    std::cout << "Running: test_scope_timer_output..." << std::endl;

    {
        ScopeTimer timer("TestScope", false);  // Don't output to avoid console spam
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }  // Destructor called here

    // If we reach here, destructor didn't crash
    TEST_ASSERT(true, "ScopeTimer destructor works");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_scope_timer_callback() {
    std::cout << "Running: test_scope_timer_callback..." << std::endl;

    bool callback_called = false;
    double captured_time = 0.0;

    {
        ScopeTimer timer([&](double millis) {
            callback_called = true;
            captured_time = millis;
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    TEST_ASSERT(callback_called, "Callback was called");
    TEST_ASSERT(captured_time >= 20.0, "Captured time is at least 20ms");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// FpsCalculator Tests
//==============================================================================

bool test_fps_calculator_initial() {
    std::cout << "Running: test_fps_calculator_initial..." << std::endl;

    FpsCalculator fps_calc;
    TEST_ASSERT(fps_calc.fps() == 0.0f, "Initial FPS is 0");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_fps_calculator_update() {
    std::cout << "Running: test_fps_calculator_update..." << std::endl;

    FpsCalculator fps_calc(50);  // Short update interval

    // Simulate some frames
    for (int i = 0; i < 10; ++i) {
        fps_calc.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // After updates, FPS should be calculated
    float fps = fps_calc.fps();
    TEST_ASSERT(fps > 0.0f, "FPS calculated after updates");

    std::cout << "  PASSED (FPS: " << fps << ")" << std::endl;
    return true;
}

bool test_fps_calculator_frame_time() {
    std::cout << "Running: test_fps_calculator_frame_time..." << std::endl;

    FpsCalculator fps_calc(50);

    for (int i = 0; i < 10; ++i) {
        fps_calc.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    float frame_time = fps_calc.frame_time_ms();
    TEST_ASSERT(frame_time > 0.0f, "Frame time positive");

    // Frame time should be roughly 1000 / FPS
    float expected = 1000.0f / fps_calc.fps();
    TEST_ASSERT(std::abs(frame_time - expected) < 50.0f, "Frame time matches FPS");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_fps_calculator_custom_interval() {
    std::cout << "Running: test_fps_calculator_custom_interval..." << std::endl;

    FpsCalculator fps_calc(200);  // 200ms interval

    // Update with simulated 60 FPS (16.67ms per frame)
    for (int i = 0; i < 20; ++i) {
        fps_calc.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    float fps = fps_calc.fps();
    // FPS calculation can vary due to sleep precision
    TEST_ASSERT(fps > 20.0f && fps < 100.0f, "FPS in reasonable range");

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

//==============================================================================
// Main Test Runner
//==============================================================================

int main() {
    std::cout << "=== Apollo Time Utilities Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    // Time class tests
    run(test_time_now);
    run(test_time_now_microseconds);
    run(test_time_now_nanoseconds);
    run(test_unix_time);
    run(test_time_format);
    run(test_time_format_now);
    run(test_time_date_str);
    run(test_time_time_str);
    run(test_time_datetime_str);
    run(test_time_to_string);

    // Timer class tests
    run(test_timer_elapsed);
    run(test_timer_elapsed_millis);
    run(test_timer_elapsed_micros);
    run(test_timer_reset);

    // ScopeTimer tests
    run(test_scope_timer_output);
    run(test_scope_timer_callback);

    // FpsCalculator tests
    run(test_fps_calculator_initial);
    run(test_fps_calculator_update);
    run(test_fps_calculator_frame_time);
    run(test_fps_calculator_custom_interval);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return (total == passed) ? 0 : 1;
}
