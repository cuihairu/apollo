#include "world_app/world_app.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/script/module_rollout.hpp"
#include "apollo/script/script_runtime.hpp"
#include <algorithm>
#include <chrono>
// POSIX journal fsync（批A 决定收口）
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <unistd.h>
#include <stdexcept>
#include <vector>
#include "world_app_detail.hpp"

namespace world_app {

namespace detail {

// 库面日志出口单定义（world_app 拆分单元共享——detail 头声明，
// cat=world_app 检索键不变）
apollo::core::log::Logger& world_log() {
    return *apollo::core::log::global_log_manager().createLogger("world_app");
}
} // namespace detail

namespace {

// 库面日志出口（detail 单定义，本单元转发——定义在 world_app.cpp）
apollo::core::log::Logger& worldLog() {
    return detail::world_log();
}

class PersistentVolumeLock {
public:
    static bool acquire(const std::string& journal_path) {
        const std::string path = journal_path + ".lock";
        std::ifstream existing(path);
        if (existing) {
            std::string owner_line;
            std::getline(existing, owner_line);
            const auto sep = owner_line.find(':');
            const bool mine = owner_line.rfind(std::to_string(::getpid()) + ":", 0) == 0;
            if (sep != std::string::npos && pid_alive(owner_line.substr(0, sep))) {
                throw std::runtime_error("world journal volume is locked by live pid "
                                         + owner_line.substr(0, sep) + " at " + path);
            }
            if (sep == std::string::npos) {
                worldLog().warning(
                    "breaking malformed world journal volume lock at " + path);
            } else if (!pid_alive(owner_line.substr(0, sep))) {
                worldLog().warning(
                    "breaking stale world journal volume lock at " + path
                    + " stale_pid=" + owner_line.substr(0, sep));
            }
        }
        std::ofstream out(path, std::ios::trunc);
        if (!out) {
            throw std::runtime_error("failed to open world journal volume lock at "
                                     + path);
        }
        out << ::getpid() << ':' << started_at() << '\n';
        out.flush();
        if (!out) {
            throw std::runtime_error("failed to write world journal volume lock at "
                                     + path);
        }
        return true;
    }

private:
    static std::string started_at() {
        const auto now = std::chrono::system_clock::now();
        const auto seconds = std::chrono::system_clock::to_time_t(now);
        std::tm utc{};
        gmtime_r(&seconds, &utc);
        char buffer[32]{};
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
        return buffer;
    }

    static bool pid_alive(const std::string& pid_text) {
        char* end = nullptr;
        const long pid = std::strtol(pid_text.c_str(), &end, 10);
        return end != pid_text.c_str() && *end == '\0' && pid > 0
               && ::kill(static_cast<pid_t>(pid), 0) == 0;
    }

};
} // namespace

//==============================================================================
// WorldApp 生命周期（manager 角色壳：M1 传输端点装配 + 管理泵启停）
//==============================================================================

WorldApp::WorldApp(const WorldAppConfig& config)
    : config_(config) {
    if (config_.bootAtMs == 0) {
        config_.bootAtMs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }
}

WorldApp::~WorldApp() {
    stop();
}

void WorldApp::start() {
    if (isRunning()) return;
    running_ = true;

    if (!config_.worldJournalPath.empty()) {
        volumeLocked_ = PersistentVolumeLock::acquire(config_.worldJournalPath);
    }
    // 管理日志装载（L101 管理日志切片 → 批A journal v2）：闸开前——受理面
    // 未开，无并发写者（损坏行显式拒启，见 loadWorldJournal；B 行铸代次
    // 先记后开受理面——本调用先于 makeM1LinkServer 装配）
    loadWorldJournal();

    // net M1 接收端（2b-ii，2b-iii 起 WorldApp 唯一受理入口——legacy RepSocket
    // 已退役）：InterServerLink passive 端点装配（先建对象不监听——管理泵
    // 泵引用它在位；bind/listen 在泵起之后）
    m1Link_ = makeM1LinkServer(
        M1LinkConfig{config_.host, config_.m1Port, config_.m1RouteJson,
                     config_.m1AuthorityEpoch},
        [this](IntentEnvelope /*intent*/) {
            // 转发成功时延时应答由注册 cell 回程；失败/无 cell 时在 raw 回
            // 调回 2001（此处只承载受理计数，不重复回执）。
        },
        [this](const std::vector<std::uint8_t>& payload) {
            // 切片2 意图族转发：932 原始 envelope 经首个就绪 cell 的注册
            // 连接反向转发；空目标/发送失败 = ADR-017 即时回执（2001），
            // 不静默丢弃（gateway pending-reply 超时面不再重发）。
            const auto cell_peer = firstReadyCellPeer();
            if (!cell_peer.empty() && m1Link_) {
                (void)m1Link_->sendCellIntentRaw(cell_peer, payload);
                return;
            }
            IntentEnvelope intent;
            if (decodeEnvelopeIntent(payload, intent) && intent.request_id != 0
                && m1Link_) {
                m1Link_->sendIntentAck(intent.request_id, kIntentAckInternal,
                                       "simulation not hosted");
            }
        },
        [this](const std::string& peer, const DebugEvalRequest& request) {
            onM1DebugEval(peer, request);
        },
        [this](const std::string& peer, const ModuleRolloutRequest& request) {
            onM1ModuleRollout(peer, request);
        },
        [this](const std::string& peer, const WorldDrainRequest& request) {
            onM1WorldDrain(peer, request);
        },
        [this](const std::string& peer, const CellRegisterRequest& request) {
            onCellRegister(peer, request);
        },
        [this](const std::string& peer, const CellRetireRequest& request) {
            onCellRetire(peer, request);
        },
        [this](const std::string& peer, const CellSpaceReport& report) {
            onCellSpaceReport(peer, report);
        },
        [this](const std::string& peer, const CellEvalResult& result) {
            onCellEvalResult(peer, result);
        },
        [this](const std::string& peer, const CellRolloutReply& reply) {
            onCellRolloutReply(peer, reply);
        },
        [this](const std::string& peer, const CellDrainReply& reply) {
            onCellDrainReply(peer, reply);
        },
        [this](const std::string& peer, const CellIntentAck& ack) {
            onCellIntentAck(peer, ack);
        });
    // 断连存疑标记接线（L101 断连存疑切片）：on_peer_down 在 M1 泵上下文
    //（管理泵线程）触发——注册表单写者上下文，直接改表。
    m1Link_->on_peer_down([this](const std::string& peer) { onPeerLinkDown(peer); });

    // 监听后置：bind 失败同契约显形，回滚再抛
    if (!m1Link_->start()) {
        m1Link_->stop();
        m1Link_.reset();
        running_ = false;
        throw std::runtime_error("failed to start WorldApp M1 link endpoint (route table load or bind failed)");
    }

    // 管理壳泵线程：M1 传输泵 + 管理推进。
    managerThread_ = std::thread([this] {
        const auto tick = std::chrono::milliseconds(10);
        while (running_) {
            const auto now_ms = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
            if (m1Link_) m1Link_->pump(now_ms);
            if (managerPreTick_) managerPreTick_(now_ms);
            driveCellRegistry();
            std::this_thread::sleep_for(tick);
        }
    });

    worldLog().info("WorldApp M1 link listening on " + config_.host + ":"
                + std::to_string(m1Link_->listenPort()));
}

void WorldApp::stop() {
    // 停机：管理泵线程停（join）→ M1 端点停
    running_ = false;
    if (managerThread_.joinable()) {
        managerThread_.join();
    }

    if (m1Link_) {
        m1Link_->stop();
        m1Link_.reset();
    }
    if (volumeLocked_ && !config_.worldJournalPath.empty()) {
        std::error_code ec;
        std::filesystem::remove(config_.worldJournalPath + ".lock", ec);
    }
    volumeLocked_ = false;
}

} // namespace world_app
