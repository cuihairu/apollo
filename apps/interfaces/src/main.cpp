#include "apollo/runtime/crash_capture.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/net/tls/tls_transport.hpp"
#include "interfaces/callback_verifier.hpp"
#include "interfaces/callback_chain.hpp"
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
    std::uint32_t requestTimeoutMs = 0;   // 0 = 不限；>0 = 请求超时 408
    std::size_t callbackCap = 1024;       // 回调链顶格拒收线
    std::size_t callbackMaxPump = 64;     // 回调链单拍消费上限
    std::uint32_t callbackExpireMs = 0;   // 0 = 不过期；>0 = 滞留超期丢弃
    std::string callbackSecret;           // 空 = 验签 gate 拒绝回调
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
        } else if (arg == "--request-timeout" && i + 1 < argc) {
            config.requestTimeoutMs = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--callback-cap" && i + 1 < argc) {
            config.callbackCap = static_cast<std::size_t>(std::atoi(argv[++i]));
        } else if (arg == "--callback-max-pump" && i + 1 < argc) {
            config.callbackMaxPump = static_cast<std::size_t>(std::atoi(argv[++i]));
        } else if (arg == "--callback-expire-ms" && i + 1 < argc) {
            config.callbackExpireMs = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--callback-secret" && i + 1 < argc) {
            config.callbackSecret = argv[++i];
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
                      << "  --request-timeout <ms>    Request completion budget (0 = unlimited, default: 0)\n"
                      << "  --callback-cap <num>      Callback chain capacity before 429 (default: 1024)\n"
                      << "  --callback-max-pump <num> Callback drain budget per tick (default: 64)\n"
                      << "  --callback-expire-ms <ms> Callback staleness budget before drop (0 = never, default: 0)\n"
                      << "  --callback-secret <secret> Enable HMAC-SHA256 callback verification\n"
                      << "  --help, -h                 Show this help\n";
            std::exit(0);
        }
    }
    return config;
}

// 演示路由（健康检查 + 回显；ws 侧专用升级路由走 echo 生命周期，不进
// HTTP handler——todo L45 路由隔离验收）。
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

// 平台回调路由（todo L62）：验签 gate 在快照入链前执行。签名绑定
// raw_request 逐字节原始帧；校验失败/缺声明体长 = 401/403 且不进入链。
void install_callback_route(HttpIngressServer& server, CallbackChain& chain,
                            const CallbackVerifier& verifier) {
    server.setHttpRoute("/callbacks", [&chain, &verifier](const HttpRequest& request) {
        HttpResponse response;
        const auto verified = verifier.verify(request);
        if (!verified.valid) {
            response.status = verified.status;
            response.reason = verified.status == 401 ? "Unauthorized" : "Forbidden";
            const std::string body =
                verified.status == 401 ? "callback signature mismatch\n"
                                       : "callback signature malformed\n";
            response.body.assign(body.begin(), body.end());
            return response;
        }
        const auto result = chain.push(snapshot(request));
        response.status = result.accepted ? 202 : 429;
        response.reason = result.accepted ? "Accepted" : "Too Many Requests";
        const std::string body = result.accepted ? "accepted\n"
                                                 : "callback backlog full\n";
        response.body.assign(body.begin(), body.end());
        return response;
    });
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
        http_config.requestTimeout = std::chrono::milliseconds(config.requestTimeoutMs);

        // 平台回调受理链（有界单写者）：入口泵入队 + 202/429 回执，drain 在
        // 主循环单写者上消费（业务执行与入口泵同宿主但不同步进——见 pump 循环）。
        CallbackChainConfig chain_config;
        chain_config.cap = config.callbackCap;
        chain_config.high_watermark =
            (config.callbackCap * 3) / 4 > 0 ? (config.callbackCap * 3) / 4 : 1;
        chain_config.max_per_pump = config.callbackMaxPump;
        chain_config.expire_after = std::chrono::milliseconds(config.callbackExpireMs);
        CallbackChain callback_chain(chain_config);
        const CallbackVerifier callback_verifier(config.callbackSecret);

        HttpIngressConfig https_config;
        https_config.host = config.host;
        https_config.port = config.httpsPort;
        https_config.maxConnections = config.maxConnections;
        https_config.connectionIdBase = kIdStride;
        https_config.requestTimeout = std::chrono::milliseconds(config.requestTimeoutMs);
#if !defined(_WIN32)
        // TLS 上下文本体 POSIX only（windows 头面仅前向声明，指针型可存）
        std::unique_ptr<apollo::net::tls::TlsServerContext> tls_context;
#endif
        if (config.httpsPort != 0) {
            if (config.tlsCertPath.empty() || config.tlsKeyPath.empty()) {
                throw std::runtime_error(
                    "https ingress enabled but tls cert/key not configured "
                    "(--tls-cert/--tls-key)");
            }
#if !defined(_WIN32)
            tls_context = std::make_unique<apollo::net::tls::TlsServerContext>();
            const auto load_result = tls_context->load(config.tlsCertPath, config.tlsKeyPath);
            if (!load_result.ok) {
                throw std::runtime_error("https tls context load failed: "
                                         + load_result.error);
            }
            https_config.tls = tls_context.get();
#else
            // TLS 底座不进本批：不降级为明文监听（端口声明即 TLS 面）
            throw std::runtime_error(
                "--https-port needs the tls base (posix only)");
#endif
        }

        auto http_server = makeHttpIngressServer(http_config);
        install_demo_routes(*http_server, config.wsPath);
        install_callback_route(*http_server, callback_chain, callback_verifier);
        http_server->setObserver(&observer);
        http_server->start();

        std::unique_ptr<HttpIngressServer> https_server;
        if (config.httpsPort != 0) {
            https_server = makeHttpIngressServer(https_config);
            install_demo_routes(*https_server, config.wsPath);
            install_callback_route(*https_server, callback_chain, callback_verifier);
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
            // 回调链单写者消费（与入口泵同线程 = 顺序两相：入队相位完成于
            // pump()，业务相位在此；业务慢只推迟本拍余量，入口受理面不堵）。
            // 演示 executor = 观测日志（真实业务接线 = P3 词汇落地后的挂账面）。
            const auto drained =
                callback_chain.drain([&](const CallbackRequest& callback) {
                    logger->info("callback id=" + std::to_string(callback.id) + " "
                                 + callback.method + " " + callback.path + " body="
                                 + std::to_string(callback.body.size()) + "B");
                });
            if (drained.executed != 0 || drained.expired != 0 || drained.failed != 0) {
                logger->info("callback drain: executed="
                             + std::to_string(drained.executed)
                             + " expired=" + std::to_string(drained.expired)
                             + " failed=" + std::to_string(drained.failed)
                             + " backlog=" + std::to_string(callback_chain.size()));
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
