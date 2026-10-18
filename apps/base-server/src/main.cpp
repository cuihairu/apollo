#include "database/client.hpp"
#include "player_store/player_store.hpp"
#include "apollo/game/session/fleet_recovery.hpp"
#include "apollo/net/discovery/udp_transport.hpp"
#include "apollo/runtime/crash_capture.hpp"
#include "apollo/core/log/log_manager.h"

#include <charconv>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
volatile std::sig_atomic_t run = 1;
void onSignal(int) { run = 0; }
std::uint16_t port(const std::string& text, bool ephemeral = false) {
    unsigned value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || (!value && !ephemeral) || value > 65535)
        throw std::runtime_error("invalid port");
    return static_cast<std::uint16_t>(value);
}
std::uint64_t positive(const std::string& text, const char* what) {
    unsigned long long value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value == 0)
        throw std::runtime_error(std::string("invalid ") + what);
    return value;
}
}

int main(int argc, char** argv) {
    try {
        player_store::PlayerStoreConfig config;
        config.host = "127.0.0.1";
        config.m1Enabled = true;
        config.m1Port = 9201;
        config.m1SelfName = "baseServer";
        config.dataDir.clear();
        config.journalPath.clear();
        config.m1RouteJson = APOLLO_SERVICE_ROUTE_JSON;
        database::ClientConfig storage;
        storage.route_json = config.m1RouteJson;
        // 锚点全量重报面（L19 资料归属切换：自 WorldApp 迁入）。
        // --report-port 0 = 关；开启时 component 号必给（wire 拒 0）。
        std::string reportHost = "127.0.0.1";
        std::uint16_t reportPort = 0;
        std::uint64_t reportComponent = 0;
        std::uint32_t reportZone = 0;
        std::uint64_t reportIntervalMs = 0;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                std::cout << "baseServer [--host IP] [--port N] [--id NAME] "
                             "[--database-host IP] [--database-port N] [--database-id NAME] [--route-json PATH] "
                             "[--report-host IP] [--report-port N] [--report-component-id N] "
                             "[--report-zone-id N] [--report-interval-ms N]\n";
                return 0;
            }
            if (i + 1 == argc) throw std::runtime_error("missing option value");
            const std::string value = argv[++i];
            if (arg == "--host") config.host = value;
            else if (arg == "--port") config.m1Port = port(value, true);
            else if (arg == "--id") config.m1SelfName = storage.self_name = value;
            else if (arg == "--database-host") storage.host = value;
            else if (arg == "--database-port") storage.port = port(value);
            else if (arg == "--database-id") storage.peer_name = value;
            else if (arg == "--route-json") config.m1RouteJson = storage.route_json = value;
            else if (arg == "--report-host") reportHost = value;
            else if (arg == "--report-port") reportPort = port(value, true);
            else if (arg == "--report-component-id") reportComponent = positive(value, "report component id");
            else if (arg == "--report-zone-id") reportZone = static_cast<std::uint32_t>(positive(value, "report zone id"));
            else if (arg == "--report-interval-ms") reportIntervalMs = positive(value, "report interval");
            else throw std::runtime_error("unknown option: " + arg);
        }
        if (reportPort != 0 && reportComponent == 0)
            throw std::runtime_error("--report-component-id required when fleet reporting enabled");
        apollo::core::log::LogManagerConfig logging;
        logging.processIdentity = config.m1SelfName;
        apollo::core::log::global_log_manager().initialize(logging);
        apollo::runtime::init_crash_capture(argc, argv, "baseServer");
        std::signal(SIGINT, onSignal);
        std::signal(SIGTERM, onSignal);
        player_store::PlayerStore base(config, database::makeClient(storage));
        base.start();
        if (!base.m1_listen_port()) throw std::runtime_error("Base listener unavailable");
        std::cout << "baseServer ready port=" << base.m1_listen_port() << std::endl;

        // 恢复相位全量重报（P3-1 增量③）：启动即报 + 周期刷新（幂等
        // intake；interval 0 = 仅启动期一报）。承载域 zone 由部署给定。
        apollo::net::discovery::UdpBeaconTransport reportTransport;
        std::unique_ptr<apollo::game::session::FleetReporter> reporter;
        if (reportPort != 0) {
            if (!reportTransport.open())
                throw std::runtime_error("Base fleet report transport unavailable");
            reporter = std::make_unique<apollo::game::session::FleetReporter>(
                reportComponent, reportZone,
                [&reportTransport, &reportHost, reportPort](const std::uint8_t* data, std::size_t len) {
                    return reportTransport.send_bytes(reportHost, reportPort, data, len);
                });
            (void)reporter->report_full(
                player_store::collect_mirror_entries(base, reportZone));
            std::cout << "baseServer fleet report: component=" << reportComponent
                      << " zone=" << reportZone << " -> " << reportHost << ":"
                      << reportPort << " (interval " << reportIntervalMs << "ms)" << std::endl;
        }
        auto lastReport = std::chrono::steady_clock::now();
        while (run) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            if (reporter && reportIntervalMs != 0) {
                const auto since = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::steady_clock::now() - lastReport)
                                       .count();
                if (since >= static_cast<std::int64_t>(reportIntervalMs)) {
                    lastReport = std::chrono::steady_clock::now();
                    (void)reporter->report_full(
                        player_store::collect_mirror_entries(base, reportZone));
                }
            }
        }
        reporter.reset();
        reportTransport.close();
        base.stop();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "baseServer startup/service failure: " << error.what() << '\n';
        return 1;
    }
}
