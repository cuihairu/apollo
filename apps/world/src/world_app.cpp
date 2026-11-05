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

namespace world_app {

namespace {

// 常量时间字符串相等（L109 认证凭据面）：凭据校验不得经计时侧信道泄
// 露首字节差异——长度先比（泄不泄长度），再全量异或累积不早退。OpenSSL
// CRYPTO_memcmp 口径同 apps/interfaces/src/callback_verifier.cpp:73。
bool constant_time_string_equal(const std::string& a, const std::string& b) {
    bool equal = a.size() == b.size();
    unsigned char mismatch = 0;
    const std::size_t span = a.size() < b.size() ? a.size() : b.size();
    for (std::size_t i = 0; i < span; ++i) {
        mismatch |= static_cast<unsigned char>(a[i]) ^
                    static_cast<unsigned char>(b[i]);
    }
    return equal && mismatch == 0;
}

// 库面日志出口：cat=world_app（§5.1 cat= 检索键；主程已接文件面）
apollo::core::log::Logger& worldLog() {
    return *apollo::core::log::global_log_manager().createLogger("world_app");
}

// steady 单调毫秒（在途关联账时戳域；与 cellExpiryMs 同一单调域）。
std::uint64_t steady_now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// 能力位覆盖判定（容量申报裁决）：cell 声明的 capabilities 须覆盖 Space
// 要求的 needs 全部位才有认领资格。needs=0 = 无要求——任何 cell 均
// 合格，含未声明（capabilities=0）的现状语义。
bool capability_fit(std::int32_t capabilities, std::int32_t needs) {
    return (capabilities & needs) == needs;
}

// 分配池成员判定（SpaceSlot 表口径，pool 复用两处：装载终态重建 + 观测）。
bool pool_has(const WorldAppConfig& config, std::uint64_t space_id) {
    for (const auto& slot : config.assignSpaces) {
        if (slot.space_id == space_id) {
            return true;
        }
    }
    return false;
}

// 逗号分隔依赖名 → 列表（952 编排请求；空段跳过，全空 = 无依赖）。
constexpr std::size_t kM1EvalBacklogMax = 64;

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
// WorldApp 实现（manager 角色壳：M1 传输端点 + 管理编排；执行内核 CellApp
// 已分离——切片3 删除内嵌内核，管理命令经注册 cell 连接转发）
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

void WorldApp::onCellRegister(const std::string& peer,
                              const CellRegisterRequest& request) {
    // 960 受理（L101 注册首切片；本回调在内核线程泵上下文——注册表单线程
    // 独占无锁，受理点同步回 961）。request_id = 0 保留（无回执无登记）。
    // 认证凭据面已落地（L109）：registerToken 非空时凭据逐字节匹配才受理
    //（拒伪造注册，见下）；空 = 认证关（同 944/952/956 裸帧惯例）。
    if (request.request_id == 0) {
        return;
    }
    const auto reply = [&](bool accepted, std::int32_t code, const std::string& detail) {
        if (m1Link_) {
            m1Link_->sendCellRegisterReply(peer, request.request_id, accepted, code,
                                           config_.m1AuthorityEpoch, request.cell_id,
                                           detail);
        }
    };
    if (request.cell_id == 0 || request.start_epoch == 0) {
        // 差距 ①：身份/代次非法显式驳回——逻辑 ID 与启动代次是登记前提
        //（PID/IP 不当身份，0 号即缺位）
        reply(false, kCellRegisterRejectedInvalid, "invalid cell_id or start_epoch");
        return;
    }
    const auto now_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    auto it = cellRegistrations_.find(request.cell_id);
    // 代次裁决先于兼容裁决（code 3 先于 code 5）：旧代次复活凭 code 3 自冻
    // 结隔离（客户端语义）——若被 code 5/code 6 抢答，旧执行端失去自隔离
    // 信号。凭据裁决置于代次之后：旧代次执行端凭据过期/轮换仍收 code 3
    //（自隔离），不因 code 6 抢答丢失隔离语义。
    if (it == cellRegistrations_.end()) {
        // 旧执行端隔离（L101 隔离切片）：代次水位——摘除忘却登记，但本进程
        // 曾受理的更高代次仍拒旧代次复活（旧端不能借摘除冒认首注册；同代次
        // = 同一化身断链回拨，放行走首注册。内存水位，持久墓碑随管理日志批）
        const auto wm = cellEpochWatermarks_.find(request.cell_id);
        if (wm != cellEpochWatermarks_.end() && request.start_epoch < wm->second) {
            reply(false, kCellRegisterRejectedStaleEpoch,
                  "stale epoch: watermark=" + std::to_string(wm->second));
            return;
        }
    } else if (request.start_epoch < it->second.start_epoch) {
        // §4.4：旧代次不可覆盖新登记（重启获新代次——旧连接回拨拒绝，
        // 登记保持新代次；旧执行端隔离随 L101 后续切片）
        reply(false, kCellRegisterRejectedStaleEpoch,
              "stale epoch: registered=" + std::to_string(it->second.start_epoch));
        return;
    }
    // 启动凭据自证（L109 认证凭据面）：World config.registerToken 非空时，
    // cell 上报的 credential 须逐字节等于该值才受理——spawn 注入的凭据源
    // 拒未持凭据/错凭据的伪造注册。空配置 = 认证关（任何值均可，含未声明，
    // 同 944/952/956 裸帧惯例）。凭据不落可公开日志（detail 只给布尔面）。
    // 常量时间比较：非定长 std::string operator!= 首字节不同即短路，可经
    // 计时侧信道逐字节枚举（见 constant_time_string_equal）。
    if (!config_.registerToken.empty()
        && !constant_time_string_equal(request.credential, config_.registerToken)) {
        reply(false, kCellRegisterRejectedCredential, "register credential mismatch");
        return;
    }
    // 契约兼容裁决（960 版本/兼容切片 + L109 兼容范围白名单面，§4.3「World
    // 验证所属关系、版本和身份」的版本面）：声明投影须落在「本进程投影 ∪
    // compatWhitelist」内才受理——未声明（0）即不可验证，始终拒 code 5。
    // 白名单非空时允许旧版投影（混编期间的旧 cell 与新版 World 并存）。
    const std::int64_t compat_projection = contract_compat_projection();
    bool compat_ok = request.compat == compat_projection;
    if (!compat_ok) {
        for (const auto accepted_compat : config_.compatWhitelist) {
            if (request.compat == accepted_compat && accepted_compat != 0) {
                compat_ok = true;
                break;
            }
        }
    }
    if (!compat_ok) {
        reply(false, kCellRegisterRejectedIncompatible,
              "incompatible contract compat: declared="
                  + std::to_string(request.compat) + " expected="
                  + std::to_string(compat_projection));
        return;
    }
    if (it == cellRegistrations_.end()) {
        CellRegistration reg;
        reg.cell_id = request.cell_id;
        reg.start_epoch = request.start_epoch;
        reg.host = request.host;
        reg.port = request.port;
        reg.capabilities = request.capabilities;
        reg.compat = request.compat;
        reg.load = request.load;
        reg.drain_state = request.drain_state;
        reg.ready = request.ready;
        reg.registered_at_ms = now_ms;
        reg.peer = peer;
        cellRegistrations_.emplace(request.cell_id, std::move(reg));
        recordEpochWatermark(request.cell_id, request.start_epoch);
        worldLog().info("cell registered: id=" + std::to_string(request.cell_id)
                        + " epoch=" + std::to_string(request.start_epoch)
                        + " peer=" + peer);
        reply(true, kCellRegisterAccepted, "registered");
        // 分配推送（962 分配面）：裁决随刚落表的自报面——ready/drain/load
        // 同拍生效，首注册即按资格认领池内未分配 Space
        pushSpaceAssignments(peer, cellRegistrations_.at(request.cell_id));
        return;
    }
    const bool idempotent = request.start_epoch == it->second.start_epoch;
    // 同代次幂等刷新 / 新代次替换：自报面整体刷新（peer 也换——重连 =
    // 同名或新名链路，回执按名寻路）
    it->second.start_epoch = request.start_epoch;
    it->second.host = request.host;
    it->second.port = request.port;
    it->second.capabilities = request.capabilities;
    it->second.compat = request.compat;
    it->second.load = request.load;
    it->second.drain_state = request.drain_state;
    it->second.ready = request.ready;
    it->second.registered_at_ms = now_ms;
    it->second.peer = peer;
    it->second.suspect = false;  // 断链复核归来（961 受理裁决通过）清存疑
    // 水位随受理推进 + 墓碑前进才落盘（recordEpochWatermark 统一判前进，
    // 这里不再直写 map——先写会骗过前进判定漏落盘）
    recordEpochWatermark(request.cell_id, request.start_epoch);
    worldLog().info(std::string("cell re-") + (idempotent ? "registered: " : "placed (new epoch): ")
                    + "id=" + std::to_string(request.cell_id)
                    + " epoch=" + std::to_string(request.start_epoch)
                    + " peer=" + peer);
    reply(true, idempotent ? kCellRegisterAcceptedIdempotent : kCellRegisterAccepted,
          idempotent ? "re-registered (same epoch, refreshed)"
                     : "replaced (new epoch)");
    // 分配推送（962 分配面；code 1/2 受理都推——断链重拨后分配不丢）
    pushSpaceAssignments(peer, it->second);
}

void WorldApp::onPeerLinkDown(const std::string& peer) {
    // 断连存疑标记（L101 断连存疑切片，on_down 泵上下文 = 注册表单写者
    // 线程）：peer 对上登记即置存疑——gap 表 §line71 断开后先标记，不立即
    // 另端重建权威（释放/重认领仍走既有过期窗裁决）。未知 peer（gateway/
    // admin 链路无登记）无操作；同名重连前旧标记不清除（清除只随 961
    // 复核受理，见 onCellRegister）。
    for (auto& entry : cellRegistrations_) {
        if (entry.second.peer == peer && !entry.second.suspect) {
            entry.second.suspect = true;
            worldLog().info("cell link down marked suspect: cell_id="
                            + std::to_string(entry.first) + " peer=" + peer);
        }
    }
    dropCellReplyAccountsForPeer(peer);
}

void WorldApp::dropCellReplyAccountsForPeer(const std::string& peer) {
    // 在途关联账撤销（L101 断连存疑切片同上下文）：目标 cell 断连后永无
    // 回执——按 cell_peer 匹配撤账，并显式回「cell link lost」替代静默悬
    // 挂（eval/rollout/drain 三账同型）。账内条目本就应关联目标 cell；不
    // 匹配 peer 的条目保留（另一 cell 链路不受牵连）。
    auto drop = [&](auto& map, auto&& reply_fn) {
        for (auto it = map.begin(); it != map.end();) {
            if (it->second.cell_peer == peer) {
                reply_fn(it->second.fleetctl_peer, it->first);
                it = map.erase(it);
            } else {
                ++it;
            }
        }
    };
    drop(evalReplyPeers_, [this](const std::string& fp, std::uint64_t id) {
        sendM1EvalReply(fp, id, false, "", "cell link lost");
    });
    drop(rolloutReplyPeers_, [this](const std::string& fp, std::uint64_t id) {
        if (m1Link_) {
            m1Link_->sendModuleRolloutReply(fp, id, 0, false, -1, 0, 0,
                                            "cell link lost");
        }
    });
    drop(drainReplyPeers_, [this](const std::string& fp, std::uint64_t id) {
        if (m1Link_) {
            m1Link_->sendWorldDrainReply(fp, id, 0, false, 0, 0,
                                         "cell link lost");
        }
    });
}

void WorldApp::sweepExpiredCellReplyAccounts(std::uint64_t now_ms) {
    // 过期清扫（管理泵每拍，同注册表 driveCellRegistry 口径）：cell 回执
    // 在 cellExpiryMs 内未到即撤账（cell 死亡/回执丢失——断连面已撤，此
    // 面兜底挂起不回的链路级失效）。0 = 永不过期（同注册表面口径）。
    if (config_.cellExpiryMs == 0) {
        return;
    }
    auto sweep = [&](auto& map, auto&& reply_fn) {
        for (auto it = map.begin(); it != map.end();) {
            if (now_ms - it->second.sent_at_ms > config_.cellExpiryMs) {
                reply_fn(it->second.fleetctl_peer, it->first);
                it = map.erase(it);
            } else {
                ++it;
            }
        }
    };
    sweep(evalReplyPeers_, [this](const std::string& fp, std::uint64_t id) {
        sendM1EvalReply(fp, id, false, "", "eval reply timeout");
    });
    sweep(rolloutReplyPeers_, [this](const std::string& fp, std::uint64_t id) {
        if (m1Link_) {
            m1Link_->sendModuleRolloutReply(fp, id, 0, false, -1, 0, 0,
                                            "rollout reply timeout");
        }
    });
    sweep(drainReplyPeers_, [this](const std::string& fp, std::uint64_t id) {
        if (m1Link_) {
            m1Link_->sendWorldDrainReply(fp, id, 0, false, 0, 0,
                                         "drain reply timeout");
        }
    });
}

std::size_t WorldApp::suspect_cell_count() const {
    std::size_t count = 0;
    for (const auto& entry : cellRegistrations_) {
        if (entry.second.suspect) {
            ++count;
        }
    }
    return count;
}

void WorldApp::pushSpaceAssignments(const std::string& peer,
                                    const CellRegistration& reg) {
    // 分配面（L101 首切片 + L108 回收切片 + L104 就绪/排空/负载裁决切片）：
    // 受理即推全池权威归属视图。未登记 Space 首认领（版本 1）；owner=0
    // （过期释放）重认领且版本 +1（认领转移面：离线旧主本地视图停在旧版本，
    // 须版本前进才收敛，防 ABA 重放；同主回拨也前进）；本 cell 已归属的
    // 刷新（版本不变幂等）；归属其它 cell 的推 ready=0 撤销标记（收帧方据
    // 此撤本地残留路由——离线期缺口经重注册收敛，见 L108 切片）。认领独占：
    // 不重分他人 Space（重平衡随 L108 拓扑批）。
    //
    // 认领资格（L104 负载裁决 + 容量申报裁决切片）：未就绪 / 自报排空中 /
    // World 单方缩容（retire）的 cell 不认领新 Space——进程存活不等于可分配，
    // 既有归属照常刷新（撤空不撤权）；声明能力未覆盖该 Space needs 的
    // cell 同样不认领——没有容量明确拒绝（world-cell-management §5），留池
    // 等合格候选，不落到不合格 cell 上凑数。已释放的 Space 走最小负载优先，
    // 存在负载严格更低且同样合格的候选则本 cell 退让（等负载面收敛后随下次
    // 受理/刷新认领），同负载让位给先受理者（不抖动重分）。
    if (config_.assignSpaces.empty()) {
        return;
    }
    if (journalMissingAtBoot_) {
        if (!missingJournal_) {
            missingJournal_ = true;
            worldLog().warning(
                "world journal missing evidence: cell=" + std::to_string(reg.cell_id)
                + " has no tombstone; all assignments frozen");
        }
        for (const auto& slot : config_.assignSpaces) {
            worldLog().warning(
                "space assignment blocked by missing journal evidence: space="
                + std::to_string(slot.space_id));
        }
        return;
    }
    const bool may_claim = reg.ready && reg.drain_state == 0 && !reg.retire;
    for (const auto& slot : config_.assignSpaces) {
        const std::uint64_t space_id = slot.space_id;
        if (missingJournal_) {
            worldLog().warning(
                "space assignment blocked by missing journal evidence: space="
                + std::to_string(space_id));
            continue;
        }
        // 冲突冻结（批B 967 裁决面）：冻结期不推新分配、不推刷新、不推撤销
        //（设计稿 §4——自报与权威表分歧未决前，任何一侧声明都可能固化错误
        // 归属；解冻随人工修复批挂账）。
        if (conflictedSpaces_.count(space_id) > 0) {
            continue;
        }
        auto it = spaceAssignments_.find(space_id);
        if (it == spaceAssignments_.end() || it->second.cell_id == 0) {
            // 未分配 / 已释放：无资格、无容量（能力未覆盖）或让位者不发 962
            //（无归属视图可推）
            if (!may_claim || !capability_fit(reg.capabilities, slot.needs) ||
                lower_load_peer(reg.cell_id, reg.load, slot.needs)) {
                continue;
            }
            const bool reclaim = it != spaceAssignments_.end();  // owner=0
            if (reclaim) {
                ++it->second.version;  // 重认领转移：版本前进（防 ABA）
            } else {
                SpaceAssignment a;
                a.space_id = space_id;
                a.cell_id = reg.cell_id;
                a.version = 1;
                it = spaceAssignments_.emplace(space_id, a).first;
            }
            it->second.cell_id = reg.cell_id;
            // write-ahead（批A journal v2）：A 行 append+flush 成功才推 962
            // ——持久决定先于对外声明。落盘失败回滚内存认领并跳过声明
            //（Space 留池待下次受理重试，告警 loudly；刷新/撤销标记不经此闸
            //——无转移发生，最后一条 A 行已覆盖该归属+版本）。
            if (!appendJournalRow("A " + std::to_string(space_id) + "|"
                                  + std::to_string(reg.cell_id) + "|"
                                  + std::to_string(it->second.version), true)) {
                worldLog().warning(
                    "world journal append failed: " + config_.worldJournalPath
                    + " (space claim withheld: space=" + std::to_string(space_id)
                    + " cell=" + std::to_string(reg.cell_id) + ")");
                if (reclaim) {
                    --it->second.version;  // 回滚版本前进（保持释放态）
                } else {
                    spaceAssignments_.erase(it);
                }
                continue;
            }
            worldLog().info(reclaim
                                ? "space re-claimed: space=" + std::to_string(space_id)
                                      + " cell=" + std::to_string(reg.cell_id)
                                      + " version=" + std::to_string(it->second.version)
                                : "space assigned: space=" + std::to_string(space_id)
                                      + " cell=" + std::to_string(reg.cell_id)
                                      + " version=1");
        } else if (it->second.cell_id != reg.cell_id) {
            if (m1Link_) {
                m1Link_->sendCellSpaceAssign(peer, it->second.version,
                                             config_.m1AuthorityEpoch, space_id,
                                             it->second.cell_id, false);
            }
            continue;  // 归属它方：撤销标记（owner/版本 = 当前归属视图）
        }
        if (m1Link_) {
            m1Link_->sendCellSpaceAssign(peer, it->second.version,
                                         config_.m1AuthorityEpoch, space_id,
                                         it->second.cell_id, true);
        }
    }
}

bool WorldApp::lower_load_peer(std::uint64_t cell_id, std::int32_t load,
                               std::int32_t needs) const {
    // 最小负载优先（L104 负载裁决 + 容量申报裁决切片）：登记表内任一合格
    // 候选负载严格低于本 cell 即让位——合格 = 就绪、不排空、未缩容且声明
    // 能力覆盖该 Space 的 needs（不合格的更低负载 peer 不参裁，否则合格
    // 方会把归属让给无资格方，Space 永久搁置）。同负载不退让（认领归先受理
    // 者，裁决对注册顺序稳定）；存疑（suspect）条目仍参裁——链路未决时归属
    // 不动比抢分安全。缩容意图（retire）条目排除在外：它永不认领，让位给
    // 它会永久搁置该 Space。
    for (const auto& entry : cellRegistrations_) {
        const auto& other = entry.second;
        if (other.cell_id != cell_id && other.ready && other.drain_state == 0 &&
            !other.retire && capability_fit(other.capabilities, needs) &&
            other.load < load) {
            return true;
        }
    }
    return false;
}

void WorldApp::releaseCellSpaces(std::uint64_t cell_id) {
    // 过期释放（L108 回收切片，driveCellRegistry 同上下文）：owner 置 0、
    // 版本不变——释放不是认领，无转移发生；重认领方经 pushSpaceAssignments
    // 版本 +1 收敛。条目保留：池成员身份不随心跳消失，Space 回池待认领。
    for (auto& entry : spaceAssignments_) {
        // 冲突冻结（批B 967 裁决面）：冻结期不释放（设计稿 §4「不推新分配、
        // 不释放」）——条目停尸在当前 owner，等待人工修复裁决解冻。
        if (conflictedSpaces_.count(entry.first) > 0) {
            continue;
        }
        if (entry.second.cell_id == cell_id) {
            entry.second.cell_id = 0;
            // R 行（批A journal v2）：释放是终态转移（owner 归零、版本不变），
            // 重放以每 space 最后一条 A/R 为准——缺席则重启误认旧主仍持有权。
            // 失败告警降级（批㉚ 可用性优先；装载面对账裁决随批B）。
            if (!appendJournalRow("R " + std::to_string(entry.first))) {
                worldLog().warning("world journal append failed: "
                                   + config_.worldJournalPath
                                   + " (release replays as owned: space="
                                   + std::to_string(entry.first) + ")");
            }
            worldLog().info("space released: space="
                            + std::to_string(entry.first)
                            + " cell=" + std::to_string(cell_id)
                            + " version=" + std::to_string(entry.second.version));
        }
    }
}

std::string WorldApp::firstReadyCellPeer() const {
    // 目标 cell peer 选择（切片2 转发）：首个就绪、未自报排空、未缩容的
    // 登记 cell 的链路 peer。无合格目标返回空串。
    for (const auto& entry : cellRegistrations_) {
        const auto& reg = entry.second;
        if (reg.ready && reg.drain_state == 0 && !reg.retire && !reg.suspect) {
            return reg.peer;
        }
    }
    return {};
}

std::string WorldApp::firstRegisteredCellPeer() const {
    // 管理命令 cell peer 选择：首个已登记、未缩容、未存疑的 cell（排空中
    // 仍可接收 952/956 管理命令——begin/status 是排空推进面）。
    for (const auto& entry : cellRegistrations_) {
        const auto& reg = entry.second;
        if (!reg.retire && !reg.suspect) {
            return reg.peer;
        }
    }
    return {};
}

bool WorldApp::beginCellRetire(std::uint64_t cell_id) {
    // 缩容编排（L104 缩容切片，注册表单写者上下文）：置 World 单方缩容意图
    // ——与 cell 自报 drain_state 分面（自报是 cell 自己的活动状态，本位是
    // World 的调度决定），此后该 cell 停止认领新 Space；名下归属即刻回池
    // （owner 归零、版本不变——在途活动在 cell 本地自然排空，不强断），再
    // 向其推当前权威视图：它收到的自有 Space 现为撤销标记，962 应用面据此
    // 撤本地路由。撤路由受客户端版本去重约束（释放不推进版本），故实际撤
    // 除随下次合格 peer 重认领后版本前进的那一拍收敛——同 L108 离线期缺口
    // 的收敛口径，不为强收敛改写版本号语义。
    auto it = cellRegistrations_.find(cell_id);
    if (it == cellRegistrations_.end()) {
        return false;
    }
    it->second.retire = true;
    releaseCellSpaces(cell_id);
    pushSpaceAssignments(it->second.peer, it->second);
    worldLog().info("cell retire begun: id=" + std::to_string(cell_id)
                    + " epoch=" + std::to_string(it->second.start_epoch)
                    + " peer=" + it->second.peer);
    return true;
}

bool WorldApp::cancelCellRetire(std::uint64_t cell_id) {
    // 取消缩容（设计稿「尚未发出退出命令且重新缺容量时可取消排空」）：只清
    // 意图，归属维持现状——已被他人重认领的不取回（不迁移不重分），仍释放
    // （owner=0）的随本次推送由恢复资格的本人或最低负载 peer 重认领（版本 +1）。
    auto it = cellRegistrations_.find(cell_id);
    if (it == cellRegistrations_.end()) {
        return false;
    }
    it->second.retire = false;
    pushSpaceAssignments(it->second.peer, it->second);
    worldLog().info("cell retire cancelled: id=" + std::to_string(cell_id)
                    + " epoch=" + std::to_string(it->second.start_epoch)
                    + " peer=" + it->second.peer);
    return true;
}

void WorldApp::onCellRetire(const std::string& peer, const CellRetireRequest& request) {
    // 缩容管理命令受理（963 cell_retire_request → 966，M1 泵上下文 = 登记表
    // 单写者上下文）：与 onCellRegister 同口，请求/回执同步同线程回发端链路。
    // 请求合法性先看（op 未知/cell_id 非法 → 拒收，不动表），再查登记
    // （目标未登记 = 显式 miss——运维据此判命令目标是否打错）。
    if (m1Link_ == nullptr) {
        return;
    }
    if (request.cell_id == 0) {
        m1Link_->sendCellRetireReply(peer, request.request_id, false,
                                     kCellRetireRejectedInvalid,
                                     config_.m1AuthorityEpoch, request.cell_id,
                                     "invalid cell_id");
        return;
    }
    if (request.op == kCellRetireOpBegin) {
        if (!beginCellRetire(request.cell_id)) {
            m1Link_->sendCellRetireReply(peer, request.request_id, false,
                                         kCellRetireRejectedUnknownCell,
                                         config_.m1AuthorityEpoch, request.cell_id,
                                         "cell not registered");
            return;
        }
        m1Link_->sendCellRetireReply(peer, request.request_id, true, kCellRetireBegun,
                                     config_.m1AuthorityEpoch, request.cell_id,
                                     "retire intent set; assignments released to pool");
        return;
    }
    if (request.op == kCellRetireOpCancel) {
        if (!cancelCellRetire(request.cell_id)) {
            m1Link_->sendCellRetireReply(peer, request.request_id, false,
                                         kCellRetireRejectedUnknownCell,
                                         config_.m1AuthorityEpoch, request.cell_id,
                                         "cell not registered");
            return;
        }
        m1Link_->sendCellRetireReply(peer, request.request_id, true,
                                     kCellRetireCancelled, config_.m1AuthorityEpoch,
                                     request.cell_id,
                                     "retire intent cleared; ownership unchanged");
        return;
    }
    m1Link_->sendCellRetireReply(peer, request.request_id, false,
                                 kCellRetireRejectedInvalid,
                                 config_.m1AuthorityEpoch, request.cell_id,
                                 "unknown op");
}

void WorldApp::onCellSpaceReport(const std::string& peer, const CellSpaceReport& report) {
    // 对账自报受理（967，断连对账批B；M1 泵上下文 = 注册表单写者上下文，
    // 与 onCellRegister/onCellRetire 同口）。逐条目核对权威归属表（设计稿
    // §4）——裁决就地生效，无回执（962 全池权威视图即裁决载体，冻结不经
    // 协议下发）。request_id = 0 保留（静默弃，同 952/960 关联键纪律）。
    if (report.request_id == 0 || report.cell_id == 0 || report.space_id == 0) {
        return;
    }
    const auto reg = cellRegistrations_.find(report.cell_id);
    if (reg == cellRegistrations_.end()) {
        // 未登记来源：无裁决地位（对账表以登记为准），丢弃。
        worldLog().info("space report from unregistered cell dropped: cell="
                        + std::to_string(report.cell_id)
                        + " space=" + std::to_string(report.space_id));
        return;
    }
    if (report.start_epoch != reg->second.start_epoch) {
        // 旧化身上报：登记已是新代次（新化身），旧端视图无对账效力，丢弃。
        worldLog().info("space report from stale incarnation dropped: cell="
                        + std::to_string(report.cell_id)
                        + " reported_epoch=" + std::to_string(report.start_epoch));
        return;
    }
    if (reg->second.suspect) {
        // 控制面失联期隔离：961 复核归来前，同登记代次但走新/旧链路的
        // 旧化身或旧端自报不得参与对账，也不能借 967 证明旧归属。
        worldLog().warning("space report from suspect cell dropped: cell="
                           + std::to_string(report.cell_id)
                           + " space=" + std::to_string(report.space_id));
        return;
    }
    ++spaceReportCount_;
    const auto entry = spaceAssignments_.find(report.space_id);
    if (!config_.worldJournalPath.empty() && entry != spaceAssignments_.end() &&
        entry->second.cell_id == report.cell_id &&
        entry->second.version == report.applied_version) {
        appendJournalRow("C " + std::to_string(report.space_id) + '|'
                         + std::to_string(report.cell_id) + '|'
                         + std::to_string(report.applied_version));
        ++reconciledSpaceCount_;
    }
    if (entry == spaceAssignments_.end()) {
        // 未登记权威（§4「无 S 记录 | C 报 S」行）：权威归属表无此 Space
        //（池外 id，或池内从未认领/记忆已失）——源隔离不明，进冲突集
        //（观测面；池外 id 无分配语义可冻结，冻结面天然只作用于池内）。
        const bool fresh = conflictedSpaces_.insert(report.space_id).second;
        worldLog().warning("space report conflict (no authoritative record): space="
                           + std::to_string(report.space_id)
                           + " cell=" + std::to_string(report.cell_id)
                           + (fresh ? " (frozen)" : " (already frozen)"));
        return;
    }
    if (entry->second.cell_id == report.cell_id) {
        if (entry->second.version == report.applied_version) {
            // 匹配（§4 行1）：保持，无操作。独立 cellServer 进程 967 真链路
            // 对账观测行（不改变裁决，只让冒烟可断言）。
            worldLog().info("space report reconciled: space="
                            + std::to_string(report.space_id)
                            + " cell=" + std::to_string(report.cell_id)
                            + " version=" + std::to_string(report.applied_version));
            return;
        }
        // 版本冲突（§4 行2）：权威仍归属本 cell 但应用版本对不上——journal
        // 落后或应用面偏离，该 Space 冻结（不认领不释放不推送），待对账修复。
        const bool fresh = conflictedSpaces_.insert(report.space_id).second;
        worldLog().warning("space report version conflict: space="
                           + std::to_string(report.space_id)
                           + " cell=" + std::to_string(report.cell_id)
                           + " authoritative=" + std::to_string(entry->second.version)
                           + " applied=" + std::to_string(report.applied_version)
                           + (fresh ? " (frozen)" : " (already frozen)"));
        return;
    }
    // 其余（owner=0 已释放 / 归属它方）：权威已前进——962 全池视图即裁决，
    // 上报端随撤销标记/重认领推送收敛（§4 行3 既有路径），收敛滞后不入冲突集。
    worldLog().info("space report lags authority: space="
                    + std::to_string(report.space_id)
                    + " cell=" + std::to_string(report.cell_id)
                    + " authoritative_owner=" + std::to_string(entry->second.cell_id)
                    + " applied=" + std::to_string(report.applied_version));
}

std::size_t WorldApp::retired_cell_count() const {
    std::size_t count = 0;
    for (const auto& entry : cellRegistrations_) {
        if (entry.second.retire) {
            ++count;
        }
    }
    return count;
}

const WorldApp::CellRegistration* WorldApp::find_cell_registration(
    std::uint64_t cell_id) const {
    // 泵上下文只读观测面（注册表单写者 = M1 泵上下文受理点）
    const auto it = cellRegistrations_.find(cell_id);
    return it != cellRegistrations_.end() ? &it->second : nullptr;
}

const WorldApp::SpaceAssignment* WorldApp::find_space_assignment(
    std::uint64_t space_id) const {
    const auto it = spaceAssignments_.find(space_id);
    return it != spaceAssignments_.end() ? &it->second : nullptr;
}

void WorldApp::loadWorldJournal() {
    // 管理日志装载（启动窗，M1 链路开闸前——受理面未开，无并发写者）。
    // v2 行型（world-reconciliation 设计稿 §2，前缀区型）：`[T] cell|epoch`
    // 代次墓碑（T 前缀可省——v1 裸行直接升级不迁移）、`A space|cell|version`
    // 认领转移、`R space` 释放（owner 归零版本不变）、`B epoch` 启动代次。
    // 装载终态 = 每 space 最后一条 A/R + 每 cell 最大 T + 最大 B。0 号/非
    // 数字/缺字段/多字段 = 损坏显式拒启（不静默部分回放——orders journal
    // 同口径）；行尾 \r 剥除（Windows 落盘坑账防御）。文件缺省 = 首启，
    // append 建档（空日志顶替禁令的判定随批C）。
    if (config_.worldJournalPath.empty()) {
        return;
    }
    std::ifstream in(config_.worldJournalPath, std::ios::binary);
    if (!in) {
        // 空日志判定（world-reconciliation §2）分两步：启动时文件缺失先按
        // 「无 cell 拨入的真空候选」放行；首个 961 拨入即证明曾存在旧管理
        // 关系，转记录缺失并全池冻结。装载面 T 水位也可直接证明证据缺失。
        journalMissingAtBoot_ = true;
        if (!cellEpochWatermarks_.empty()) {
            missingJournal_ = true;
            worldLog().warning(
                "world journal file missing: " + config_.worldJournalPath
                + " known_cells=" + std::to_string(cellEpochWatermarks_.size())
                + " (missing journal evidence frozen all assignments)");
            return;
        }
        return;  // 无记录 = 首启候选，受理后按拨入证据裁决
    }
    const auto bad_line = [&](std::uint64_t line_no, const char* why) {
        throw std::runtime_error(std::string("world journal corrupt (") + why
                                 + ") at " + config_.worldJournalPath + ":"
                                 + std::to_string(line_no));
    };
    const auto parse_u64 = [&](const std::string& field,
                               std::uint64_t line_no) -> std::uint64_t {
        std::size_t pos = 0;
        std::uint64_t value = 0;
        try {
            value = std::stoull(field, &pos);
        } catch (const std::exception&) {
            bad_line(line_no, "not a number");
        }
        if (pos != field.size() || field.empty()) {
            bad_line(line_no, "trailing garbage");
        }
        return value;
    };
    struct ReplaySpace {
        std::uint64_t cell_id = 0;
        std::uint64_t version = 0;
    };
    std::unordered_map<std::uint64_t, ReplaySpace> replay;
    std::uint64_t max_boot_epoch = 0;
    bool saw_boot_row = false;
    std::string line;
    std::uint64_t line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (line.compare(0, 2, "A ") == 0) {
            const std::string body = line.substr(2);
            const auto b1 = body.find('|');
            if (b1 == std::string::npos) {
                bad_line(line_no, "missing '|'");
            }
            const auto b2 = body.find('|', b1 + 1);
            if (b2 == std::string::npos) {
                bad_line(line_no, "missing '|'");
            }
            if (body.find('|', b2 + 1) != std::string::npos) {
                bad_line(line_no, "extra field");
            }
            const auto space_id = parse_u64(body.substr(0, b1), line_no);
            const auto cell_id = parse_u64(body.substr(b1 + 1, b2 - b1 - 1), line_no);
            const auto version = parse_u64(body.substr(b2 + 1), line_no);
            if (space_id == 0 || cell_id == 0 || version == 0) {
                bad_line(line_no, "zero id or version");
            }
            replay[space_id] = ReplaySpace{cell_id, version};  // 最后一条 A 为准
            continue;
        }
        if (line.compare(0, 2, "R ") == 0) {
            const std::string body = line.substr(2);
            if (body.find('|') != std::string::npos) {
                bad_line(line_no, "extra field");
            }
            const auto space_id = parse_u64(body, line_no);
            if (space_id == 0) {
                bad_line(line_no, "zero space id");
            }
            const auto it = replay.find(space_id);
            if (it != replay.end()) {
                it->second.cell_id = 0;  // 终态 owner 归零（版本保留）
            }
            // 无 A 的 R = 无操作（释放等价于从未认领）
            continue;
        }
        if (line.compare(0, 2, "B ") == 0) {
            const std::string body = line.substr(2);
            if (body.find('|') != std::string::npos) {
                bad_line(line_no, "extra field");
            }
            const auto epoch = parse_u64(body, line_no);
            if (epoch == 0) {
                bad_line(line_no, "zero epoch");
            }
            max_boot_epoch = std::max(max_boot_epoch, epoch);
            saw_boot_row = true;
            continue;
        }
        if (line.compare(0, 2, "C ") == 0 || line.compare(0, 2, "S ") == 0) {
            continue;
        }
        // T 行（可省前缀）：v2 带 `T ` 前缀或 v1 裸行，同解析；其余无管道
        // 行型显式损坏（防新行型被 T 兼容面误吞）。
        if (line.compare(0, 2, "T ") != 0 && line.find('|') == std::string::npos) {
            bad_line(line_no, "unknown row type");
        }
        std::string body;
        if (line.compare(0, 2, "T ") == 0) {
            body = line.substr(2);
        } else {
            body = line;
        }
        const auto bar = body.find('|');
        if (bar == std::string::npos) {
            bad_line(line_no, "missing '|'");
        }
        if (body.find('|', bar + 1) != std::string::npos) {
            bad_line(line_no, "extra field");
        }
        const auto cell_id = parse_u64(body.substr(0, bar), line_no);
        const auto epoch = parse_u64(body.substr(bar + 1), line_no);
        if (cell_id == 0 || epoch == 0) {
            bad_line(line_no, "zero id or epoch");
        }
        auto& wm = cellEpochWatermarks_[cell_id];
        if (epoch > wm) {
            wm = epoch;
        }
    }
    // B 行铸代次（设计稿 §5 步骤3）：重启 = 最大 B + 1；首启无 B 行 = 配置
    // 代次即初始代次（写首个 B 行——先记后开受理面，本函数先于链路装配）。
    // 自铸代次（配置 0）且无 B 行 = 无从记，沿用进程自铸（每启动自然不同）。
    // 追加失败告警降级：内存代次已铸，两次启动可能同代次（loudly 可见；
    // flush/fsync 持久级别随设计稿待拍板项）。
    if (config_.m1AuthorityEpoch != 0 || saw_boot_row) {
        if (saw_boot_row && max_boot_epoch == std::numeric_limits<std::uint64_t>::max()) {
            throw std::runtime_error("world journal epoch exhausted at "
                                     + config_.worldJournalPath);
        }
        const std::uint64_t cast = saw_boot_row ? max_boot_epoch + 1
                                                : config_.m1AuthorityEpoch;
        if (!appendJournalRow("B " + std::to_string(cast))) {
            worldLog().warning("world journal B row append failed: "
                               + config_.worldJournalPath
                               + " (restart may reuse management epoch)");
        }
        config_.m1AuthorityEpoch = cast;  // 受理面统一用铸后代次（961/962 透传）
    }
    // 装载终态重建进池（批A）：仅池成员生效（非池行计数告警——池配置变更后
    // 旧记录无权威落点，逐项对账随批B）；已释放（owner=0）条目照旧重建——
    // 重认领版本 +1 依赖历史版本（新启 version 1 会倒退，ABA 回放面）。
    // 有主的条目进 pending 过期窗（driveCellRegistry 驱动释放）。
    std::size_t seeded = 0;
    std::size_t skipped = 0;
    for (const auto& entry : replay) {
        if (!pool_has(config_, entry.first)) {
            ++skipped;
            continue;
        }
        SpaceAssignment a;
        a.space_id = entry.first;
        a.cell_id = entry.second.cell_id;
        a.version = entry.second.version;
        spaceAssignments_[entry.first] = a;
        if (a.cell_id != 0) {
            pendingJournalOwners_[a.cell_id] = config_.bootAtMs;
        }
        ++seeded;
    }
    worldLog().info("world journal loaded: " + config_.worldJournalPath
                    + " watermarks=" + std::to_string(cellEpochWatermarks_.size())
                    + " spaces=" + std::to_string(seeded)
                    + (skipped != 0
                           ? " skipped_nonpool=" + std::to_string(skipped)
                           : "")
                    + " epoch=" + std::to_string(config_.m1AuthorityEpoch));
}

bool WorldApp::appendJournalRow(const std::string& row, bool durable) {
    // journal v2 追加统一入口（批A；批C 巡检代决：A 行 fsync，write-ahead
    // 决定必须先落介质；R/B/T 行 flush + 失败告警降级，沿可用性优先）。
    // journal 关 = 纯内存语义恒成功（批㉚ 已代决沿用）。调用点全在内核线程
    //（受理点/驱动钩子 = 注册表单写者上下文），无并发写者。
    if (config_.worldJournalPath.empty()) {
        return true;
    }
    std::ofstream out(config_.worldJournalPath, std::ios::app | std::ios::binary);
    if (!out) {
        return false;
    }
    out << row << '\n';
    out.flush();
    if (!out) return false;
    if (!durable) {
        return true;
    }
    const int fd = ::open(config_.worldJournalPath.c_str(), O_RDONLY);
    if (fd < 0) return false;
    const bool synced = ::fsync(fd) == 0;
    ::close(fd);
    return synced;
}

void WorldApp::recordEpochWatermark(std::uint64_t cell_id, std::uint64_t epoch) {
    // 水位记录 + 墓碑追加（L101 管理日志切片）：仅水位真实前进才落盘——
    // 同代次刷新/回拨不追加（行冗余无害但省 I/O；prev 已在日志或本进程
    // 追加过）。v1 裸行格式保留（装载面直接升级，不迁移）。追加失败只告
    // 警不拒受理：墓碑缺席时隔离退化回批㉗ 内存水位（可用性优先，loudly
    // 可见）。调用点 = onCellRegister 受理点（内核线程泵上下文，单写者）。
    const auto it = cellEpochWatermarks_.find(cell_id);
    const std::uint64_t prev = it != cellEpochWatermarks_.end() ? it->second : 0;
    cellEpochWatermarks_[cell_id] = std::max(prev, epoch);
    if (epoch <= prev) {
        return;
    }
    if (!appendJournalRow(std::to_string(cell_id) + '|' + std::to_string(epoch))) {
        worldLog().warning("world journal append failed: " + config_.worldJournalPath
                           + " (isolation degrades to in-memory watermark)");
    }
}

void WorldApp::driveCellRegistry() {
    // 注册表过期清扫（960 心跳批；post-tick 上下文 = 注册表单写者线程）：
    // last-heard（registered_at_ms，每次受理刷新）超 cellExpiryMs 未刷新 =
    // 心跳失效，整条摘除。摘除后该 cell 重注册走受理面首注册路径（登记
    // 忘却——代次墓碑随旧执行端隔离批）；cell 侧心跳 = 周期 960 重发。
    if (config_.cellExpiryMs == 0) {
        return;
    }
    const auto now_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    if (!cellRegistrations_.empty()) {
        for (auto it = cellRegistrations_.begin(); it != cellRegistrations_.end();) {
            if (now_ms - it->second.registered_at_ms > config_.cellExpiryMs) {
                worldLog().info("cell expired: id=" + std::to_string(it->first)
                                + " epoch=" + std::to_string(it->second.start_epoch)
                                + " peer=" + it->second.peer);
                releaseCellSpaces(it->first);  // 归属 Space 释放回池（L108 回收切片）
                it = cellRegistrations_.erase(it);
            } else {
                ++it;
            }
        }
    }
    // journal 已知归属主过期窗（批A 装载终态重建）：装载面重建的归属，其主
    // 未随登记面出现——窗内保持归属（不立即另端重建，设计稿 §4「过期窗不
    // 来」行），超窗未归队即释放回池（R 行）。真身已登记 = 除名（此后随
    // 登记面心跳/过期语义，不双轨）。
    for (auto it = pendingJournalOwners_.begin(); it != pendingJournalOwners_.end();) {
        if (cellRegistrations_.count(it->first) > 0) {
            it = pendingJournalOwners_.erase(it);
            continue;
        }
        if (now_ms - it->second > config_.cellExpiryMs) {
            worldLog().info("journal owner expired: cell=" + std::to_string(it->first));
            releaseCellSpaces(it->first);
            it = pendingJournalOwners_.erase(it);
        } else {
            ++it;
        }
    }
    // 在途关联账过期清扫（同窗）：cell 回执超窗未到即撤账显式回超时——
    // 与注册表摘除同一裁决窗，不引入第二套过期口径。
    sweepExpiredCellReplyAccounts(now_ms);
}

void WorldApp::onM1DebugEval(const std::string& peer, const DebugEvalRequest& request) {
    // 944 受理（§7.1 attach transport；管理泵上下文）：切片2 转发——World
    // 只承载管理命令，debug eval 经注册 cell 连接反向发送；cell 宿主处理后
    // 回 945（onCellEvalResult 按原 fleetctl peer 回传）。无合格 cell = 即时
    // 回执。
    if (request.eval_id == 0) {
        return;  // 0 保留（admin 关联键缺位，拒收面都不给）
    }
    if (evalReplyPeers_.size() >= kM1EvalBacklogMax) {
        sendM1EvalReply(peer, request.eval_id, false, "", "eval backlog full");
        return;
    }
    const auto cell_peer = firstRegisteredCellPeer();
    if (cell_peer.empty()
        || !m1Link_->sendCellDebugEvalRequest(cell_peer, request)) {
        sendM1EvalReply(peer, request.eval_id, false, "", "simulation not hosted");
        return;
    }
    evalReplyPeers_[request.eval_id] = PendingCellReply{
        peer, cell_peer, steady_now_ms()};
}

void WorldApp::onM1ModuleRollout(const std::string& peer,
                                 const ModuleRolloutRequest& request) {
    // 952 受理：切片2 转发——World 只承载管理命令，模块投递经注册 cell
    // 连接反向发送；cell 宿主处理后回 953（onCellRolloutReply 按原
    // fleetctl peer 回传）。无合格 cell = 即时回执。
    if (request.request_id == 0) {
        return;
    }
    const auto cell_peer = firstRegisteredCellPeer();
    if (cell_peer.empty()
        || !m1Link_->sendCellModuleRolloutRequest(cell_peer, request)) {
        m1Link_->sendModuleRolloutReply(peer, request.request_id, request.op,
                                        false, -1, 0, 0,
                                        "script runtime not ready");
        return;
    }
    rolloutReplyPeers_[request.request_id] = PendingCellReply{
        peer, cell_peer, steady_now_ms()};
}

void WorldApp::onM1WorldDrain(const std::string& peer,
                              const WorldDrainRequest& request) {
    // 956 受理：切片2 转发——World 只承载管理命令，排空命令经注册 cell
    // 连接反向发送；cell 宿主处理后回 957（onCellDrainReply 按原
    // fleetctl peer 回传）。无合格 cell = 即时回执。
    if (request.request_id == 0) {
        return;
    }
    const auto cell_peer = firstRegisteredCellPeer();
    if (cell_peer.empty()
        || !m1Link_->sendCellWorldDrainRequest(cell_peer, request)) {
        m1Link_->sendWorldDrainReply(peer, request.request_id, request.op,
                                     false, 0, 0, "squad service not ready");
        return;
    }
    drainReplyPeers_[request.request_id] = PendingCellReply{
        peer, cell_peer, steady_now_ms()};
}

void WorldApp::sendM1EvalReply(const std::string& peer, std::uint64_t eval_id, bool ok,
                                 const std::string& result, const std::string& error) {
    if (m1Link_) {
        m1Link_->sendEvalResult(peer, eval_id, ok, result, error);
    }
}

void WorldApp::onCellEvalResult(const std::string& /*peer*/,
                                const CellEvalResult& result) {
    // 切片2 转发收账：cell 宿主回 945（经注册连接），按 eval_id 关联原
    // fleetctl/admin peer 后回传。关联键缺位/未知 = 静默弃（尽力面）。
    const auto it = evalReplyPeers_.find(result.eval_id);
    if (it == evalReplyPeers_.end()) {
        return;
    }
    sendM1EvalReply(it->second.fleetctl_peer, result.eval_id, result.ok, result.result,
                    result.error);
    evalReplyPeers_.erase(it);
}

void WorldApp::onCellRolloutReply(const std::string& /*peer*/,
                                  const CellRolloutReply& reply) {
    const auto it = rolloutReplyPeers_.find(reply.request_id);
    if (it == rolloutReplyPeers_.end() || !m1Link_) {
        return;
    }
    m1Link_->sendModuleRolloutReply(it->second.fleetctl_peer, reply.request_id, reply.op,
                                    reply.ok, reply.state, reply.window_faults,
                                    reply.module_version, reply.detail);
    rolloutReplyPeers_.erase(it);
}

void WorldApp::onCellDrainReply(const std::string& /*peer*/,
                                const CellDrainReply& reply) {
    const auto it = drainReplyPeers_.find(reply.request_id);
    if (it == drainReplyPeers_.end() || !m1Link_) {
        return;
    }
    m1Link_->sendWorldDrainReply(it->second.fleetctl_peer, reply.request_id, reply.op,
                                 reply.ok, reply.state, reply.active_instances,
                                 reply.detail);
    drainReplyPeers_.erase(it);
}

void WorldApp::onCellIntentAck(const std::string& /*peer*/,
                               const CellIntentAck& ack) {
    // 切片2 意图转发回程：cell 宿主的 929 经 gateway_link() 转发 gateway
    //（sendIntentAck 已按 gateway_link 寻路）。0 关联键 = 拒收面静默。
    if (m1Link_ && ack.request_id != 0) {
        m1Link_->sendIntentAck(ack.request_id, ack.code, ack.message);
    }
}

} // namespace world_app
