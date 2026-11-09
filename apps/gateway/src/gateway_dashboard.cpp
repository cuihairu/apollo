#include "gateway/gateway_server.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <chrono>
#include <vector>
#if !defined(_WIN32)
#include <unistd.h>
#endif

//------------------------------------------------------------------------------
// /api/state 快照渲染（gateway 维度 dashboard；调用线程 = 宿主泵线程——
// dashboard 旁路 pump 面挂在本泵上，会话面走管理器锁，计数面全原子）。
//------------------------------------------------------------------------------

// （gateway_server 拆分单元：dashboard 序列化 + /proc 读取助手）

namespace gateway {

namespace {

// JSON 字符串转义（drain_detail 等自由文本进 JSON 时防损坏）。
std::string json_escape(const std::string& s) {
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
                    std::snprintf(buf, sizeof buf, "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// /proc/self 读取（同 world dashboard_data 口径；读失败返回 0）。
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

std::uint64_t proc_rss_kb() {
    return read_proc_u64("/proc/self/statm", 1) * 4;  // 页数 × 4KB→KB(4096/1024)
}

std::uint64_t proc_threads() {
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

std::uint64_t wall_now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

const char* drain_state_label(std::int32_t state) {
    switch (state) {
        case kDrainDraining: return "draining";
        case kDrainDrained: return "drained";
        default: return "serving";
    }
}
} // namespace

std::string GatewayServer::dashboardJson() const {
    // 会话面（管理器锁内取全量，按状态分桶）
    std::uint64_t online = 0;
    std::uint64_t authenticated = 0;
    std::uint64_t retained = 0;
    for (const auto& session : sessionManager_->getAllSessions()) {
        ++online;
        if (session->state == SessionState::DISCONNECTED) {
            ++retained;
        } else if (session->playerId != 0) {
            ++authenticated;
        }
    }

    // 进程期 uptime（wall 钟构造点起算）
    const std::uint64_t uptime_ms = wall_now_ms() - startedWallMs_;
    const std::uint64_t ticks = pumpTicks_.load(std::memory_order_relaxed);
    const double pump_hz = uptime_ms > 0 ? static_cast<double>(ticks) * 1000.0 /
                                               static_cast<double>(uptime_ms)
                                         : 0.0;

    auto& registry = apollo::core::metrics::MetricRegistry::instance();

    std::ostringstream out;
    out << "{\"role\":\"gateway\"";
    out << ",\"process\":{\"pid\":" << static_cast<std::uint64_t>(::getpid())
        << ",\"rss_kb\":" << proc_rss_kb()
        << ",\"threads\":" << proc_threads()
        << ",\"started_ms\":" << startedWallMs_
        << ",\"uptime_ms\":" << uptime_ms << "}";
    out << ",\"listen\":{\"host\":\"" << config_.host << "\""
        << ",\"port\":" << listenPort()
        << ",\"ws_port\":" << wsListenPort()
        << ",\"wss_port\":" << wssListenPort()
        << ",\"max_connections\":" << config_.maxConnections
        << ",\"active_connections\":" << getActiveConnectionCount() << "}";
    out << ",\"sessions\":{\"online\":" << online
        << ",\"authenticated\":" << authenticated
        << ",\"retained\":" << retained
        << ",\"heartbeat_timeout_total\":" << heartbeatTimeoutClosures_.load() << "}";
    out << ",\"auth\":{\"requests\":" << authRequests_.load()
        << ",\"accepted\":" << authAccepted_.load()
        << ",\"rejected\":" << authRejected_.load() << "}";
    out << ",\"rejects\":{\"unauthenticated_world\":" << unauthenticatedWorldDrops_.load()
        << ",\"unauthenticated_base\":" << unauthenticatedBaseDrops_.load()
        << ",\"duplicate_frames\":" << sessionManager_->duplicateUplinkFrames() << "}";
    out << ",\"rate\":{\"drain_state\":\"" << drain_state_label(drainState()) << "\""
        << ",\"drain_detail\":\"" << json_escape(drain_detail_) << "\""
        << ",\"base_forward_resolves\":" << baseForwardResolves_.load()
        << ",\"base_forward_rejections\":" << baseForwardRejections_.load() << "}";
    out << ",\"intents\":{\"sent\":" << gatewayIntentsSentTotal_.load()
        << ",\"acked\":" << gatewayIntentsAckedTotal_.load()
        << ",\"failed\":" << gatewayIntentsFailedTotal_.load()
        << ",\"timed_out\":" << gatewayIntentsTimedOutTotal_.load()
        << ",\"unforwardable\":" << gatewayIntentsUnforwardableTotal_.load()
        << ",\"dropped\":" << gatewayIntentsDroppedTotal_.load() << "}";
    out << ",\"tick\":{\"pump_ticks\":" << ticks
        << ",\"pump_hz\":" << std::fixed << std::setprecision(1) << pump_hz << "}";
    out << ",\"metrics\":[";
    {
        bool first = true;
        for (const auto& sample : registry.snapshot()) {
            if (!first) {
                out << ",";
            }
            first = false;
            out << "{\"name\":\"" << sample.name << "\"";
            if (sample.is_histogram) {
                out << ",\"is_histogram\":true"
                    << ",\"sum\":" << sample.histogram_sum
                    << ",\"count\":" << sample.histogram_count;
            } else if (sample.is_counter) {
                out << ",\"value\":" << sample.counter_value;
            } else {
                out << ",\"value\":" << sample.gauge_value;
            }
            out << "}";
        }
    }
    out << "]}";
    return out.str();
}

} // namespace gateway
