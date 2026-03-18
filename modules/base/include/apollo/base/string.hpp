#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace apollo::base {

class String {
public:
    // Trim functions - use string_view to avoid ambiguity with const char*
    static std::string trim(std::string_view str) {
        std::string result(str);
        trim_in_place(result);
        return result;
    }

    static std::string ltrim(std::string_view str) {
        std::string result(str);
        ltrim_in_place(result);
        return result;
    }

    static std::string rtrim(std::string_view str) {
        std::string result(str);
        rtrim_in_place(result);
        return result;
    }

    static void ltrim_in_place(std::string& str) {
        str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
    }

    static void rtrim_in_place(std::string& str) {
        str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), str.end());
    }

    static void trim_in_place(std::string& str) {
        ltrim_in_place(str);
        rtrim_in_place(str);
    }

    // Case conversion
    static std::string to_lower(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return str;
    }

    static std::string to_upper(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(),
                       [](unsigned char c) { return std::toupper(c); });
        return str;
    }

    // String comparison (case-insensitive)
    static bool iequals(std::string_view a, std::string_view b) {
        if (a.size() != b.size()) {
            return false;
        }
        return std::equal(a.begin(), a.end(), b.begin(),
                          [](unsigned char ca, unsigned char cb) {
                              return std::tolower(ca) == std::tolower(cb);
                          });
    }

    static bool starts_with(std::string_view str, std::string_view prefix) {
        return str.size() >= prefix.size() &&
               str.compare(0, prefix.size(), prefix) == 0;
    }

    static bool ends_with(std::string_view str, std::string_view suffix) {
        return str.size() >= suffix.size() &&
               str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    static bool istarts_with(std::string_view str, std::string_view prefix) {
        if (str.size() < prefix.size()) {
            return false;
        }
        return iequals(str.substr(0, prefix.size()), prefix);
    }

    static bool iends_with(std::string_view str, std::string_view suffix) {
        if (str.size() < suffix.size()) {
            return false;
        }
        return iequals(str.substr(str.size() - suffix.size()), suffix);
    }

    // Split and join
    static std::vector<std::string> split(std::string_view str, char delimiter) {
        std::vector<std::string> result;
        split(str, delimiter, result);
        return result;
    }

    static std::vector<std::string> split(std::string_view str, std::string_view delimiter) {
        std::vector<std::string> result;
        split(str, delimiter, result);
        return result;
    }

    static void split(std::string_view str, char delimiter, std::vector<std::string>& out) {
        out.clear();
        std::string_view::size_type start = 0;
        std::string_view::size_type pos = 0;

        while ((pos = str.find(delimiter, start)) != std::string_view::npos) {
            out.emplace_back(str.substr(start, pos - start));
            start = pos + 1;
        }
        out.emplace_back(str.substr(start));
    }

    static void split(std::string_view str, std::string_view delimiter, std::vector<std::string>& out) {
        out.clear();
        if (delimiter.empty()) {
            out.emplace_back(str);
            return;
        }

        std::string_view::size_type start = 0;
        std::string_view::size_type pos = 0;
        const size_t delim_len = delimiter.size();

        while ((pos = str.find(delimiter, start)) != std::string_view::npos) {
            out.emplace_back(str.substr(start, pos - start));
            start = pos + delim_len;
        }
        out.emplace_back(str.substr(start));
    }

    static std::string join(const std::vector<std::string>& strs, std::string_view delimiter) {
        if (strs.empty()) {
            return "";
        }

        std::ostringstream oss;
        oss << strs[0];
        for (size_t i = 1; i < strs.size(); ++i) {
            oss << delimiter << strs[i];
        }
        return oss.str();
    }

    static std::string join(const std::vector<std::string_view>& strs, std::string_view delimiter) {
        if (strs.empty()) {
            return "";
        }

        std::ostringstream oss;
        oss << strs[0];
        for (size_t i = 1; i < strs.size(); ++i) {
            oss << delimiter << strs[i];
        }
        return oss.str();
    }

    // Replace
    static std::string replace(std::string str, std::string_view from, std::string_view to) {
        if (from.empty()) {
            return str;
        }

        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.size(), to);
            pos += to.size();
        }
        return str;
    }

    static void replace_in_place(std::string& str, std::string_view from, std::string_view to) {
        if (from.empty()) {
            return;
        }

        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.size(), to);
            pos += to.size();
        }
    }

    // Format utilities
    template <typename... Args>
    static std::string format(const char* fmt, Args... args) {
        int size = std::snprintf(nullptr, 0, fmt, args...);
        if (size <= 0) {
            return "";
        }

        std::string result(size, '\0');
        std::snprintf(&result[0], size + 1, fmt, args...);
        return result;
    }

    // Empty and blank checks
    static bool is_empty(std::string_view str) {
        return str.empty();
    }

    static bool is_blank(std::string_view str) {
        return std::all_of(str.begin(), str.end(),
                          [](unsigned char ch) { return std::isspace(ch); });
    }

    // Conversion utilities
    static int32_t to_int32(std::string_view str, int32_t default_value = 0) {
        try {
            return std::stoi(std::string(str));
        } catch (...) {
            return default_value;
        }
    }

    static int64_t to_int64(std::string_view str, int64_t default_value = 0) {
        try {
            return std::stoll(std::string(str));
        } catch (...) {
            return default_value;
        }
    }

    static double to_double(std::string_view str, double default_value = 0.0) {
        try {
            return std::stod(std::string(str));
        } catch (...) {
            return default_value;
        }
    }

    static bool to_bool(std::string_view str, bool default_value = false) {
        const auto s = to_lower(trim(str));
        if (s == "true" || s == "1" || s == "yes" || s == "on") {
            return true;
        }
        if (s == "false" || s == "0" || s == "no" || s == "off") {
            return false;
        }
        return default_value;
    }

    // Hash utilities
    static size_t hash(std::string_view str) {
        return std::hash<std::string_view>{}(str);
    }

    // Byte size formatting
    static std::string format_bytes(uint64_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
        int unit_index = 0;
        double size = static_cast<double>(bytes);

        while (size >= 1024.0 && unit_index < 5) {
            size /= 1024.0;
            ++unit_index;
        }

        char buffer[32];
#ifdef _WIN32
        sprintf_s(buffer, sizeof(buffer), "%.2f %s", size, units[unit_index]);
#else
        std::snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unit_index]);
#endif
        return std::string(buffer);
    }
};

} // namespace apollo::base
