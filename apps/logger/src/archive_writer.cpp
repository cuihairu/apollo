#include "logger/archive_writer.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace logger {

std::string archive_today() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    std::ostringstream oss;
    oss << std::put_time(&local, "%Y%m%d");
    return oss.str();
}

int max_files_from_env(const char* value) {
    if (value == nullptr || *value == '\0') {
        return 30;
    }
    try {
        return std::stoi(value);
    } catch (const std::exception&) {
        return 30;
    }
}

ArchiveWriter::ArchiveWriter(Config config) : config_(std::move(config)) {}

bool ArchiveWriter::open() {
    std::error_code ec;
    fs::create_directories(config_.directory, ec);
    if (!config_.directory.empty() && !fs::is_directory(config_.directory)) {
        std::cerr << "archive writer: cannot create directory " << config_.directory
                  << std::endl;
        return false;
    }
    current_date_ = date_provider_ ? date_provider_() : archive_today();
    current_path_ = path_for(current_date_);
    // app 模式：同日重启续写，不覆写既有
    stream_.open(current_path_, std::ios::app | std::ios::binary);
    if (!stream_) {
        std::cerr << "archive writer: cannot open " << current_path_ << std::endl;
        current_path_.clear();
        current_date_.clear();
        return false;
    }
    return true;
}

void ArchiveWriter::write_line(const std::string& line) {
    const std::string today = date_provider_ ? date_provider_() : archive_today();
    if (today != current_date_) {
        rotate_to(today);
    }
    if (!stream_.is_open()) {
        return;  // 重开失败 = 写失败降级（归档主路停行不抛，受理面存活）
    }
    // 逐行冲刷：采集面 durability 优先（编队日志速率低）——硬杀不吞缓冲，
    // scan/follow 即时可见（§5 本地文件真相源同口径）。
    stream_ << line << '\n';
    stream_.flush();
}

void ArchiveWriter::flush() {
    if (stream_.is_open()) {
        stream_.flush();
    }
}

void ArchiveWriter::rotate_to(const std::string& date) {
    // 关旧→开新当日本名（FileAppender doRotation 同构：close 即落盘，
    // 不落 rename）；重开失败不回滚——写面自停行，日期面已前移。
    stream_.close();
    stream_.clear();
    current_date_ = date;
    current_path_ = path_for(date);
    stream_.open(current_path_, std::ios::app | std::ios::binary);
    if (!stream_) {
        std::cerr << "archive writer: rotation open failed: " << current_path_
                  << std::endl;
    }
    cleanup_old_files();
}

std::string ArchiveWriter::path_for(const std::string& date) const {
    fs::path p(config_.directory);
    p /= config_.base_name + "_" + date + ".log";
    return p.string();
}

void ArchiveWriter::cleanup_old_files() {
    if (config_.max_files <= 0) {
        return;
    }
    std::error_code ec;
    if (!fs::is_directory(config_.directory, ec)) {
        return;
    }
    // 只清本写入器名下的 <base>_*.log（BI tap / 旧名 collector.log 等
    // 他名文件不动）；排序按文件名——定宽日期命名下字典序恒等时间序
    //（mtime 在 rename 缺席的本 scheme 下无信息量）。
    const std::string prefix = config_.base_name + "_";
    std::vector<std::string> files;
    for (const auto& entry : fs::directory_iterator(config_.directory, ec)) {
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (name.rfind(prefix, 0) != 0 || name.size() <= prefix.size() ||
            name.compare(name.size() - 4, 4, ".log") != 0) {
            continue;
        }
        files.push_back(name);
    }
    if (static_cast<int>(files.size()) <= config_.max_files) {
        return;
    }
    std::sort(files.begin(), files.end());
    const std::size_t to_delete = files.size() - static_cast<std::size_t>(config_.max_files);
    for (std::size_t i = 0; i < to_delete; ++i) {
        std::error_code rm_ec;
        fs::remove(fs::path(config_.directory) / files[i], rm_ec);
        if (rm_ec) {
            std::cerr << "archive writer: cleanup failed for " << files[i] << std::endl;
        }
    }
}

}  // namespace logger
