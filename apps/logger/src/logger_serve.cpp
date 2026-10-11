#include "logger/serve.hpp"

#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"
#include "logger/structured_line.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

#if !defined(_WIN32)

// collector 受理面真实现（POSIX）：零内部线程——受理/归档全在宿主 serve
// 循环拍内（manager pump → 分发 handler → 原行落盘，单写者）。
//
// 断连降级（logging.md §5）：collector 挂 = 各进程只写本地（发送面有界
// 队列缓冲 + 重连补投）；本端重启 = 对端退避重拨自动恢复，归档续写。

#ifndef APOLLO_LOGGER_CONTRACT_ROUTE_JSON
#define APOLLO_LOGGER_CONTRACT_ROUTE_JSON ""
#endif

namespace fs = std::filesystem;

namespace logger {

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

} // namespace

int run_serve(const ServeOptions& options) {
    const std::string route_path =
        !options.route_json.empty() ? options.route_json : APOLLO_LOGGER_CONTRACT_ROUTE_JSON;
    link::RouteTable table;
    {
        std::ifstream in(route_path, std::ios::binary);
        std::ostringstream buffer;
        if (!in) {
            std::cerr << "logger serve: route json not found: " << route_path << std::endl;
            return 1;
        }
        buffer << in.rdbuf();
#if APOLLO_NET_LINK_HAS_JSON
        if (!link::load_route_table_json(buffer.str(), table)) {
            std::cerr << "logger serve: bad route json: " << route_path << std::endl;
            return 1;
        }
#else
        (void)table;
        std::cerr << "logger serve: link json support unavailable" << std::endl;
        return 1;
#endif
    }

    // 归档先于链路（受理面起得来、落不下盘 = 装配错误显形）
    std::error_code ec;
    fs::create_directories(options.archive_dir, ec);
    std::ofstream archive(fs::path(options.archive_dir) / "collector.log",
                          std::ios::app | std::ios::binary);
    if (!archive) {
        std::cerr << "logger serve: cannot open archive file in " << options.archive_dir
                  << std::endl;
        return 1;
    }

    link::LinkManagerConfig mc;
    mc.self_name = "logger";
    mc.route_version = link::route_version_from_hash(table.schema_hash());
    link::LinkManager manager(mc);

    link::EndpointConfig ep;
    ep.name = "apps";  // 本端监听键（对端以各自自报名入册）
    ep.host = options.host;
    ep.port = options.port;
    ep.role = link::EndpointRole::kPassive;
    if (!manager.add_endpoint(ep)) {
        std::cerr << "logger serve: cannot bind " << options.host << ":" << options.port
                  << std::endl;
        return 1;
    }

    // 受理装配随 peer 上线（fan-out 面）；handler 只挂 930——行原文归档
    //（保真同 follow 透传），解析仅为记账（skipped = 非结构化行）。
    std::atomic<std::uint64_t> received{0};
    std::atomic<std::uint64_t> rejected{0};
    std::atomic<std::uint64_t> skipped{0};
    manager.set_peer_listener([&](link::InterServerLink& peer) {
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
                                       ++rejected;
                                       return;
                                   }
                                   const std::string line = std::move(fields[0].text);
                                   StructuredLine record;
                                   if (!parse_structured_line(line, &record)) {
                                       ++skipped;
                                   }
                                   archive << line << '\n';
                                   ++received;
                               });
    });

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    std::cout << "logger serve: listening on " << options.host << ":" << manager.endpoint_port("apps")
              << ", archive " << options.archive_dir << "/collector.log" << std::endl;
    while (g_run != 0) {
        manager.pump(now_ms());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    archive.flush();

    std::cerr << "serve: received=" << received.load() << " rejected=" << rejected.load()
              << " skipped=" << skipped.load() << std::endl;
    return 0;
}

} // namespace logger

#else // _WIN32

#include <iostream>

namespace logger {

int run_serve(const ServeOptions& /*options*/) {
    // link TCP 底座不进本批（POSIX only）：serve 在 Windows 不可用，
    // 检索面（scan/follow）不受影响。
    std::cerr << "logger serve: requires the link TCP base (POSIX only)" << std::endl;
    return 1;
}

} // namespace logger

#endif // !_WIN32
