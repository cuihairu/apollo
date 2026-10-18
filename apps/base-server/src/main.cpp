#include "database/client.hpp"
#include "player_store/player_store.hpp"
#include "apollo/runtime/crash_capture.hpp"
#include "apollo/core/log/log_manager.h"

#include <charconv>
#include <chrono>
#include <csignal>
#include <iostream>
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
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                std::cout << "baseServer [--host IP] [--port N] [--id NAME] "
                             "[--database-host IP] [--database-port N] [--database-id NAME] [--route-json PATH]\n";
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
            else throw std::runtime_error("unknown option: " + arg);
        }
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
        while (run) std::this_thread::sleep_for(std::chrono::milliseconds(20));
        base.stop();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "baseServer startup/service failure: " << error.what() << '\n';
        return 1;
    }
}
