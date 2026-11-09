#include "login/login_server.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include <cstdint>
#include <iostream>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <filesystem>
#include "apollo/utils/crypto.h"

using apollo::utils::PasswordHash;

namespace login {
//==============================================================================
// LoginServer 实现
//==============================================================================

//==============================================================================
// 编队面（P3-1 re-home：baseappmgr 退场后目录 owner = Login 进程）
//==============================================================================

namespace {
namespace session_ns = apollo::game::session;
namespace disco_ns = apollo::net::discovery;

std::uint64_t steady_now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// 目录全量快照导出（旧 baseappmgr 壳同款，增量② owner 侧）：Online 条目 →
// 镜像投影条目（gateway_addr 不上 wire——镜像是定位面不是连接面）。
std::vector<session_ns::MirrorEntry> collect_mirror_entries(
    const online_directory::OnlineDirectory& directory) {
    std::vector<session_ns::MirrorEntry> entries;
    const auto& dir = directory.directory();
    entries.reserve(dir.size());
    for (const auto player_id : dir.online_ids()) {
        const auto* entry = dir.find(player_id);
        if (entry == nullptr) {
            continue;
        }
        session_ns::MirrorEntry me;
        me.player_id = player_id;
        me.anchor_epoch = entry->anchor_epoch;
        me.session_id = entry->binding.session_id;
        me.gateway_id = entry->binding.gateway_id;
        me.zone_id = entry->zone_id;
        me.state = entry->state;
        me.assignment = entry->assignment;
        entries.push_back(me);
    }
    return entries;
}
} // namespace

LoginServer::LoginServer(const LoginConfig& config)
    : config_(config)
    , authenticator_(std::make_unique<Authenticator>(config))
    , gatewayAllocator_(std::make_unique<GatewayAllocator>(config))
    , sessionManager_(std::make_unique<SessionManager>(config))
    , serviceRegistry_(std::make_unique<ServiceRegistry>()) {
}

LoginServer::~LoginServer() {
    stop();
}

void LoginServer::start() {
    if (running_) return;

    if (config_.directoryEnabled) {
        directory_ = std::make_unique<online_directory::OnlineDirectory>(0, config_.host);
        if (!config_.directoryEpochPath.empty()) {
            const auto parent = std::filesystem::path(config_.directoryEpochPath).parent_path();
            if (!parent.empty()) std::filesystem::create_directories(parent);
            directory_->attach_epoch_store(config_.directoryEpochPath);
            if (!directory_->directory().epoch_store_ok()) {
                directory_.reset();
                throw std::runtime_error("Login directory epoch storage is unavailable");
            }
        }
        directory_->set_m1_face(config_.directoryPort, config_.m1RouteJson);
        directory_->start();
        if (directory_->listenPort() == 0) {
            directory_->stop();
            directory_.reset();
            throw std::runtime_error("Login online directory failed to listen");
        }
        config_.m1OnlineDirectoryPort = directory_->listenPort();
        config_.m1OnlineDirectoryHost = config_.host == "0.0.0.0" ? "127.0.0.1" : config_.host;
    }
    running_ = true;

    // M1 准入受理面（P4-4 批①）：m1Port = 0 = 不装配（非 M1 接线的本地
    // 构建面）；装配失败降级（警告 + 不拦启动）。nng 准入 REP 已随批⑤退役
    // （桩传输死面）——handleLoginRequest/handlePing 留作 P3-2 ingress
    // 主线接线面。
    if (config_.m1Enabled || config_.m1Port != 0) {
        M1AdmissionConfig m1Config;
        m1Config.host = config_.host;
        m1Config.port = config_.m1Port;
        m1Config.route_json = config_.m1RouteJson;
        m1Admission_ = makeM1AdmissionServer(
            std::move(m1Config), [this](const std::uint8_t* frame, std::size_t size) {
                return dispatchRequest(
                    std::vector<std::uint8_t>(frame, frame + size));
            });
        if (m1Admission_->start()) {
            apollo::core::log::global_log_manager()
                .createLogger("login_server")
                ->info("M1 admission listening on " + config_.host + ":"
                       + std::to_string(m1Admission_->listenPort()));
        } else {
            apollo::core::log::global_log_manager()
                .createLogger("login_server")
                ->warning("M1 admission degraded: route table unavailable ("
                          + config_.m1RouteJson + "), admission face stays off");
            m1Admission_.reset();
        }
    }

    // M1 服务注册受理面（L123 首切片）：显式 opt-in（CLI 传
    // --service-register-port 即装配；不传 = 不装配）；port 0 = 内核分配
    //（listenPort 读回实际端口）；装配失败降级（警告 + 不拦启动，同准入
    // 面）。registry 常驻（面关也可观测/后续批本地写入）。
    if (config_.serviceRegisterEnabled) {
        M1ServiceRegisterConfig regConfig;
        regConfig.host = config_.host;
        regConfig.port = config_.serviceRegisterPort;
        regConfig.route_json = config_.m1RouteJson;
        m1ServiceRegister_ =
            makeM1ServiceRegisterServer(std::move(regConfig), serviceRegistry_.get());
        if (m1ServiceRegister_->start()) {
            apollo::core::log::global_log_manager()
                .createLogger("login_server")
                ->info("service register listening on " + config_.host + ":"
                       + std::to_string(m1ServiceRegister_->listenPort()));
        } else {
            apollo::core::log::global_log_manager()
                .createLogger("login_server")
                ->warning("service register degraded: route table unavailable ("
                          + config_.m1RouteJson + "), register face stays off");
            m1ServiceRegister_.reset();
        }
    }

    // M1 base 客户端面（P4-4 批③）：worldStoreM1Port = 0 = 不装配（RPC 面缺席
    // = 降级缝 throw）；装配失败降级（警告 + 不拦启动）。
    if (config_.worldStoreM1Port != 0) {
        M1BaseClientConfig baseConfig;
        baseConfig.host = config_.worldStoreM1Host;
        baseConfig.port = config_.worldStoreM1Port;
        baseConfig.route_json = config_.m1RouteJson;
        worldStoreClient_ = makeM1BaseClient(std::move(baseConfig));
        if (worldStoreClient_) {
            apollo::core::log::global_log_manager()
                .createLogger("login_server")
                ->info("M1 base client dialing " + config_.worldStoreM1Host + ":"
                       + std::to_string(config_.worldStoreM1Port));
        } else {
            apollo::core::log::global_log_manager()
                .createLogger("login_server")
                ->warning("M1 base client degraded: route table unavailable ("
                          + config_.m1RouteJson + "), base rpc degrades");
        }
    }

    // M1 manager 客户端面（P4-4 批④）：AssignWorld 落点裁决链路；装配
    // 失败降级（警告 + 不拦启动），AssignWorld 缺席 = 降级缝 throw。
    if (!directory_ && config_.m1OnlineDirectoryPort != 0) {
        M1BaseClientConfig mgrConfig;
        mgrConfig.host = config_.m1OnlineDirectoryHost;
        mgrConfig.port = config_.m1OnlineDirectoryPort;
        mgrConfig.route_json = config_.m1RouteJson;
        mgrConfig.peer_name = "login";
        m1OnlineDirectoryClient_ = makeM1BaseClient(std::move(mgrConfig));
        if (m1OnlineDirectoryClient_) {
            apollo::core::log::global_log_manager()
                .createLogger("login_server")
                ->info("M1 OnlineDirectory client dialing " + config_.m1OnlineDirectoryHost + ":"
                       + std::to_string(config_.m1OnlineDirectoryPort));
        } else {
            apollo::core::log::global_log_manager()
                .createLogger("login_server")
                ->warning("M1 manager client degraded: route table unavailable ("
                          + config_.m1RouteJson + "), assign rpc degrades");
        }
    }

    // ---- 编队面（P3-1 re-home：baseappmgr 退场后目录 owner = Login 进程；
    //      三面全关 = 零接线零线程，与旧进程壳默认一致）----
    if (directory_ && config_.mirrorPort != 0 && mirrorTransport_.open()) {
        const auto host = config_.mirrorHost;
        const auto port = config_.mirrorPort;
        mirrorPublisher_ = std::make_shared<session_ns::DirectoryPublisher>(
            [this, host, port](const std::uint8_t* data, std::size_t len) {
                return mirrorTransport_.send_bytes(host, port, data, len);
            });
    }
    if (directory_ && config_.recoveryPort != 0 &&
        recoveryFeed_.open(config_.recoveryPort, "0.0.0.0")) {
        // 恢复相位排他（拒新）：进程启动 = machined 拉起后的恢复序——收各
        // World/gateway 全量重报，收敛（全报或超时）开放准入。
        recovery_ = std::make_unique<session_ns::FleetRecoveryCoordinator>(
            config_.recoveryExpected, config_.recoveryTimeoutMs,
            [this](std::uint64_t component_id, std::uint32_t zone_id,
                   const std::vector<session_ns::MirrorEntry>& sessions) {
                const auto taken =
                    directory_->intake_directory_full_report(sessions);
                apollo::core::log::global_log_manager()
                    .createLogger("fleet")
                    ->info("full report: component=" + std::to_string(component_id) +
                           " zone=" + std::to_string(zone_id) + " sessions=" +
                           std::to_string(sessions.size()) +
                           " intake=" + std::to_string(taken));
            });
        recovery_->begin(steady_now_ms());
        directory_->set_admission_gate(
            [this]() { return recovery_->admissible(); });
        apollo::core::log::global_log_manager()
            .createLogger("login_server")
            ->info("fleet recovery intake on port " +
                   std::to_string(recoveryFeed_.port()) + " (expected " +
                   std::to_string(config_.recoveryExpected) + ", timeout " +
                   std::to_string(config_.recoveryTimeoutMs) + "ms)");
    }
    if (directory_ && mirrorPublisher_) {
        // 目录事件链入镜像 publisher（迁移编排完成后的改指立即推由
        // set_snapshot_publisher 承担；快照按周期发布）
        directory_->set_snapshot_publisher(mirrorPublisher_);
        directory_->set_directory_event_listener(
            [publisher = mirrorPublisher_](
                const apollo::game::session::PlayerDirectory::Event& event) {
                publisher->publish(event);
            });
        // 启动即发一轮全量（晚加入镜像不用等首周期）
        mirrorPublisher_->publish_snapshot(collect_mirror_entries(*directory_));
    }
    if (config_.machinedPort != 0 && config_.machinedComponentId != 0 &&
        deathFeed_.open(0, "0.0.0.0") && deathTransport_.open()) {
        disco_ns::MemberId self;
        self.component_id = config_.machinedComponentId;
        self.service_port = deathFeed_.port();
        deathListener_ = std::make_unique<disco_ns::DeathListener>(
            self, config_.machinedHost, config_.machinedPort, deathTransport_);
        (void)deathListener_->subscribe();
    }
    if (mirrorPublisher_ || recovery_ || deathListener_) {
        fleetThread_ = std::thread([this]() { fleetLoop(); });
    }

    // 启动会话清理线程
    cleanupThread_ = std::thread([this]() {
        std::unique_lock<std::mutex> lock(cleanupMutex_);
        while (!cleanupWake_.wait_for(lock, std::chrono::seconds(60), [this] { return !running_.load(); })) {
            lock.unlock();
            sessionManager_->cleanupExpiredSessions();
            lock.lock();
        }
    });

    apollo::core::log::global_log_manager()
        .createLogger("login_server")
        ->info("Login server listening on " + config_.host + ":"
               + std::to_string(config_.port));
}

void LoginServer::stop() {
    running_ = false;
    cleanupWake_.notify_all();

    // 编队面先收线（fleet 线程引用 directory_/协调器/publisher，先 join 再拆）。
    if (fleetThread_.joinable()) {
        fleetThread_.join();
    }
    deathListener_.reset();
    recovery_.reset();
    mirrorPublisher_.reset();

    // M1 准入面先收线（泵线程停驱 + 监听拆线）。
    if (m1Admission_) {
        m1Admission_->stop();
        m1Admission_.reset();
    }

    // M1 服务注册面收线（同准入面：泵线程停驱 + 监听拆线）。
    if (m1ServiceRegister_) {
        m1ServiceRegister_->stop();
        m1ServiceRegister_.reset();
    }

    // M1 base / manager 客户端面收线（in-flight call 由宿主泵线程先归还
    // ——本函数只在宿主停驱后执行）。
    if (worldStoreClient_) {
        worldStoreClient_->stop();
        worldStoreClient_.reset();
    }
    if (m1OnlineDirectoryClient_) {
        m1OnlineDirectoryClient_->stop();
        m1OnlineDirectoryClient_.reset();
    }

    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }
    if (directory_) {
        directory_->stop();
        directory_.reset();
    }
}

//==============================================================================
// 编队面（P3-1 re-home：baseappmgr 退场后目录 owner = Login 进程）
//==============================================================================

void LoginServer::fleetLoop() {
    const auto logger =
        apollo::core::log::global_log_manager().createLogger("fleet");
    auto since_snapshot = std::chrono::milliseconds(0);
    bool was_recovering = recovery_ && recovery_->recovering();
    std::uint8_t report_buf[disco_ns::kMaxDatagramSize];
    std::uint8_t death_buf[disco_ns::kWireSize];

    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        since_snapshot += std::chrono::seconds(1);

        // 恢复相位轮询（增量③）：收 FullReport 分片 + 喂钟收敛判定
        if (recovery_) {
            std::size_t rlen = 0;
            while (recoveryFeed_.try_receive(report_buf, sizeof(report_buf), rlen)) {
                session_ns::FleetReportPart part;
                if (session_ns::decode_fleet_report(report_buf, rlen, part)) {
                    (void)recovery_->on_report_part(part);
                }
            }
            recovery_->tick(steady_now_ms());
            if (was_recovering && !recovery_->recovering()) {
                was_recovering = false;
                logger->info("recovery converged: reported="
                             + std::to_string(recovery_->last_reported_count())
                             + "/" + std::to_string(config_.recoveryExpected)
                             + ", admission open");
                // 收敛即向镜像面发一轮全量（重建目录进投影）
                if (mirrorPublisher_ && directory_) {
                    const auto entries =
                        collect_mirror_entries(*directory_);
                    const auto parts =
                        mirrorPublisher_->publish_snapshot(entries);
                    logger->info("snapshot published entries=" +
                                 std::to_string(entries.size()) + " parts=" +
                                 std::to_string(parts));
                }
            }
        }

        // 周期全量快照（对账 healing 面：delta 丢失由快照收敛）
        if (mirrorPublisher_ && directory_ &&
            since_snapshot.count() >=
                static_cast<std::int64_t>(config_.mirrorSnapshotMs)) {
            since_snapshot = std::chrono::milliseconds(0);
            const auto entries = collect_mirror_entries(*directory_);
            const auto parts = mirrorPublisher_->publish_snapshot(entries);
            logger->info("snapshot published entries=" +
                         std::to_string(entries.size()) + " parts=" +
                         std::to_string(parts));
        }

        // 窗口满扫描（§4 tick 驱动）：到期 Suspended 删条目 + SessionDown
        //（事件链自动进镜像面）。0 终结不打日志（空转是常态）。
        if (directory_) {
            if (const auto expired =
                    directory_->sweep_suspended(steady_now_ms() / 1000);
                expired > 0) {
                logger->info("sweep expired=" + std::to_string(expired));
            }
        }

        if (!deathListener_) {
            continue;
        }
        // 订阅刷新（幂等——machined 侧 TTL = 3 × 其心跳周期，漏刷即停推）
        (void)deathListener_->subscribe();
        // 死亡事件收包 → 死亡行窗口处置（zone 行 / gateway 行批量进保活
        // 窗口，窗口满由 sweep 终结）
        std::string sender_host;
        std::uint16_t sender_port = 0;
        while (deathFeed_.try_receive(death_buf, sender_host, sender_port)) {
            disco_ns::WirePacket packet;
            if (!disco_ns::WirePacket::decode_from(death_buf, packet)) {
                continue;
            }
            disco_ns::DeathEvent death;
            if (!disco_ns::death_event_from(packet, death)) {
                continue;
            }
            logger->warning("death: component="
                            + std::to_string(death.member.component_id) + " zone="
                            + std::to_string(death.member.zone_id) + " exit="
                            + std::to_string(death.exit_code));
            if (!directory_) {
                continue;
            }
            const std::uint64_t now_tick = steady_now_ms() / 1000;
            if (death.member.zone_id != 0) {
                const auto suspended = directory_->suspend_zone_sessions(
                    death.member.zone_id, now_tick, config_.suspendWindowTicks);
                logger->warning("zone death zone=" +
                                std::to_string(death.member.zone_id) +
                                " suspended=" + std::to_string(suspended));
            }
            const auto gw = config_.gatewayByComponent.find(death.member.component_id);
            if (gw != config_.gatewayByComponent.end() && gw->second != 0) {
                const auto suspended = directory_->suspend_gateway_sessions(
                    gw->second, now_tick, config_.suspendWindowTicks);
                logger->warning("gateway death component=" +
                                std::to_string(death.member.component_id) +
                                " gateway=" + std::to_string(gw->second) +
                                " suspended=" + std::to_string(suspended));
            }
        }
    }
}

} // namespace login
