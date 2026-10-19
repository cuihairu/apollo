#include "apollo/runtime/crash_capture.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/net/tls/tls_transport.hpp"
#include "interfaces/http_ingress.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace interfaces;

namespace {

apollo::core::log::Logger& glog() {
    return *apollo::core::log::global_log_manager().createLogger("interfaces");
}

// 信号路径只翻标志（logger 面带锁、pump 不可重入——stop 归主线程收尾）
std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    // 信号上下文保持直写 console（同 gateway main 纪律）
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

struct InterfacesConfig {
    std::string host = "0.0.0.0";
    std::uint16_t port = 8080;     // 明文 HTTP 端口
    std::uint16_t httpsPort = 0;   // 0 = 不开 HTTPS 监听
    std::string tlsCertPath;
    std::string tlsKeyPath;
    std::string wsPath = "/ws";    // 专用升级路由
    std::size_t maxConnections = 0;
};

InterfacesConfig loadConfig(int argc, char* argv[]) {
    InterfacesConfig config;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<std::uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];
        } else if (arg == "--https-port" && i + 1 < argc) {
            config.httpsPort = static_cast<std::uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--tls-cert" && i + 1 < argc) {
            config.tlsCertPath = argv[++i];
        } else if (arg == "--tls-key" && i + 1 < argc) {
            config.tlsKeyPath = argv[++i];
        } else if (arg == "--ws-path" && i + 1 < argc) {
            config.wsPath = argv[++i];
        } else if (arg == "--max-connections" && i + 1 < argc) {
            config.maxConnections = static_cast<std::size_t>(std::atoi(argv[++i]));
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --host <host>            Listen address (default: 0.0.0.0)\n"
                      << "  --port <port>            HTTP listen port (default: 8080)\n"
                      << "  --https-port <port>      HTTPS listen port (0 = off, default: 0)\n"
                      << "  --tls-cert <file>        HTTPS PEM cert chain (required by --https-port)\n"
                      << "  --tls-key <file>         HTTPS PEM private key (required by --https-port)\n"
                      << "  --ws-path <path>         Dedicated websocket upgrade route (default: /ws)\n"
                      << "  --max-connections <num>   Max connections per listener (0 = unlimited, default: 0)\n"
                      << "  --help, -h                 Show this help\n";
            std::exit(0);
        }
    }
    return config;
}

// 演示路由（L50 平台回调接线替换此面）：健康检查 + 回显；ws 侧专用升级
// 路由走 echo 生命周期，不进 HTTP handler（todo L45 路由隔离验收）。
void install_demo_routes(HttpIngressServer& server, const std::string& ws_path) {
    server.setHttpRoute("/health", [](const HttpRequest&) {
        HttpResponse response;
        const std::string body = "{\"status\":\"ok\"}\n";
        response.body.assign(body.begin(), body.end());
        return response;
    });
    server.setHttpRoute("/echo", [](const HttpRequest& request) {
        HttpResponse response;
        response.body = request.body;  // POST 体原样回显（预算 1 MiB 内）
        return response;
    });
    server.setWsRoute(ws_path);
}

// 日志接线（ADR-013 L1+L2）：console 人读 + 本地结构化文件真相源并行。
apollo::core::log::LoggerPtr initLogging() {
    auto& logs = apollo::core::log::global_log_manager();
    apollo::core::log::LogManagerConfig config;
    config.processIdentity = "interfaces";
    config.fileEnabled = true;
    config.fileConfig.directory = "log";
    config.fileConfig.baseName = "interfaces";
    config.fileConfig.structuredOutput = true;
    logs.initialize(config);
    return logs.createLogger("interfaces");
}

// 观测面（日志即受理链的 M1 骨架证据；L50 换成有界受理链）。
struct DemoObserver final : IHttpIngressObserver {
    void onHttpHandled(ConnectionID id, const HttpRequest& request, int status) override {
        glog().info("http " + request.method + " " + request.path
                    + " -> " + std::to_string(status) + " (id="
                    + std::to_string(id) + ", body="
                    + std::to_string(request.body.size()) + "B)");
    }
    void onWsOpened(ConnectionID id, const ClientEndpoint& endpoint) override {
        glog().info("ws opened id=" + std::to_string(id) + " peer="
                    + endpoint.address + ":" + std::to_string(endpoint.port));
    }
    void onWsBinary(ConnectionID id, std::span<const std::uint8_t> payload) override {
        glog().info("ws binary frame id=" + std::to_string(id)
                    + " len=" + std::to_string(payload.size()));
    }
    void onWsClosed(ConnectionID id, DisconnectReason reason) override {
        glog().info("ws closed id=" + std::to_string(id)
                    + " reason=" + std::to_string(static_cast<int>(reason)));
    }
};

} // namespace

int main(int argc, char* argv[]) {
    const auto logger = initLogging();
    apollo::runtime::init_crash_capture(argc, argv, "interfaces");
    std::cout << "======================================" << std::endl;
    std::cout << "        Apollo Interfaces             " << std::endl;
    std::cout << "======================================" << std::endl;

    const InterfacesConfig config = loadConfig(argc, argv);
    logger->info("Configuration:");
    logger->info("  HTTP: " + config.host + ":" + std::to_string(config.port));
    logger->info("  HTTPS: " + (config.httpsPort == 0
                                    ? "off"
                                    : "port " + std::to_string(config.httpsPort)));
    logger->info("  ws upgrade route: " + config.wsPath);

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
#ifdef _WIN32
    std::signal(SIGBREAK, signalHandler);
#else
    std::signal(SIGHUP, signalHandler);
#endif

    DemoObserver observer;

    // 并行监听 = 独立实例 + 互斥号段（号段步进与 gateway 入口 fan-in 同口径）。
    constexpr std::uint64_t kIdStride = std::uint64_t{1} << 40;

    try {
        HttpIngressConfig http_config;
        http_config.host = config.host;
        http_config.port = config.port;
        http_config.maxConnections = config.maxConnections;
        http_config.connectionIdBase = 0;

        std::unique_ptr<apollo::net::tls::TlsServerContext> tls_context;
        HttpIngressConfig https_config;
        https_config.host = config.host;
        https_config.port = config.httpsPort;
        https_config.maxConnections = config.maxConnections;
        https_config.connectionIdBase = kIdStride;
        if (config.httpsPort != 0) {
            if (config.tlsCertPath.empty() || config.tlsKeyPath.empty()) {
                throw std::runtime_error(
                    "https ingress enabled but tls cert/key not configured "
                    "(--tls-cert/--tls-key)");
            }
            tls_context = std::make_unique<apollo::net::tls::TlsServerContext>();
            const auto load_result = tls_context->load(config.tlsCertPath, config.tlsKeyPath);
            if (!load_result.ok) {
                throw std::runtime_error("https tls context load failed: "
                                         + load_result.error);
            }
            https_config.tls = tls_context.get();
        }

        auto http_server = makeHttpIngressServer(http_config);
        install_demo_routes(*http_server, config.wsPath);
        http_server->setObserver(&observer);
        http_server->start();

        std::unique_ptr<HttpIngressServer> https_server;
        if (config.httpsPort != 0) {
            https_server = makeHttpIngressServer(https_config);
            install_demo_routes(*https_server, config.wsPath);
            https_server->setObserver(&observer);
            https_server->start();
        }

        logger->info("HTTP listening on " + config.host + ":"
                     + std::to_string(http_server->listenPort()));
        if (https_server) {
            logger->info("HTTPS listening on " + config.host + ":"
                         + std::to_string(https_server->listenPort()));
        }
        logger->info("Server is running. Press Ctrl+C to stop.");

        while (g_running) {
            http_server->pump();
            if (https_server) {
                https_server->pump();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (https_server) {
            https_server->stop();
        }
        http_server->stop();
        logger->info("Server stopped.");

    } catch (const std::exception& e) {
        logger->error(std::string("Error: ") + e.what());
        return 1;
    }

    return 0;
}
