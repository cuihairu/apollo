// LoggerApp 实现：检索面 v1（scan/follow）+ 目录摄取 + 过滤。

#include "logger/logger_app.hpp"

#include <chrono>
#include <algorithm>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;

namespace logger {

namespace {

volatile std::sig_atomic_t g_run = 1;

void on_signal(int) {
    g_run = 0;
}

const char* kUsage =
    "Usage: logger [scan|follow] [options]\n"
    "Options:\n"
    "  --dir <path>       Log directory (default: log)\n"
    "  --proc <name>      Filter by proc key\n"
    "  --level <LEVEL>    Minimum level (DEBUG<INFO<WARN<ERROR<CRITICAL)\n"
    "  --cat <name>       Filter by cat key\n"
    "  --since <ms>       Only lines at or after epoch ms\n"
    "  --until <ms>       Only lines at or before epoch ms\n"
    "  --interval <ms>    Follow poll interval (default: 500)\n"
    "  --help, -h         Show this help\n";

} // namespace

int level_rank(const std::string& name) {
    if (name == "DEBUG") return 0;
    if (name == "INFO") return 1;
    if (name == "WARN") return 2;
    if (name == "ERROR") return 3;
    if (name == "CRITICAL") return 4;
    return -1;
}

bool RetrievalFilter::passes(const StructuredLine& line) const {
    if (!proc.empty() && line.proc != proc) {
        return false;
    }
    if (!cat.empty() && line.cat != cat) {
        return false;
    }
    if (min_level >= 0) {
        const int rank = level_rank(line.level);
        if (rank < min_level) {
            return false;
        }
    }
    if (since_ms != 0 && line.ts < since_ms) {
        return false;
    }
    if (until_ms != 0 && line.ts > until_ms) {
        return false;
    }
    return true;
}

IngestStats ingest_dir(const std::string& dir, IPushSink& sink) {
    IngestStats stats;
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        return stats;
    }
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".log") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    stats.files = files.size();
    for (const auto& path : files) {
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line)) {
            StructuredLine record;
            if (!parse_structured_line(line, &record)) {
                ++stats.skipped;
                continue;
            }
            ++stats.parsed;
            sink.on_record(record);
        }
    }
    return stats;
}

int LoggerApp::run(int argc, char* argv[]) {
    std::string subcommand = "scan";
    std::string dir = "log";
    RetrievalFilter filter;
    std::uint32_t interval_ms = 500;
    bool help = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "scan" || arg == "follow") {
            subcommand = arg;
        } else if (arg == "--dir" && i + 1 < argc) {
            dir = argv[++i];
        } else if (arg == "--proc" && i + 1 < argc) {
            filter.proc = argv[++i];
        } else if (arg == "--cat" && i + 1 < argc) {
            filter.cat = argv[++i];
        } else if (arg == "--level" && i + 1 < argc) {
            filter.min_level = level_rank(argv[++i]);
            if (filter.min_level < 0) {
                std::cerr << "logger: unknown level" << std::endl;
                return 1;
            }
        } else if (arg == "--since" && i + 1 < argc) {
            filter.since_ms = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--until" && i + 1 < argc) {
            filter.until_ms = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--interval" && i + 1 < argc) {
            interval_ms = static_cast<std::uint32_t>(
                std::strtoul(argv[++i], nullptr, 10));
            if (interval_ms == 0) {
                interval_ms = 500;
            }
        } else if (arg == "--help" || arg == "-h") {
            help = true;
        }
    }
    if (help) {
        std::cout << kUsage;
        return 0;
    }

    try {
        if (subcommand == "follow") {
            return run_follow(dir, filter, interval_ms);
        }
        return run_scan(dir, filter);
    } catch (const std::exception& e) {
        std::cerr << "logger: " << e.what() << std::endl;
        return 1;
    }
}

int LoggerApp::run_scan(const std::string& dir, const RetrievalFilter& filter) {
    // 检索输出 = 解析重排：六键序稳定（§5.1 键集契约），kv 段序保持解析序；
    // stdout 是查询结果面，按行冲刷（stderr 走汇总）
    class RenderSink final : public IPushSink {
    public:
        const RetrievalFilter* filter = nullptr;
        std::size_t matched = 0;

        void on_record(const StructuredLine& r) override {
            if (filter != nullptr && !filter->passes(r)) {
                return;
            }
            ++matched;
            std::cout << "ts=" << r.ts << " level=" << r.level
                      << " proc=" << r.proc << " tick=" << r.tick
                      << " cat=" << r.cat << " msg=" << r.msg;
            for (const auto& kv : r.kv) {
                std::cout << " " << kv.first << "=" << kv.second;
            }
            std::cout << std::endl;
        }
    };
    RenderSink sink;
    sink.filter = &filter;
    const IngestStats stats = ingest_dir(dir, sink);
    std::cerr << "scan: files=" << stats.files << " parsed=" << stats.parsed
              << " skipped=" << stats.skipped << " matched=" << sink.matched
              << std::endl;
    return 0;
}

void LoggerApp::poll_once(const std::string& dir, const RetrievalFilter& filter,
                          std::map<std::string, std::streamoff>* offsets) {
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        return;
    }
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".log") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const auto& path : files) {
        const std::string key = path.string();
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            continue;
        }
        const auto known = offsets->find(key);
        const std::streamoff start =
            known != offsets->end() ? known->second : 0;
        // 从 start 读到 EOF；半行（末尾无 '\n'）留待下轮
        in.seekg(0, std::ios::end);
        const std::streamoff end = in.tellg();
        if (end <= start) {
            continue;
        }
        in.seekg(start);
        std::string chunk(static_cast<std::size_t>(end - start), '\0');
        in.read(chunk.data(), end - start);
        const std::size_t last_newline = chunk.find_last_of('\n');
        if (last_newline == std::string::npos) {
            continue;  // 整段还是半行
        }
        std::size_t pos = 0;
        while (pos <= last_newline) {
            const std::size_t nl = chunk.find('\n', pos);
            const std::size_t stop =
                nl == std::string::npos ? last_newline + 1 : nl;
            std::string line = chunk.substr(pos, stop - pos);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            StructuredLine record;
            if (!line.empty() && parse_structured_line(line, &record)) {
                if (filter.passes(record)) {
                    std::cout << line << std::endl;  // 原行透传（保真）
                }
            }
            pos = stop + 1;
        }
        (*offsets)[key] = start + static_cast<std::streamoff>(last_newline) + 1;
    }
    // 已消失文件（滚动清理）的偏移记录清除，防 map 无界
    for (auto it = offsets->begin(); it != offsets->end();) {
        const bool still_there =
            std::find(files.begin(), files.end(), fs::path(it->first)) != files.end();
        it = still_there ? std::next(it) : offsets->erase(it);
    }
}

int LoggerApp::run_follow(const std::string& dir, const RetrievalFilter& filter,
                          std::uint32_t interval_ms) {
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    std::map<std::string, std::streamoff> offsets;
    while (g_run != 0) {
        poll_once(dir, filter, &offsets);
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
    return 0;
}

} // namespace logger
