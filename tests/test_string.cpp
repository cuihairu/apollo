/**
 * @file test_string.cpp
 * @brief String utilities unit tests
 */

#include "apollo/base/string.hpp"
#include <iostream>

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
// Trim Tests
//==============================================================================

bool test_trim() {
    std::cout << "Running: test_trim..." << std::endl;

    TEST_ASSERT(String::trim("  hello  ") == "hello", "Basic trim");
    TEST_ASSERT(String::trim("\t\n  hello  \n\t") == "hello", "Trim with tabs and newlines");
    TEST_ASSERT(String::trim("hello") == "hello", "No trim needed");
    TEST_ASSERT(String::trim("   ") == "", "Only spaces");
    TEST_ASSERT(String::trim("") == "", "Empty string");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_ltrim() {
    std::cout << "Running: test_ltrim..." << std::endl;

    TEST_ASSERT(String::ltrim("  hello  ") == "hello  ", "Left trim");
    TEST_ASSERT(String::ltrim("\t  hello") == "hello", "Left trim only");
    TEST_ASSERT(String::ltrim("hello  ") == "hello  ", "No left trim");
    TEST_ASSERT(String::ltrim("   ") == "", "Only spaces");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_rtrim() {
    std::cout << "Running: test_rtrim..." << std::endl;

    TEST_ASSERT(String::rtrim("  hello  ") == "  hello", "Right trim");
    TEST_ASSERT(String::rtrim("hello  ") == "hello", "Right trim only");
    TEST_ASSERT(String::rtrim("  hello") == "  hello", "No right trim");
    TEST_ASSERT(String::rtrim("   ") == "", "Only spaces");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_trim_in_place() {
    std::cout << "Running: test_trim_in_place..." << std::endl;

    std::string s = "  hello  ";
    String::trim_in_place(s);
    TEST_ASSERT(s == "hello", "Trim in place");

    s = "  world  ";
    String::ltrim_in_place(s);
    TEST_ASSERT(s == "world  ", "Left trim in place");

    s = "  world  ";
    String::rtrim_in_place(s);
    TEST_ASSERT(s == "  world", "Right trim in place");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Case Conversion Tests
//==============================================================================

bool test_case_conversion() {
    std::cout << "Running: test_case_conversion..." << std::endl;

    TEST_ASSERT(String::to_lower("HELLO") == "hello", "To lower");
    TEST_ASSERT(String::to_lower("HeLLo") == "hello", "Mixed case to lower");
    TEST_ASSERT(String::to_upper("hello") == "HELLO", "To upper");
    TEST_ASSERT(String::to_upper("HeLLo") == "HELLO", "Mixed case to upper");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Comparison Tests
//==============================================================================

bool test_iequals() {
    std::cout << "Running: test_iequals..." << std::endl;

    TEST_ASSERT(String::iequals("hello", "HELLO"), "Case insensitive equal");
    TEST_ASSERT(String::iequals("HeLLo", "hElLo"), "Mixed case equal");
    TEST_ASSERT(!String::iequals("hello", "world"), "Different strings");
    TEST_ASSERT(!String::iequals("hello", "helloo"), "Different length");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_starts_with() {
    std::cout << "Running: test_starts_with..." << std::endl;

    TEST_ASSERT(String::starts_with("hello world", "hello"), "Starts with");
    TEST_ASSERT(!String::starts_with("hello world", "world"), "Does not start with");
    TEST_ASSERT(String::starts_with("hello", "hello"), "Equal strings");
    TEST_ASSERT(!String::starts_with("hi", "hello"), "Prefix too long");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_ends_with() {
    std::cout << "Running: test_ends_with..." << std::endl;

    TEST_ASSERT(String::ends_with("hello world", "world"), "Ends with");
    TEST_ASSERT(!String::ends_with("hello world", "hello"), "Does not end with");
    TEST_ASSERT(String::ends_with("hello", "hello"), "Equal strings");
    TEST_ASSERT(!String::ends_with("hi", "hello"), "Suffix too long");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_istarts_with() {
    std::cout << "Running: test_istarts_with..." << std::endl;

    TEST_ASSERT(String::istarts_with("Hello World", "HELLO"), "Case insensitive starts with");
    TEST_ASSERT(!String::istarts_with("Hello World", "WORLD"), "Does not start with");
    TEST_ASSERT(String::istarts_with("hello", "HELLO"), "Equal strings");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_iends_with() {
    std::cout << "Running: test_iends_with..." << std::endl;

    TEST_ASSERT(String::iends_with("Hello World", "WORLD"), "Case insensitive ends with");
    TEST_ASSERT(!String::iends_with("Hello World", "HELLO"), "Does not end with");
    TEST_ASSERT(String::iends_with("hello", "HELLO"), "Equal strings");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Split and Join Tests
//==============================================================================

bool test_split_char() {
    std::cout << "Running: test_split_char..." << std::endl;

    auto result = String::split("a,b,c", ',');
    TEST_ASSERT(result.size() == 3, "Split count");
    TEST_ASSERT(result[0] == "a", "First element");
    TEST_ASSERT(result[1] == "b", "Second element");
    TEST_ASSERT(result[2] == "c", "Third element");

    result = String::split("a,b,c,", ',');
    TEST_ASSERT(result.size() == 4, "Split with trailing delimiter");
    TEST_ASSERT(result[3] == "", "Empty last element");

    result = String::split(",,,", ',');
    TEST_ASSERT(result.size() == 4, "Split only delimiters");

    result = String::split("single", ',');
    TEST_ASSERT(result.size() == 1, "No delimiter");
    TEST_ASSERT(result[0] == "single", "Single element");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_split_string() {
    std::cout << "Running: test_split_string..." << std::endl;

    auto result = String::split("a::b::c", "::");
    TEST_ASSERT(result.size() == 3, "Split count");
    TEST_ASSERT(result[0] == "a", "First element");
    TEST_ASSERT(result[1] == "b", "Second element");
    TEST_ASSERT(result[2] == "c", "Third element");

    result = String::split("abc", "::");
    TEST_ASSERT(result.size() == 1, "No delimiter match");
    TEST_ASSERT(result[0] == "abc", "Single element");

    // Empty delimiter returns original
    result = String::split("a,b,c", "");
    TEST_ASSERT(result.size() == 1, "Empty delimiter");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_split_to_out() {
    std::cout << "Running: test_split_to_out..." << std::endl;

    std::vector<std::string> out;
    String::split("a,b,c", ',', out);
    TEST_ASSERT(out.size() == 3, "Split to out");

    String::split("x,y", ',', out);
    TEST_ASSERT(out.size() == 2, "Split to out (clears previous)");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_join() {
    std::cout << "Running: test_join..." << std::endl;

    std::vector<std::string> strs = {"a", "b", "c"};
    TEST_ASSERT(String::join(strs, ",") == "a,b,c", "Join strings");
    TEST_ASSERT(String::join(strs, "::") == "a::b::c", "Join with custom delimiter");

    std::vector<std::string> empty;
    TEST_ASSERT(String::join(empty, ",") == "", "Join empty");

    std::vector<std::string> single = {"one"};
    TEST_ASSERT(String::join(single, ",") == "one", "Join single");

    // String_view version
    std::vector<std::string_view> svs = {"x", "y", "z"};
    TEST_ASSERT(String::join(svs, "-") == "x-y-z", "Join string_views");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Replace Tests
//==============================================================================

bool test_replace() {
    std::cout << "Running: test_replace..." << std::endl;

    TEST_ASSERT(String::replace("hello world", "world", "there") == "hello there", "Basic replace");
    TEST_ASSERT(String::replace("aaa", "a", "b") == "bbb", "Replace all");
    TEST_ASSERT(String::replace("hello", "x", "y") == "hello", "No match");

    // Empty from returns original
    TEST_ASSERT(String::replace("hello", "", "x") == "hello", "Empty from");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_replace_in_place() {
    std::cout << "Running: test_replace_in_place..." << std::endl;

    std::string s = "hello world";
    String::replace_in_place(s, "world", "there");
    TEST_ASSERT(s == "hello there", "Replace in place");

    s = "aaa";
    String::replace_in_place(s, "a", "b");
    TEST_ASSERT(s == "bbb", "Replace all in place");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Format Tests
//==============================================================================

bool test_format() {
    std::cout << "Running: test_format..." << std::endl;

    TEST_ASSERT(String::format("%s %d", "test", 42) == "test 42", "Format string and int");
    TEST_ASSERT(String::format("%d %d %d", 1, 2, 3) == "1 2 3", "Format multiple ints");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Empty and Blank Tests
//==============================================================================

bool test_is_empty() {
    std::cout << "Running: test_is_empty..." << std::endl;

    TEST_ASSERT(String::is_empty(""), "Empty string");
    TEST_ASSERT(!String::is_empty("a"), "Non-empty string");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_is_blank() {
    std::cout << "Running: test_is_blank..." << std::endl;

    TEST_ASSERT(String::is_blank(""), "Empty is blank");
    TEST_ASSERT(String::is_blank("   "), "Spaces is blank");
    TEST_ASSERT(String::is_blank("\t\n"), "Tabs and newlines is blank");
    TEST_ASSERT(!String::is_blank("a"), "Non-blank");
    TEST_ASSERT(!String::is_blank(" a "), "Contains non-whitespace");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Conversion Tests
//==============================================================================

bool test_to_int32() {
    std::cout << "Running: test_to_int32..." << std::endl;

    TEST_ASSERT(String::to_int32("123") == 123, "Parse int");
    TEST_ASSERT(String::to_int32("-456") == -456, "Parse negative int");
    TEST_ASSERT(String::to_int32("abc", 99) == 99, "Invalid returns default");
    TEST_ASSERT(String::to_int32("", 0) == 0, "Empty returns default");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_to_int64() {
    std::cout << "Running: test_to_int64..." << std::endl;

    TEST_ASSERT(String::to_int64("12345678901") == 12345678901LL, "Parse int64");
    TEST_ASSERT(String::to_int64("-9876543210") == -9876543210LL, "Parse negative int64");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_to_double() {
    std::cout << "Running: test_to_double..." << std::endl;

    TEST_ASSERT(String::to_double("3.14") == 3.14, "Parse double");
    TEST_ASSERT(String::to_double("-2.5") == -2.5, "Parse negative double");
    TEST_ASSERT(String::to_double("abc", 1.0) == 1.0, "Invalid returns default");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_to_bool() {
    std::cout << "Running: test_to_bool..." << std::endl;

    TEST_ASSERT(String::to_bool("true"), "Parse true");
    TEST_ASSERT(String::to_bool("TRUE"), "Parse TRUE");
    TEST_ASSERT(String::to_bool("1"), "Parse 1");
    TEST_ASSERT(String::to_bool("yes"), "Parse yes");
    TEST_ASSERT(String::to_bool("on"), "Parse on");

    TEST_ASSERT(!String::to_bool("false"), "Parse false");
    TEST_ASSERT(!String::to_bool("0"), "Parse 0");
    TEST_ASSERT(!String::to_bool("no"), "Parse no");
    TEST_ASSERT(!String::to_bool("off"), "Parse off");

    TEST_ASSERT(String::to_bool("invalid", true) == true, "Invalid returns default true");
    TEST_ASSERT(String::to_bool("invalid", false) == false, "Invalid returns default false");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Hash Tests
//==============================================================================

bool test_hash() {
    std::cout << "Running: test_hash..." << std::endl;

    size_t h1 = String::hash("hello");
    size_t h2 = String::hash("hello");
    TEST_ASSERT(h1 == h2, "Same string same hash");

    size_t h3 = String::hash("world");
    TEST_ASSERT(h1 != h3, "Different strings different hashes (likely)");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Format Bytes Tests
//==============================================================================

bool test_format_bytes() {
    std::cout << "Running: test_format_bytes..." << std::endl;

    auto s1 = String::format_bytes(0);
    TEST_ASSERT(s1.find("0.00") != std::string::npos && s1.find("B") != std::string::npos, "Zero bytes");

    auto s2 = String::format_bytes(1024);
    TEST_ASSERT(s2.find("KB") != std::string::npos, "Kilobytes");

    auto s3 = String::format_bytes(1024 * 1024);
    TEST_ASSERT(s3.find("MB") != std::string::npos, "Megabytes");

    auto s4 = String::format_bytes(1024 * 1024 * 1024);
    TEST_ASSERT(s4.find("GB") != std::string::npos, "Gigabytes");

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

//==============================================================================
// Main Test Runner
//==============================================================================

int main() {
    std::cout << "=== Apollo String Utilities Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    // Trim tests
    run(test_trim);
    run(test_ltrim);
    run(test_rtrim);
    run(test_trim_in_place);

    // Case conversion tests
    run(test_case_conversion);

    // Comparison tests
    run(test_iequals);
    run(test_starts_with);
    run(test_ends_with);
    run(test_istarts_with);
    run(test_iends_with);

    // Split and join tests
    run(test_split_char);
    run(test_split_string);
    run(test_split_to_out);
    run(test_join);

    // Replace tests
    run(test_replace);
    run(test_replace_in_place);

    // Format tests
    run(test_format);

    // Empty and blank tests
    run(test_is_empty);
    run(test_is_blank);

    // Conversion tests
    run(test_to_int32);
    run(test_to_int64);
    run(test_to_double);
    run(test_to_bool);

    // Hash tests
    run(test_hash);

    // Format bytes tests
    run(test_format_bytes);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return (total == passed) ? 0 : 1;
}
