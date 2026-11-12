#include "world_app/world_app.hpp"
#include "world_app_detail.hpp"
#include <chrono>

namespace world_app {

namespace {

// 库面日志出口（detail 单定义，本单元转发——定义在 world_app.cpp）
apollo::core::log::Logger& worldLog() {
    return detail::world_log();
}

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
} // namespace

//==============================================================================
// cell 登记与缩容（961 受理裁决 / 断链存疑 / 心跳摘除 / 963 缩容编排）
//==============================================================================

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
    // 启动凭据自证（L109 认证凭据面）：World config.registerToken 非空时，
    // cell 上报的 credential 须逐字节等于该值才受理——部署方经环境注入的
    // 凭据源拒未持凭据/错凭据的伪造注册。空配置 = 认证关（任何值均可，
    // 含未声明，同 944/952/956 裸帧惯例）。凭据不落可公开日志（detail 只
    // 给布尔面）。常量时间比较：非定长 std::string operator!= 首字节不同即
    // 短路，可经计时侧信道逐字节枚举（见 constant_time_string_equal）。
    // 凭据裁决先于代次/兼容裁决：未持/错凭据的探测者一律 code 6，不因
    // cell_id 是否已登记或其启动代次泄露任何登记状态（逐 cell_id 枚举无
    // oracle）；code 6 在 cell 侧触发自冻结（cell_register_client 冻结条
    // 件），故旧代次执行端凭据过期/轮换仍得隔离信号，不因 code 3/5 之外
    // 抢答丢失。
    if (!config_.registerToken.empty()
        && !constant_time_string_equal(request.credential, config_.registerToken)) {
        reply(false, kCellRegisterRejectedCredential, "register credential mismatch");
        return;
    }
    auto it = cellRegistrations_.find(request.cell_id);
    // 代次裁决先于兼容裁决（code 3 先于 code 5）：旧代次复活凭 code 3 自冻
    // 结隔离（客户端语义）——若被 code 5 抢答，旧执行端失去自隔离信号。
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

std::size_t WorldApp::suspect_cell_count() const {
    std::size_t count = 0;
    for (const auto& entry : cellRegistrations_) {
        if (entry.second.suspect) {
            ++count;
        }
    }
    return count;
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

void WorldApp::driveCellRegistry() {
    // 执行位置投影 seed（L115，首拍一次）：journal 装载的已提交归属灌入发布
    // 镜像——链路 kUp 跃迁全量重放补投目录（World 重启窗的目录重建面，D05
    // §1.2）。发布面缺席（未注入/面关）= 待命清零不重试，零噪声。
    if (projectionSeedPending_) {
        projectionSeedPending_ = false;
        if (projection_ != nullptr) {
            for (const auto& entry : spaceAssignments_) {
                if (entry.second.cell_id != 0) {
                    publishProjection(entry.first, entry.second.cell_id,
                                      entry.second.version, true);
                }
            }
        }
    }
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

} // namespace world_app
