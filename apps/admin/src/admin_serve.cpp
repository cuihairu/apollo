#include "admin/serve.hpp"

#include "admin/alert_notifier.hpp"
#include "admin/alert_webhook.hpp"
#include "admin/metrics_hub.hpp"
#include "admin/metrics_http.hpp"

#include "apollo/net/http/rest_client.h"
#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

#if !defined(_WIN32)

// exporter 受理面真实现（POSIX）：零内部线程——受理（link pump）与出口
// （HTTP pump）全在宿主 serve 循环拍内，单写者。
//
// 断连降级（logging.md §5）：admin 挂 = 各进程推面丢拍不崩（kDroppable
// 无队列，发送面已实测）；本端重启 = 对端退避重拨自然恢复，快照自含
// （最新者胜，不补投）；scrape 掉 = hub 已聚合，下拍重拉即恢复。

#ifndef APOLLO_ADMIN_CONTRACT_ROUTE_JSON
#define APOLLO_ADMIN_CONTRACT_ROUTE_JSON ""
#endif

namespace apollo::admin {

namespace {

namespace link = apollo::net::link;
namespace wire = apollo::net::wire;

using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) {
    return static_cast<std::uint16_t>(id);
}

volatile std::sig_atomic_t g_run = 1;

void on_signal(int) {
    g_run = 0;
}

// 单调毫秒（serve 泵节拍；与内核注入时钟同口径）
std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count());
}

}  // namespace

int run_serve(const ServeOptions& options) {
    const std::string route_path = !options.route_json.empty()
                                       ? options.route_json
                                       : APOLLO_ADMIN_CONTRACT_ROUTE_JSON;
    link::RouteTable table;
    {
        std::ifstream in(route_path, std::ios::binary);
        std::ostringstream buffer;
        if (!in) {
            std::cerr << "admin serve: route json not found: " << route_path
                      << std::endl;
            return 1;
        }
        buffer << in.rdbuf();
#if APOLLO_NET_LINK_HAS_JSON
        if (!link::load_route_table_json(buffer.str(), table)) {
            std::cerr << "admin serve: bad route json: " << route_path << std::endl;
            return 1;
        }
#else
        (void)table;
        std::cerr << "admin serve: link json support unavailable" << std::endl;
        return 1;
#endif
    }

    // 受理链路先于出口（9106 起得来、9107 落不下 = 装配错误显形）
    link::LinkManagerConfig mc;
    mc.self_name = "admin";
    mc.route_version = link::route_version_from_hash(table.schema_hash());
    link::LinkManager manager(mc);

    link::EndpointConfig ep;
    ep.name = "metrics";  // 各进程 active 端点的 dial 目标别名
    ep.host = options.host;
    ep.port = options.port;
    ep.role = link::EndpointRole::kPassive;
    if (!manager.add_endpoint(ep)) {
        std::cerr << "admin serve: cannot bind " << options.host << ":" << options.port
                  << std::endl;
        return 1;
    }

    MetricsHubConfig hub_config;
    hub_config.stale_ms = options.stale_ms;
    MetricsHub hub(hub_config);

    MetricsHttpConfig http_config;
    http_config.host = options.http_host;
    http_config.port = options.http_port;
    MetricsHttp http(http_config, hub);
    if (!http.start()) {
        std::cerr << "admin serve: cannot bind http " << options.http_host << ":"
                  << http_config.port << std::endl;
        return 1;
    }

    // 告警 notifier 最小面（§5.2 层 B 内核）：规则 = APOLLO_ALERT_RULES
    // env（缺省空 = 不判定零开销）；出站缺省 cat=alert 结构化行（本地留
    // 证）。webhook 出站面（§5.2 ①④⑤）：APOLLO_WEBHOOK_URL 装配即启用，
    // 双出口——事件正文照常落本地行 + webhook 入队（worker 线程出站，
    // serve 拍永不阻塞 HTTP）；密钥/env 不落日志。
    AlertNotifier notifier;
    notifier.set_rules(parse_alert_rules(std::getenv("APOLLO_ALERT_RULES")
                                             ? std::getenv("APOLLO_ALERT_RULES")
                                             : ""));

    const AlertWebhookConfig webhook_config = parse_webhook_config();
    std::unique_ptr<AlertWebhook> webhook;
    if (!webhook_config.url.empty()) {
        webhook = std::make_unique<AlertWebhook>(webhook_config);
        webhook->start();
        notifier.set_sink([&webhook](const AlertEvent& event) {
            std::cerr << alert_log_line(event) << std::endl;  // 本地留证照常
            webhook->send(event);
        });
        std::cerr << "alert webhook enabled (target host: "
                  << apollo::net::http::RestTemplate::hostOf(webhook_config.url)
                  << ")" << std::endl;  // 只记 host——④密钥/token 不进日志
    }

    // 受理装配随 peer 上线（fan-in 面）；handler 只挂 931——快照原文入库
    //（proc/body 保真），聚合/渲染在出口面
    std::atomic<std::uint64_t> received{0};
    std::atomic<std::uint64_t> rejected{0};
    manager.set_peer_listener([&](link::InterServerLink& peer) {
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
                                       ++rejected;
                                       return;
                                   }
                                   const auto t = now_ms();
                                   notifier.observe(fields[0].text, fields[1].text, t);
                                   hub.ingest(std::move(fields[0].text),
                                               std::move(fields[1].text), t);
                                   ++received;
                               });
    });

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    std::cout << "admin serve: metrics link on " << options.host << ":"
              << manager.endpoint_port("metrics")
              << ", http on " << options.http_host << ":" << http.port()
              << std::endl;
    while (g_run != 0) {
        manager.pump(now_ms());
        http.pump(now_ms());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    http.stop();

    std::cerr << "serve: received=" << received.load()
              << " rejected=" << rejected.load()
              << " procs=" << hub.procs()
              << " hub_rejected=" << hub.rejected()
              << " http_served=" << http.served()
              << " http_aborted=" << http.aborted() << std::endl;
    return 0;
}

}  // namespace apollo::admin

#else  // _WIN32

#include <iostream>

namespace apollo::admin {

int run_serve(const ServeOptions& /*options*/) {
    // link TCP 底座不进本批（POSIX only）：serve 在 Windows 不可用，
    // 聚合/渲染面不受影响。
    std::cerr << "admin serve: requires the link TCP base (POSIX only)"
              << std::endl;
    return 1;
}

}  // namespace apollo::admin

#endif  // !_WIN32
