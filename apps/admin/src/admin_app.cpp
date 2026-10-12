// AdminApp 实现：serve 受面子命令（metrics exporter，logging.md §5.1）。

#include "admin/admin_app.hpp"
#include "admin/serve.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace apollo::admin {

namespace {

const char* kUsage =
    "Usage: admin serve [options]\n"
    "Serve options (metrics exporter, logging.md §5.1):\n"
    "  --host <addr>      Metrics link listen host (default: 0.0.0.0)\n"
    "  --port <port>      Metrics link listen port (default: 9106)\n"
    "  --http-host <addr> Exporter HTTP listen host (default: 0.0.0.0)\n"
    "  --http-port <port> Exporter HTTP listen port (default: 9107)\n"
    "  --route-json <p>   Contract route json (default: build-time path)\n"
    "  --stale-ms <n>     proc_up staleness window ms (default: 30000)\n"
    "  --help, -h         Show this help\n";

}  // namespace

int AdminApp::run(int argc, char* argv[]) {
    ServeOptions options;
    bool serve = false;
    bool help = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "serve") {
            serve = true;
        } else if (arg == "--host" && i + 1 < argc) {
            options.host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            options.port = static_cast<std::uint16_t>(
                std::strtoul(argv[++i], nullptr, 10));
        } else if (arg == "--http-host" && i + 1 < argc) {
            options.http_host = argv[++i];
        } else if (arg == "--http-port" && i + 1 < argc) {
            options.http_port = static_cast<std::uint16_t>(
                std::strtoul(argv[++i], nullptr, 10));
        } else if (arg == "--route-json" && i + 1 < argc) {
            options.route_json = argv[++i];
        } else if (arg == "--stale-ms" && i + 1 < argc) {
            options.stale_ms = static_cast<std::uint64_t>(
                std::strtoull(argv[++i], nullptr, 10));
        } else if (arg == "--help" || arg == "-h") {
            help = true;
        }
    }
    if (help) {
        std::cout << kUsage;
        return 0;
    }
    if (!serve) {
        std::cout << kUsage;
        return 1;
    }
    try {
        return run_serve(options);
    } catch (const std::exception& e) {
        std::cerr << "admin: " << e.what() << std::endl;
        return 1;
    }
}

}  // namespace apollo::admin
