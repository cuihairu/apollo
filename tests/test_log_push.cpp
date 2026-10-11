// L4 log push 通道测试（LogPushTests）。
//
// 覆盖（logging.md §5 collector 形态 + 断连降级主语义）：
//   1. push_e2e：collector（LinkManager passive + 930 收集）↔ LogPushClient
//      多行 round-trip（行内容保真）；
//   2. disconnect_buffering：collector 缺席期推 N 行（有界队列缓冲），
//      collector 上线后重连补投——「收集永不构成运行依赖」；
//   3. queue_cap：缓冲顶格丢新 + 计数显形；
//   4. push_appender：LogManagerConfig.pushSink 接线（结构化行出回 +
//      proc 键注入 + 后建 logger 复制推面）。

#include "apollo/net/logpush/log_push_client.hpp"
#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"
#include "apollo/core/log/log_manager.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
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
namespace logpush = apollo::net::logpush;

using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) {
    return static_cast<std::uint16_t>(id);
}

std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count());
}

#if !defined(_WIN32)

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

// collector 侧受集体：passive 端点 + 930 handler 收行（原行保真记账）
struct Collector {
    link::RouteTable table;
    std::unique_ptr<link::LinkManager> manager;
    std::vector<std::string> lines;
    std::mutex lines_mutex;

    bool start(std::uint16_t port = 0) {
        if (!load_table(table)) {
            return false;
        }
        link::LinkManagerConfig mc;
        mc.self_name = "logger";
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
            dispatcher.set_handler(id_of(MsgId::logger_line_push),
                                   [&](std::uint16_t, const std::uint8_t* data,
                                       std::size_t size) {
                                       std::vector<wire::FieldValue> fields;
                                       if (!wire::decode_payload(
                                               id_of(MsgId::logger_line_push), data, size,
                                               fields) ||
                                           fields.empty()) {
                                           return;
                                       }
                                       std::lock_guard<std::mutex> lock(lines_mutex);
                                       lines.push_back(std::move(fields[0].text));
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
        std::lock_guard<std::mutex> lock(lines_mutex);
        return lines.size();
    }

    std::vector<std::string> copy() {
        std::lock_guard<std::mutex> lock(lines_mutex);
        return lines;
    }
};

// 双面泵到条件成立（10ms 拍，秒级预算）
template <typename Pred>
bool pump_until(Collector& collector, logpush::LogPushClient& client, Pred&& done,
                int timeout_ms = 5000) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        collector.pump();
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    collector.pump();
    return done();
}

bool test_push_e2e() {
    Collector collector;
    TEST_ASSERT(collector.start(), "collector assembled");
    TEST_ASSERT(collector.port() != 0, "ephemeral port observed");

    logpush::LogPushClientConfig cfg;
    cfg.port = collector.port();
    cfg.route_json = APOLLO_CONTRACT_ROUTE_JSON;
    cfg.self_name = "app_probe";
    logpush::LogPushClient client(cfg);
    TEST_ASSERT(client.start(), "client started");

    TEST_ASSERT(client.push("ts=1 level=INFO proc=app_probe tick=0 cat=a msg=one"), "push 1");
    TEST_ASSERT(client.push("ts=2 level=WARN proc=app_probe tick=1 cat=b msg=two"), "push 2");
    TEST_ASSERT(client.push("ts=3 level=ERROR proc=app_probe tick=2 cat=c msg=\"three x\""),
                "push 3");

    TEST_ASSERT(pump_until(collector, client, [&] { return collector.count() == 3; }),
                "three lines delivered");
    TEST_ASSERT(client.connected(), "client link up");
    TEST_ASSERT(client.pushed() == 3, "three pushed");

    const auto lines = collector.copy();
    TEST_ASSERT(lines.size() == 3, "three recorded");
    TEST_ASSERT(lines[0] == "ts=1 level=INFO proc=app_probe tick=0 cat=a msg=one",
                "line 1 verbatim");
    TEST_ASSERT(lines[2] == "ts=3 level=ERROR proc=app_probe tick=2 cat=c msg=\"three x\"",
                "line 3 verbatim");

    client.stop();
    return true;
}

bool test_disconnect_buffering() {
    // collector#1 只用来占口拿端口，随即拆走（client 拨号期无人受理）
    std::uint16_t port = 0;
    {
        Collector placeholder;
        TEST_ASSERT(placeholder.start(), "placeholder assembled");
        port = placeholder.port();
        TEST_ASSERT(port != 0, "port picked");
    }

    logpush::LogPushClientConfig cfg;
    cfg.port = port;
    cfg.route_json = APOLLO_CONTRACT_ROUTE_JSON;
    cfg.self_name = "app_probe";
    cfg.pump_interval_ms = 20;
    logpush::LogPushClient client(cfg);
    TEST_ASSERT(client.start(), "client started (collector absent)");

    // 缺席期推入：全部入队（本地文件真相源语义由上层 FileAppender 承担，
    // 本面只证缓冲不丢账）
    TEST_ASSERT(client.push("ts=10 level=INFO proc=app_probe tick=0 cat=a msg=buffered1"),
                "push while down 1");
    TEST_ASSERT(client.push("ts=11 level=INFO proc=app_probe tick=0 cat=a msg=buffered2"),
                "push while down 2");
    TEST_ASSERT(client.push("ts=12 level=INFO proc=app_probe tick=0 cat=a msg=buffered3"),
                "push while down 3");
    TEST_ASSERT(client.queued() == 3, "three buffered");

    // collector 上线（同口显式 bind）→ client 退避重拨 → 补投
    Collector collector;
    TEST_ASSERT(collector.start(port), "collector rebound on same port");
    TEST_ASSERT(pump_until(collector, client, [&] { return collector.count() == 3; }),
                "buffered lines flushed after reconnect");
    const auto lines = collector.copy();
    TEST_ASSERT(lines.size() == 3 && lines[0].find("buffered1") != std::string::npos &&
                    lines[2].find("buffered3") != std::string::npos,
                "flush in order");

    client.stop();
    return true;
}

bool test_queue_cap() {
    // 不 start：顶格/丢新计数全在 push() 入队路径，无泵线程竞态
    // （start 失败降级态 = 同一调用面，push 照常缓冲）
    logpush::LogPushClientConfig cfg;
    cfg.port = 1;  // 无人受理（本面不触网）
    cfg.route_json = APOLLO_CONTRACT_ROUTE_JSON;
    cfg.self_name = "app_probe";
    cfg.queue_cap = 2;
    logpush::LogPushClient client(cfg);

    TEST_ASSERT(client.push("ts=1 level=INFO proc=p tick=0 cat=a msg=l1"), "cap push 1");
    TEST_ASSERT(client.push("ts=2 level=INFO proc=p tick=0 cat=a msg=l2"), "cap push 2");
    TEST_ASSERT(!client.push("ts=3 level=INFO proc=p tick=0 cat=a msg=l3"),
                "cap push 3 dropped");
    TEST_ASSERT(client.queued() == 2, "queue at cap");
    TEST_ASSERT(client.dropped() >= 1, "overflow counted");

    return true;
}

#endif // !_WIN32

bool test_push_appender() {
    // LogManagerConfig.pushSink 接线：结构化行出回 + proc 注入；后建
    // logger 经 appender 链复制随推面（ LogManager 全局单例——本二进制
    // 一次初始化）。
    auto& manager = apollo::core::log::global_log_manager();
    apollo::core::log::LogManagerConfig cfg;
    cfg.consoleEnabled = false;
    cfg.fileEnabled = false;
    cfg.processIdentity = "probe";
    std::vector<std::string> lines;
    std::mutex lines_mutex;
    cfg.pushSink = [&](const std::string& line) {
        std::lock_guard<std::mutex> lock(lines_mutex);
        lines.push_back(line);
    };
    manager.initialize(cfg);

    auto logger = manager.createLogger("probe_cat");
    TEST_ASSERT(logger != nullptr, "logger created");
    logger->log(apollo::core::log::LogRecord(apollo::core::log::LogLevel::Warning,
                                             std::string("hello push"),
                                             std::string("probe_cat")));

    // push 走 appender 同步路径：此刻已可读
    std::vector<std::string> got;
    {
        std::lock_guard<std::mutex> lock(lines_mutex);
        got = lines;
    }
    TEST_ASSERT(got.size() == 1, "one structured line pushed");
    TEST_ASSERT(got[0].find("proc=probe") != std::string::npos, "proc key injected");
    TEST_ASSERT(got[0].find("cat=probe_cat") != std::string::npos, "cat key present");
    TEST_ASSERT(got[0].find("level=WARN") != std::string::npos, "level key present");
    TEST_ASSERT(got[0].find("msg=") != std::string::npos, "msg key present");

    manager.shutdown();
    return true;
}

} // namespace

int main() {
#if defined(_WIN32)
    // link TCP 底座 POSIX only：Windows 只跑 appender 接线面
    if (!test_push_appender()) {
        return 1;
    }
    std::cout << "LogPushTests: windows-scope cases passed" << std::endl;
    return 0;
#else
    const struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"push_e2e", test_push_e2e},
        {"disconnect_buffering", test_disconnect_buffering},
        {"queue_cap", test_queue_cap},
        {"push_appender", test_push_appender},
    };
    int failed = 0;
    for (const auto& c : cases) {
        if (!c.fn()) {
            std::cerr << "CASE FAILED: " << c.name << std::endl;
            ++failed;
        }
    }
    if (failed == 0) {
        std::cout << "LogPushTests: all " << (sizeof(cases) / sizeof(cases[0]))
                  << " cases passed" << std::endl;
        return 0;
    }
    std::cout << "LogPushTests: " << failed << " case(s) failed" << std::endl;
    return 1;
#endif
}
