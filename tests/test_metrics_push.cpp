// 指标 push 通道测试（MetricsPushTests，P3-3 批）。
//
// 覆盖（logging.md §5.1 上行管线 + 采样流语义分工，对比 LogPushTests）：
//   1. render_exposition：counter/gauge 两型、TYPE 行同名只出一次、
//      标签随行 + 转义（\ " \n）、proc 转义、空快照零输出；
//   2. snapshot_e2e：hub（LinkManager passive + 931 收集）↔
//      MetricsPushClient round-trip（proc/body 保真）；
//   3. periodic_latest_wins：周期推（20ms 档）连发多拍，接收序与发送序
//      一致（TCP 序）且逐拍新快照（无队列不重放）；
//   4. down_no_queue：对端缺席期零推面零丢计（kUp 守卫——与日志面
//      「缺席缓冲补投」相区分的采样流语义），对端上线后快照自然上行；
//   5. hub_semantics：apps/admin MetricsHub 聚合面（last-wins 覆盖、
//      逐 proc 名排序、proc_up 陈旧墓碑、体长上限拒收不覆盖）；
//   6. http_metrics_endpoint：apps/admin MetricsHttp 出口面真端口往返
//      （GET /metrics → 200 text/plain + 聚合文本、其余路径 404、
//      应答完成连接表清空）。
//
// Windows：link TCP 底座 POSIX only——只跑 render 面（纯字符串拼装）。

#include "apollo/net/metricspush/metrics_push_client.hpp"
#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/link/tcp_transport.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"

#include "admin/metrics_hub.hpp"
#include "admin/metrics_http.hpp"

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifndef APOLLO_CONTRACT_ROUTE_JSON
#define APOLLO_CONTRACT_ROUTE_JSON ""
#endif

#define TEST_ASSERT(cond, msg)                                                           \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")"   \
                      << std::endl;                                                      \
            return false;                                                                \
        }                                                                                \
    } while (0)

namespace {

namespace link = apollo::net::link;
namespace wire = apollo::net::wire;
namespace metricspush = apollo::net::metricspush;
namespace admin = apollo::admin;

using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) {
    return static_cast<std::uint16_t>(id);
}

std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count());
}

bool load_table(link::RouteTable& table) {
    std::ifstream in(APOLLO_CONTRACT_ROUTE_JSON, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
#if APOLLO_NET_LINK_HAS_JSON
    return link::load_route_table_json(buffer.str(), table);
#else
    (void)table;
    return false;
#endif
}

bool test_render_exposition() {
    using Sample = apollo::core::metrics::MetricRegistry::Sample;

    // 空快照：零输出
    TEST_ASSERT(metricspush::render_exposition({}, "proc_a").empty(), "空快照零输出");

    // counter + gauge、TYPE 行同名只出一次、标签随行、值域两型
    std::vector<Sample> samples;
    Sample c0;
    c0.name = "fault_total";
    c0.is_counter = true;
    c0.counter_value = 7;
    samples.push_back(c0);
    Sample g0;
    g0.name = "entity_count";
    g0.labels = {{"scene", "town"}};
    g0.gauge_value = 42;
    samples.push_back(g0);
    Sample g1;
    g1.name = "entity_count";
    g1.labels = {{"scene", "mine"}};
    g1.gauge_value = -3;
    samples.push_back(g1);

    const std::string text = metricspush::render_exposition(samples, "cell@1");
    const std::string expected =
        "# TYPE fault_total counter\n"
        "fault_total{proc=\"cell@1\"} 7\n"
        "# TYPE entity_count gauge\n"
        "entity_count{proc=\"cell@1\",scene=\"town\"} 42\n"
        "entity_count{proc=\"cell@1\",scene=\"mine\"} -3\n";
    TEST_ASSERT(text == expected, "exposition 文本逐位比对");

    // 标签值转义（\ " \n）+ proc 转义
    Sample e0;
    e0.name = "esc_total";
    e0.is_counter = true;
    e0.counter_value = 1;
    e0.labels = {{"msg", "a\"b\\c\nd"}};
    const std::vector<Sample> esc = {e0};
    const std::string esc_text = metricspush::render_exposition(esc, "p\"x\\y\nz");
    TEST_ASSERT(esc_text ==
                    "# TYPE esc_total counter\n"
                    "esc_total{proc=\"p\\\"x\\\\y\\nz\",msg=\"a\\\"b\\\\c\\nd\"} 1\n",
                "标签值与 proc 转义");
    return true;
}

#if !defined(_WIN32)

// hub 侧受集体：passive 端点 + 931 handler 收快照（proc/body 保真记账）
struct Hub {
    link::RouteTable table;
    std::unique_ptr<link::LinkManager> manager;
    std::vector<std::pair<std::string, std::string>> snapshots;  // {proc, body}
    std::mutex snapshots_mutex;

    bool start(std::uint16_t port = 0) {
        if (!load_table(table)) {
            return false;
        }
        link::LinkManagerConfig mc;
        mc.self_name = "metrics";
        mc.route_version = link::route_version_from_hash(table.schema_hash());
        manager = std::make_unique<link::LinkManager>(mc);
        link::EndpointConfig ep;
        ep.name = "apps";
        ep.host = "127.0.0.1";
        ep.port = port;
        ep.role = link::EndpointRole::kPassive;
        if (!manager->add_endpoint(ep)) {
            manager.reset();
            return false;
        }
        manager->set_peer_listener([&](link::InterServerLink& peer) {
            auto& dispatcher = peer.dispatcher();
            dispatcher.set_route_table(&table);
            dispatcher.allow_domain("internal");
            dispatcher.set_handler(id_of(MsgId::metrics_snapshot),
                                   [&](std::uint16_t, const std::uint8_t* data,
                                       std::size_t size) {
                                       std::vector<wire::FieldValue> fields;
                                       if (!wire::decode_payload(
                                               id_of(MsgId::metrics_snapshot), data, size,
                                               fields) ||
                                           fields.size() < 2) {
                                           return;
                                       }
                                       std::lock_guard<std::mutex> lock(snapshots_mutex);
                                       snapshots.emplace_back(std::move(fields[0].text),
                                                              std::move(fields[1].text));
                                   });
        });
        return true;
    }

    std::uint16_t port() const { return manager ? manager->endpoint_port("apps") : 0; }

    void pump() {
        if (manager) {
            manager->pump(now_ms());
        }
    }

    std::size_t count() {
        std::lock_guard<std::mutex> lock(snapshots_mutex);
        return snapshots.size();
    }

    std::vector<std::pair<std::string, std::string>> copy() {
        std::lock_guard<std::mutex> lock(snapshots_mutex);
        return snapshots;
    }
};

// 双面泵到条件成立（10ms 拍，秒级预算）
template <typename Pred>
bool pump_until(Hub& hub, metricspush::MetricsPushClient& client, Pred&& done,
                int timeout_ms = 5000) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        hub.pump();
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    hub.pump();
    return done();
}

metricspush::MetricsPushClientConfig probe_config(std::uint16_t port) {
    metricspush::MetricsPushClientConfig cfg;
    cfg.port = port;
    cfg.route_json = APOLLO_CONTRACT_ROUTE_JSON;
    cfg.self_name = "app_probe";
    cfg.push_interval_ms = 20;
    cfg.pump_interval_ms = 10;
    return cfg;
}

bool test_snapshot_e2e() {
    Hub hub;
    TEST_ASSERT(hub.start(), "hub assembled");
    TEST_ASSERT(hub.port() != 0, "ephemeral port observed");

    metricspush::MetricsPushClient client(
        probe_config(hub.port()),
        [] { return std::string("# TYPE probe_total counter\nprobe_total 1\n"); });
    TEST_ASSERT(client.start(), "client started");

    TEST_ASSERT(pump_until(hub, client, [&] { return hub.count() >= 1; }),
                "snapshot delivered");
    TEST_ASSERT(client.connected(), "client link up");
    TEST_ASSERT(client.pushed() >= 1, "pushed counted");
    TEST_ASSERT(client.dropped() == 0, "nothing dropped with live link");

    const auto snaps = hub.copy();
    TEST_ASSERT(snaps.back().first == "app_probe", "proc field preserved");
    TEST_ASSERT(snaps.back().second == "# TYPE probe_total counter\nprobe_total 1\n",
                "body verbatim");

    client.stop();
    return true;
}

bool test_periodic_latest_wins() {
    Hub hub;
    TEST_ASSERT(hub.start(), "hub assembled");

    // provider 每拍自增：收到的 body 序 = 发送序且逐拍新值（TCP 序 + 无队列）
    int tick = 0;
    metricspush::MetricsPushClient client(probe_config(hub.port()), [&tick] {
        return "# tick " + std::to_string(++tick);
    });
    TEST_ASSERT(client.start(), "client started");

    TEST_ASSERT(pump_until(hub, client, [&] { return hub.count() >= 3; }),
                "at least three periodic snapshots");
    const auto snaps = hub.copy();
    for (std::size_t i = 0; i < snaps.size(); ++i) {
        TEST_ASSERT(snaps[i].second == "# tick " + std::to_string(i + 1),
                    "snapshot in order, each beat fresh");
    }

    client.stop();
    return true;
}

bool test_down_no_queue() {
    // 占口即拆（client 拨号期无人受理）：缺席期零推面（kUp 守卫）、零丢计
    //、零缓冲——与日志面「缺席缓冲重连补投」相区分的采样流语义
    std::uint16_t port = 0;
    {
        Hub placeholder;
        TEST_ASSERT(placeholder.start(), "placeholder assembled");
        port = placeholder.port();
        TEST_ASSERT(port != 0, "port picked");
    }

    metricspush::MetricsPushClient client(probe_config(port),
                                          [] { return std::string("x"); });
    TEST_ASSERT(client.start(), "client started (hub absent)");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    TEST_ASSERT(client.pushed() == 0, "no push attempted while down");
    TEST_ASSERT(client.dropped() == 0, "no drop counted while down (no queue)");

    // 对端同口上线 → 退避重拨 → 快照自然上行（最新者胜）
    Hub hub;
    TEST_ASSERT(hub.start(port), "hub rebound on same port");
    TEST_ASSERT(pump_until(hub, client, [&] { return hub.count() >= 1; }),
                "snapshot flows after hub returns");
    TEST_ASSERT(client.dropped() == 0, "still zero drops (nothing queued)");

    client.stop();
    return true;
}

// ---- apps/admin 聚合面（MetricsHub）----

bool test_hub_semantics() {
    admin::MetricsHub hub;

    // last-wins：同 proc 后到快照覆盖，先到的不再出现
    hub.ingest("baseapp", "baseapp-body\n", now_ms());
    hub.ingest("baseapp", "baseapp-body-v2\n", now_ms());
    hub.ingest("gateway", "gateway-body\n", now_ms());
    TEST_ASSERT(hub.procs() == 2, "two procs registered");
    TEST_ASSERT(hub.ingested() == 3, "three snapshots ingested");

    const std::string fresh = hub.render(now_ms());
    TEST_ASSERT(fresh.rfind("# TYPE apollo_proc_up gauge\n", 0) == 0,
                "proc_up TYPE line once at head");
    TEST_ASSERT(fresh.find("apollo_proc_up{proc=\"baseapp\"} 1\nbaseapp-body-v2\n")
                    != std::string::npos,
                "baseapp fresh with last-wins body");
    TEST_ASSERT(fresh.find("apollo_proc_up{proc=\"gateway\"} 1\ngateway-body\n")
                    != std::string::npos,
                "gateway fresh");
    TEST_ASSERT(fresh.find("baseapp-body-v1") == std::string::npos,
                "superseded body dropped");
    TEST_ASSERT(fresh.find("apollo_proc_up{proc=\"baseapp\"}")
                    < fresh.find("apollo_proc_up{proc=\"gateway\"}"),
                "procs sorted by name");

    // 陈旧翻转：超出判据后只出墓碑，样本行不再出
    const std::string stale = hub.render(now_ms() + 30001);
    TEST_ASSERT(stale.find("apollo_proc_up{proc=\"baseapp\"} 0\n") != std::string::npos,
                "baseapp tombstoned");
    TEST_ASSERT(stale.find("apollo_proc_up{proc=\"gateway\"} 0\n") != std::string::npos,
                "gateway tombstoned");
    TEST_ASSERT(stale.find("baseapp-body-v2") == std::string::npos,
                "stale samples dropped");
    TEST_ASSERT(stale.find("gateway-body") == std::string::npos,
                "stale samples dropped");

    // 体长上限：超限拒收计数、不覆盖既有快照
    admin::MetricsHubConfig cfg;
    cfg.max_body_bytes = 8;
    admin::MetricsHub limited(cfg);
    limited.ingest("cell", "ok\n", now_ms());
    TEST_ASSERT(limited.ingested() == 1, "small body accepted");
    limited.ingest("cell", "way too long body\n", now_ms());
    TEST_ASSERT(limited.rejected() == 1, "oversized body rejected");
    TEST_ASSERT(limited.ingested() == 1, "oversized not counted as ingested");
    const std::string kept = limited.render(now_ms());
    TEST_ASSERT(kept.find("ok\n") != std::string::npos, "previous snapshot kept");
    TEST_ASSERT(kept.find("way too long") == std::string::npos,
                "oversized body not stored");
    limited.ingest("", "x\n", now_ms());
    TEST_ASSERT(limited.rejected() == 2, "empty proc rejected");
    return true;
}

// ---- apps/admin 出口面（MetricsHttp，真端口往返）----

struct HttpReply {
    bool connected = false;
    std::string body;
};

// 拨号（宿主泵驱动）+ 全量写出 + 读到 EOF（服务端 Connection: close）
HttpReply http_fetch(admin::MetricsHttp& http, std::uint16_t port,
                     const std::string& request) {
    HttpReply reply;
    link::TcpConnector connector;
    if (!connector.start("127.0.0.1", port, 3000, now_ms())) {
        return reply;
    }
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(5000);
    while (connector.state() == link::ConnectState::kConnecting) {
        if (connector.pump(now_ms()) != link::TcpConnector::PumpResult::kInProgress) {
            break;
        }
        if (std::chrono::steady_clock::now() > deadline) {
            return reply;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    if (connector.state() != link::ConnectState::kConnected) {
        return reply;
    }
    auto transport = connector.take_transport();
    if (transport == nullptr) {
        return reply;
    }
    reply.connected = true;

    std::size_t sent = 0;
    while (sent < request.size()) {
        const int n = transport->write(
            reinterpret_cast<const std::uint8_t*>(request.data()) + sent,
            request.size() - sent);
        if (n < 0) {
            return reply;
        }
        if (n > 0) {
            sent += static_cast<std::size_t>(n);
        } else if (std::chrono::steady_clock::now() > deadline) {
            return reply;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    std::uint8_t buf[4096];
    while (std::chrono::steady_clock::now() < deadline) {
        http.pump(now_ms());
        const int n = transport->read(buf, sizeof buf);
        if (n < 0) {
            break;  // 对端关闭（Connection: close 后 EOF）
        }
        if (n > 0) {
            reply.body.append(reinterpret_cast<const char*>(buf),
                              static_cast<std::size_t>(n));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    return reply;
}

bool test_http_metrics_endpoint() {
    admin::MetricsHub hub;
    hub.ingest("baseapp",
               "# TYPE probe_total counter\n"
               "probe_total{proc=\"baseapp\"} 7\n",
               now_ms());

    admin::MetricsHttpConfig cfg;
    cfg.port = 0;
    admin::MetricsHttp http(cfg, hub);
    TEST_ASSERT(http.start(), "http bound");
    const std::uint16_t port = http.port();
    TEST_ASSERT(port != 0, "ephemeral port observed");

    // GET /metrics → 200 text/plain + 聚合文本（proc_up 墓碑面 + 源端样本直拼接）
    const HttpReply ok =
        http_fetch(http, port, "GET /metrics HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    TEST_ASSERT(ok.connected, "client connected");
    TEST_ASSERT(ok.body.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, "200 ok");
    TEST_ASSERT(ok.body.find("Content-Type: text/plain; version=0.0.4\r\n")
                    != std::string::npos,
                "prometheus content type");
    TEST_ASSERT(ok.body.find("Content-Length: ") != std::string::npos,
                "content length present");
    TEST_ASSERT(ok.body.find("# TYPE apollo_proc_up gauge\n") != std::string::npos,
                "proc_up TYPE line");
    TEST_ASSERT(ok.body.find("apollo_proc_up{proc=\"baseapp\"} 1\n")
                    != std::string::npos,
                "proc_up alive");
    TEST_ASSERT(ok.body.find("# TYPE probe_total counter\n") != std::string::npos,
                "source TYPE line");
    TEST_ASSERT(ok.body.find("probe_total{proc=\"baseapp\"} 7\n")
                    != std::string::npos,
                "source sample verbatim");

    // 其余路径 → 404（exporter 非通用 HTTP 面）
    const HttpReply nf = http_fetch(http, port, "GET /nope HTTP/1.1\r\n\r\n");
    TEST_ASSERT(nf.connected, "second client connected");
    TEST_ASSERT(nf.body.rfind("HTTP/1.1 404 Not Found\r\n", 0) == 0,
                "404 on other path");

    // 应答完成后连接表清空、计数面正确
    for (int i = 0; i < 5; ++i) {
        http.pump(now_ms());
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    TEST_ASSERT(http.served() == 2, "two connections served");
    TEST_ASSERT(http.connections() == 0, "connections drained");
    TEST_ASSERT(http.aborted() == 0, "nothing aborted");
    TEST_ASSERT(http.refused() == 0, "nothing refused");
    http.stop();
    return true;
}

#endif // !_WIN32

} // namespace

int main() {
#if defined(_WIN32)
    if (!test_render_exposition()) {
        return 1;
    }
    std::cout << "MetricsPushTests: windows-scope cases passed" << std::endl;
    return 0;
#else
    const struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"render_exposition", test_render_exposition},
        {"snapshot_e2e", test_snapshot_e2e},
        {"periodic_latest_wins", test_periodic_latest_wins},
        {"down_no_queue", test_down_no_queue},
        {"hub_semantics", test_hub_semantics},
        {"http_metrics_endpoint", test_http_metrics_endpoint},
    };
    int failed = 0;
    for (const auto& c : cases) {
        if (!c.fn()) {
            std::cerr << "CASE FAILED: " << c.name << std::endl;
            ++failed;
        }
    }
    if (failed == 0) {
        std::cout << "MetricsPushTests: all " << (sizeof(cases) / sizeof(cases[0]))
                  << " cases passed" << std::endl;
        return 0;
    }
    std::cout << "MetricsPushTests: " << failed << " case(s) failed" << std::endl;
    return 1;
#endif
}
