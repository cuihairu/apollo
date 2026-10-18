#include "database/server.hpp"
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
        database::ServerConfig config;
        config.route_json = APOLLO_SERVICE_ROUTE_JSON;
        config.storage.dataDir = "data/database/players";
        config.storage.journalPath = "data/database/persist.log";
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                std::cout << "database [--host IP] [--port N] [--id NAME] "
                             "[--data-dir PATH] [--journal PATH] [--route-json PATH]\n";
                return 0;
            }
            if (i + 1 == argc) throw std::runtime_error("missing option value");
            const std::string value = argv[++i];
            if (arg == "--host") config.host = value;
            else if (arg == "--port") config.port = port(value, true);
            else if (arg == "--id") config.self_name = value;
            else if (arg == "--data-dir") config.storage.dataDir = value;
            else if (arg == "--journal") config.storage.journalPath = value;
            else if (arg == "--route-json") config.route_json = value;
            else throw std::runtime_error("unknown option: " + arg);
        }
        apollo::core::log::LogManagerConfig logging;
        logging.processIdentity = config.self_name;
        apollo::core::log::global_log_manager().initialize(logging);
        apollo::runtime::init_crash_capture(argc, argv, "database");
        std::signal(SIGINT, onSignal);
        std::signal(SIGTERM, onSignal);
        database::Server server(config);
        if (!server.start()) throw std::runtime_error("storage recovery/listener unavailable");
        std::cout << "database ready port=" << server.port() << std::endl;
        while (run) std::this_thread::sleep_for(std::chrono::milliseconds(20));
        server.stop();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "database startup/service failure: " << error.what() << '\n';
        return 1;
    }
}
