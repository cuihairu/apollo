#include "apollo/net/metricspush/metrics_push_client.hpp"

#include <chrono>

#if !defined(_WIN32)

// POSIX 真实现：内部泵线程单写者——链路收发/重连全归泵拍，快照拉取
// （provider）也在泵线程。与 logpush 分工（头注）：无队列、kDroppable、
// 周期推——受理与否都推进节拍（丢拍不积压），非受理 = 本拍丢（最新者
// 胜，不补投）。kUp 守卫必设：link() 对 active 端点握手前恒非空，且
// Droppable 在 kHandshaking 也受理——不守卫会在握手前推陈旧快照。

#include "apollo/net/link/route_json.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"

#include <cstddef>
#include <fstream>
#include <sstream>
#include <utility>

namespace apollo::net::metricspush {

namespace {

namespace link = apollo::net::link;
namespace wire = apollo::net::wire;

using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) {
    return static_cast<std::uint16_t>(id);
}

// 单调毫秒（泵节拍；与内核注入时钟同口径）
std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count());
}

} // namespace

MetricsPushClient::MetricsPushClient(MetricsPushClientConfig config,
                                     SnapshotProvider provider)
    : config_(std::move(config)), provider_(std::move(provider)) {}

MetricsPushClient::~MetricsPushClient() {
    stop();
}

bool MetricsPushClient::start() {
    if (running_.load(std::memory_order_acquire)) {
        return true;
    }
    route_table_ = std::make_unique<link::RouteTable>();
    if (!config_.route_json.empty()) {
        std::ifstream in(config_.route_json, std::ios::binary);
        std::ostringstream buffer;
        if (in) {
            buffer << in.rdbuf();
        }
#if APOLLO_NET_LINK_HAS_JSON
        if (!link::load_route_table_json(buffer.str(), *route_table_)) {
            route_table_.reset();
            return false;  // 装配错误显形（坏 JSON = 无推面降级，不 throw）
        }
#else
        route_table_.reset();
        return false;
#endif
    } else {
        route_table_.reset();
        return false;
    }

    link::LinkManagerConfig mc;
    mc.self_name = config_.self_name;
    mc.route_version = link::route_version_from_hash(route_table_->schema_hash());
        mc.compat_route_versions = link::compat_route_versions_from_env();
    manager_ = std::make_unique<link::LinkManager>(mc);

    link::EndpointConfig ep;
    ep.name = "metrics";  // 对端 admin exporter 的握手身份（dial 目标别名）
    ep.host = config_.host;
    ep.port = config_.port;
    ep.role = link::EndpointRole::kActive;
    if (!manager_->add_endpoint(ep)) {
        manager_.reset();
        route_table_.reset();
        return false;
    }

    running_.store(true, std::memory_order_release);
    last_push_ms_ = 0;  // 首拍即推（链路 kUp 后第一快照立刻上行）
    pump_thread_ = std::thread([this] { run(); });
    return true;
}

void MetricsPushClient::stop() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (pump_thread_.joinable()) {
        pump_thread_.join();
    }
    manager_.reset();
    route_table_.reset();
}

bool MetricsPushClient::connected() const {
    auto* link = manager_ ? manager_->link("metrics") : nullptr;
    return link != nullptr && link->state() == link::LinkState::kUp;
}

std::uint64_t MetricsPushClient::pushed() const {
    return pushed_.load(std::memory_order_relaxed);
}

std::uint64_t MetricsPushClient::dropped() const {
    return dropped_.load(std::memory_order_relaxed);
}

void MetricsPushClient::run() {
    while (running_.load(std::memory_order_acquire)) {
        auto* link = manager_ ? manager_->link("metrics") : nullptr;
        if (link != nullptr && link->state() == link::LinkState::kUp) {
            const std::uint64_t now = now_ms();
            if (now - last_push_ms_ >= config_.push_interval_ms) {
                // 无队列：拉一次快照即发一发；受理与否都推进节拍
                //（丢本拍，下拍新快照自然追赶——最新者胜不补投）
                std::vector<wire::FieldValue> fields(2);
                fields[0].text = config_.self_name;
                fields[1].text = provider_ ? provider_() : std::string{};
                std::vector<std::uint8_t> payload;
                if (wire::encode_payload(id_of(MsgId::metrics_snapshot), fields,
                                         payload) &&
                    link->send(id_of(MsgId::metrics_snapshot), payload.data(),
                               payload.size(), link::SendClass::kDroppable) ==
                        link::SendCode::kAccepted) {
                    ++pushed_;
                } else {
                    ++dropped_;  // 编码失败/背压/断连：丢本拍（无队列）
                }
                last_push_ms_ = now;
            }
        }
        if (manager_) {
            manager_->pump(now_ms());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.pump_interval_ms));
    }
}

} // namespace apollo::net::metricspush

#else // _WIN32

namespace apollo::net::metricspush {

MetricsPushClient::MetricsPushClient(MetricsPushClientConfig config,
                                     SnapshotProvider provider)
    : config_(std::move(config)), provider_(std::move(provider)) {}
MetricsPushClient::~MetricsPushClient() = default;

bool MetricsPushClient::start() {
    // link TCP 底座不进本批（POSIX only）——Windows 退空件：start 真成、
    // 无推面（pushed/dropped 恒 0），调用面无 #if。
    return true;
}

void MetricsPushClient::stop() {}

bool MetricsPushClient::connected() const { return false; }
std::uint64_t MetricsPushClient::pushed() const { return 0; }
std::uint64_t MetricsPushClient::dropped() const { return 0; }

} // namespace apollo::net::metricspush

#endif // !_WIN32

namespace apollo::net::metricspush {

namespace {

// 标签值转义（Prometheus 文本格式：\\ \" \n）
void append_escaped(std::ostringstream& out, const std::string& value) {
    for (const char c : value) {
        switch (c) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        default: out << c; break;
        }
    }
}

} // namespace

// 纯字符串拼装，无平台依赖——跨平台共用（Windows 空件仅指推面）。
std::string render_exposition(
    const std::vector<apollo::core::metrics::MetricRegistry::Sample>& samples,
    const std::string& proc) {
    std::ostringstream out;
    const std::string* last_name = nullptr;
    for (const auto& sample : samples) {
        if (last_name == nullptr || *last_name != sample.name) {
            // TYPE 行同名只出一次（快照稳定排序保同名相邻）
            out << "# TYPE " << sample.name << ' '
                << (sample.is_counter
                        ? "counter"
                        : (sample.is_histogram ? "histogram" : "gauge"))
                << '\n';
            last_name = &sample.name;
        }
        if (sample.is_histogram) {
            // histogram 族：_bucket{le=...} 累计升档（末档 +Inf）+ _sum +
            // _count（Prometheus 文本面）；le 值走流默认 double 格式
            for (std::size_t i = 0; i < sample.histogram_bounds.size(); ++i) {
                out << sample.name << "_bucket{proc=\"";
                append_escaped(out, proc);
                out << "\",le=\"" << sample.histogram_bounds[i] << "\"} "
                    << sample.histogram_buckets[i] << '\n';
            }
            out << sample.name << "_bucket{proc=\"";
            append_escaped(out, proc);
            out << "\",le=\"+Inf\"} "
                << (sample.histogram_buckets.empty()
                        ? 0
                        : sample.histogram_buckets.back())
                << '\n';
            out << sample.name << "_sum{proc=\"";
            append_escaped(out, proc);
            out << "\"} " << sample.histogram_sum << '\n';
            out << sample.name << "_count{proc=\"";
            append_escaped(out, proc);
            out << "\"} " << sample.histogram_count << '\n';
            continue;
        }
        out << sample.name << "{proc=\"";
        append_escaped(out, proc);
        out << '"';
        for (const auto& kv : sample.labels) {
            out << ',' << kv.first << "=\"";
            append_escaped(out, kv.second);
            out << '"';
        }
        out << "} ";
        if (sample.is_counter) {
            out << sample.counter_value;
        } else {
            out << sample.gauge_value;
        }
        out << '\n';
    }
    return out.str();
}

} // namespace apollo::net::metricspush
