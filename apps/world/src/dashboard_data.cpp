#include "world_app/dashboard_data.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <signal.h>
#include <sstream>
#include <unistd.h>

namespace world_app {

namespace {

// /proc/self 读取（Linux；Windows 本批退化显示 "n/a"——与 metrics_http 的
// POSIX-only 口径一致）。文件缺失/解析失败返回 0。
std::uint64_t read_proc_u64(const std::string& path, std::size_t skip_fields) {
    std::ifstream in(path);
    if (!in) {
        return 0;
    }
    std::string token;
    for (std::size_t i = 0; i < skip_fields; ++i) {
        if (!(in >> token)) {
            return 0;
        }
    }
    std::uint64_t v = 0;
    in >> v;
    return v;
}

// RSS 页数 → KB（/proc/self/statm 第 2 字段 = 驻留页数；页大小 4096）。
std::uint64_t rss_kb() {
    const std::uint64_t pages = read_proc_u64("/proc/self/statm", 1);
    return pages * 4;  // 4096/1024
}

std::uint64_t threads() {
    // /proc/self/status 的 Threads 字段（第 2 个 token = 数值）
    std::ifstream in("/proc/self/status");
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("Threads:", 0) == 0) {
            const auto pos = line.find('\t');
            if (pos != std::string::npos) {
                return std::strtoull(line.c_str() + pos + 1, nullptr, 10);
            }
        }
    }
    return 0;
}

std::string escape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (const char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// 编队成员列表（读 FLEET_DIR/fleet.roster 的 name 列；空/缺失 = 空表）。
std::vector<std::string> fleet_roles(const std::string& fleet_dir) {
    std::vector<std::string> out;
    if (fleet_dir.empty()) {
        return out;
    }
    std::ifstream in(fleet_dir + "/fleet.roster");
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto bar = line.find('|');
        if (bar == std::string::npos) {
            continue;
        }
        std::string name = line.substr(0, bar);
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
            name.pop_back();
        }
        if (!name.empty()) {
            out.push_back(std::move(name));
        }
    }
    return out;
}

// 就绪判定（读成员日志中的就绪行——设计文档 §4：各成员就绪日志行出现即
// ready，与 start.sh ready_line 口径同源）。
bool fleet_ready(const std::string& fleet_dir, const std::string& role) {
    std::ifstream in(fleet_dir + "/logs/" + role + ".log");
    std::string line;
    while (std::getline(in, line)) {
        if (line.find("Server is running.") != std::string::npos
            || line.find("ready port=") != std::string::npos
            || line.find("listening on") != std::string::npos) {
            return true;
        }
    }
    return false;
}

// 日志尾（前 N 行；就绪面渲染只取末 3 行——设计文档 §4/§6）。
std::vector<std::string> fleet_log_tail(const std::string& fleet_dir,
                                        const std::string& role,
                                        std::size_t max_lines) {
    std::ifstream in(fleet_dir + "/logs/" + role + ".log");
    std::vector<std::string> all;
    std::string line;
    while (std::getline(in, line)) {
        all.push_back(line);
        if (all.size() > 1024) {
            all.erase(all.begin());  // 环形窗口（读侧限，不驻留全文件）
        }
    }
    const std::size_t start = all.size() > max_lines ? all.size() - max_lines : 0;
    return {std::make_move_iterator(all.begin() + start),
            std::make_move_iterator(all.end())};
}

std::uint64_t read_pid(const std::string& fleet_dir, const std::string& role) {
    std::ifstream in(fleet_dir + "/.run/" + role + ".pid");
    std::uint64_t pid = 0;
    in >> pid;
    return pid;
}

// pid 存活（0 = 不活；kill(pid, 0) EPERM 也算活——权限不足≠不在）。
bool pid_alive(std::uint64_t pid) {
    if (pid == 0) {
        return false;
    }
    return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
}

}  // namespace

DashboardDataCollector::DashboardDataCollector(DashboardProcessInfo info)
    : info_(std::move(info)) {
    if (info_.started_ms == 0) {
        info_.started_ms = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }
}

void DashboardDataCollector::collect_links(
    const std::vector<apollo::net::link::LinkSnapshot>& links) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.links = links;
}

void DashboardDataCollector::collect_script(
    const std::map<std::string, std::uint64_t>& modules) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.script_modules = modules;
}

void DashboardDataCollector::collect_metrics(
    const std::vector<apollo::core::metrics::MetricRegistry::Sample>& samples) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.metrics = samples;
}

void DashboardDataCollector::collect_fleet(
    const std::vector<apollo::net::link::LinkSnapshot>&) {
    // 编队成员块读本机 dev_fleet 运行目录文件面；本批无独立采集——
    // render 直接读文件（见 fleet 段）。
}

std::string DashboardDataCollector::render() const {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto now_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    const std::uint64_t uptime_ms = now_ms > info_.started_ms ? now_ms - info_.started_ms : 0;

    std::ostringstream out;
    out << "{\"process\":{";
    out << "\"pid\":" << ::getpid();
    out << ",\"uptime_ms\":" << uptime_ms;
    out << ",\"rss_kb\":" << rss_kb();
    out << ",\"threads\":" << threads();
    out << ",\"started_ms\":" << info_.started_ms;
    out << ",\"role\":\"" << escape_json(info_.role) << "\"}";

    // links
    out << ",\"links\":[";
    bool first_link = true;
    for (const auto& l : data_.links) {
        if (!first_link) out << ',';
        first_link = false;
        out << "{\"peer\":\"" << escape_json(l.peer) << "\"";
        out << ",\"state\":\"" << escape_json(l.state) << "\"";
        out << ",\"tls\":" << (l.tls ? "true" : "false");
        out << ",\"tx\":" << l.tx;
        out << ",\"rx\":" << l.rx;
        out << ",\"queued_bytes\":" << l.queued_bytes;
        out << "}";
    }
    out << "]";

    // orders（本批生产进程无 OrderStore 实例 → 空表；契约面保形状）
    out << ",\"orders\":{\"pending\":0,\"delivered\":0,\"pending_detail\":[]}";

    // fleet（本机编队文件面：pid + ready + 日志尾）
    out << ",\"fleet\":[";
    bool first_fleet = true;
    for (const auto& role : fleet_roles(info_.fleet_dir)) {
        if (!first_fleet) out << ',';
        first_fleet = false;
        const auto pid = read_pid(info_.fleet_dir, role);
        const bool alive = pid_alive(pid);
        out << "{\"role\":\"" << escape_json(role) << "\"";
        out << ",\"pid\":" << pid;
        out << ",\"alive\":" << (alive ? "true" : "false");
        out << ",\"ready\":" << (alive && fleet_ready(info_.fleet_dir, role) ? "true" : "false");
        out << ",\"log_tail\":[";
        bool first_line = true;
        for (const auto& l : fleet_log_tail(info_.fleet_dir, role, 3)) {
            if (!first_line) out << ',';
            first_line = false;
            out << '"' << escape_json(l) << '"';
        }
        out << "]}";
    }
    out << "]";

    // script
    out << ",\"script\":{\"modules\":[";
    bool first_mod = true;
    for (const auto& [name, version] : data_.script_modules) {
        if (!first_mod) out << ',';
        first_mod = false;
        out << "{\"name\":\"" << escape_json(name) << "\",\"version\":" << version << "}";
    }
    out << "]}";

    // load（ticks_total + gauges）
    std::uint64_t ticks_total = 0;
    out << ",\"load\":{\"ticks_total\":";
    for (const auto& s : data_.metrics) {
        if (s.name == "world_ticks_total" && s.is_counter) {
            ticks_total = s.counter_value;
            break;
        }
    }
    out << ticks_total << ",\"gauges\":[";
    bool first_gauge = true;
    for (const auto& s : data_.metrics) {
        if (s.is_histogram) {
            continue;
        }
        if (!first_gauge) out << ',';
        first_gauge = false;
        out << "{\"name\":\"" << escape_json(s.name) << "\"";
        out << ",\"value\":" << (s.is_counter ? s.counter_value : s.gauge_value);
        out << "}";
    }
    out << "]}";

    out << "}";
    return out.str();
}

}  // namespace world_app
