#include "player_store/player_store.hpp"
#include "player_store/business_wire.hpp"
#include "player_store/migration_puller.hpp"
#include "player_store/migration_wire.hpp"
#include "player_store_detail.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/game/base/avatar_reward_sink.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

namespace player_store {

namespace store_detail {

// 库面日志出口单定义（player_store 拆分单元共享——detail 头声明；
// cat=base_server（§5.1 cat= 检索键；主程已接文件面））
apollo::core::log::Logger& blog() {
    return *apollo::core::log::global_log_manager().createLogger("base_server");
}
} // namespace store_detail

namespace {

// 库面日志出口（detail 单定义，本单元转发——定义在 player_store.cpp）
apollo::core::log::Logger& blog() {
    return store_detail::blog();
}

} // namespace

//==============================================================================
// PlayerStore 实现
//==============================================================================

PlayerStore::PlayerStore(const PlayerStoreConfig& config,
                         std::unique_ptr<PlayerStorage> storage)
    : config_(config)
    , database_(storage ? std::move(storage) : std::make_unique<DatabaseService>(config))
    , saveQueue_(std::make_unique<SaveQueue>(config_.workerThreads, config_.saveQueueCapacity))
    , avatarManager_(std::make_shared<apollo::game::base::AvatarManager>())
    , sessionLocator_(std::make_shared<apollo::game::session::SessionLocator>())
    , journal_(database_->ownsJournal() ? nullptr :
          std::make_unique<apollo::data::journal::PersistJournal>(config_.journalPath)) {
    // P1-4 真链路：SaveQueue 出队 → DatabaseService 落盘（workerLoop
    // 此前只 callback(true) 不写任何介质）
    saveQueue_->set_worker([this](const PlayerData& data) {
        return database_->savePlayer(data);
    });
}

std::uint64_t PlayerStore::nextWriteGen(PlayerID playerId,
                                        const std::uint64_t* seed_hint) {
    auto it = writeGens_.find(playerId);
    if (it != writeGens_.end()) {
        return ++it->second;
    }
    // 缺账播种：调用方已持真源（接权快照/首载基线）取其大，必要时补一次
    // 现档读——接管旧进程无本进程账，迟到标记值必 ≤ database 地板被拒
    std::uint64_t seed = seed_hint != nullptr ? *seed_hint : 0;
    PlayerData landed{};
    if (database_->loadPlayer(playerId, landed)) {
        seed = std::max(seed, landed.writeGen);
    }
    return ++writeGens_.emplace(playerId, seed).first->second;
}

std::uint64_t PlayerStore::appendJournal(const PlayerData& data) {
    if (!journal_) return 0; // Remote database owns its journal and write ordering.
    // write-ahead：先落 journal（恢复位点），再经 SaveQueue 异步落档；
    // 返回段全局序供落档水位记账（SaveTask::journalSeq）
    return journal_->append(std::to_string(data.playerId), data.toJson(),
                            getCurrentTimeMs());
}

std::size_t PlayerStore::drainJournal() {
    if (!journal_) return 0;
    // journal 消费侧（P1-5）：定额出队，把已落 journal 的载荷压到档案；
    // 载荷为全量快照，重放幂等。落盘一律经 SaveQueue 同玩家槽位（与
    // flush/RPC 同一串行点，直接落盘会与队列在途任务竞写同档）；队列
    // 已停（停机追平面）改同步直写——此拍生产者已全停，单线程安全。
    // 同玩家槽位忙时入队被拒：条目顶回 journal 队首，下轮重试（写序
    // 保持：同槽先入队者先落盘）
    return journal_->drain(
        [this](const apollo::data::journal::JournalEntry& e) {
            PlayerData data;
            try {
                data = PlayerData::fromJson(e.payload);
                data.playerId = static_cast<PlayerID>(std::stoull(e.key));
            } catch (const std::exception&) {
                return false;  // 脏条目保留，人工排查
            }
            std::lock_guard<std::mutex> lock(storeMutex_);
            // 只读态玩家（迁移已交权/接权排干窗）：档案不由本域队列写
            // （无双写不变量；冻结/接权段已同步直写收口，条目消费即可）
            if (const auto avatar = avatarManager_->find(data.playerId);
                avatar &&
                avatar->state() == apollo::game::base::AvatarState::Transferring) {
                return true;
            }
            // 接权前旧域条目（seq < 水位）：消费不落档——接权直写已是终值
            if (const auto wm = adoptWatermarks_.find(data.playerId);
                wm != adoptWatermarks_.end() && e.sequence < wm->second) {
                return true;
            }
            // 已落档水位：条目载荷 ≤ 既有落盘 = 旧快照——消费不倒灌；
            // > 水位 = 被合并跳过的唯一载荷载体，经队列落档（journalSeq
            // 随任务记账，落档后同序条目自然让位）
            if (const auto it = landedSeqs_.find(data.playerId);
                it != landedSeqs_.end() && e.sequence <= it->second) {
                return true;
            }
            if (saveQueue_->is_running()) {
                SaveTask task;
                task.playerId = data.playerId;
                task.data = std::move(data);
                task.journalSeq = e.sequence;
                task.createdAtMs = getCurrentTimeMs();
                return saveQueue_->enqueue(task);
            }
            return database_->savePlayer(data);
        },
        static_cast<std::size_t>(config_.journalDrainQuota));
}

PlayerStore::~PlayerStore() {
    stop();
}

void PlayerStore::start() {
    if (running_) return;
    // 停机位单向（G-3）：stop 后本实例不再服务——重启语义归进程重启，
    // 防止复位后静默落入维护中受理
    if (shuttingDown_) {
        throw std::runtime_error("PlayerStore cannot restart after stop (process restart instead)");
    }

    // 初始化数据库
    if (!database_->initialize()) {
        throw std::runtime_error("Failed to initialize database");
    }

    // 启动序列 restore→admission→ready（P1-5，lifecycle §3）：journal
    // replay 恢复未落档位点 → 准入检查（数据面可服务）→ Ready 才开 RPC
    if (journal_ && !journal_->open()) {
        throw std::runtime_error("Failed to open storage journal");
    }
    recovery_ = std::make_unique<apollo::game::base::RecoveryCoordinator>(
        "base-app",
        [this]() -> std::size_t {
            if (!journal_) return 0;
            return journal_->replay([this](const apollo::data::journal::JournalEntry& e) {
                PlayerData data;
                try {
                    data = PlayerData::fromJson(e.payload);
                    data.playerId = static_cast<PlayerID>(std::stoull(e.key));
                } catch (const std::exception&) {
                    return false;
                }
                return database_->savePlayer(data);
            });
        },
        [this]() {
            // Avatar 恢复（档案在、会话无，Offline 待登录）+ 数据面准入
            std::vector<PlayerID> ids;
            if (!database_->listPlayers(ids)) return false;
            const auto restored =
                apollo::game::base::restore_avatars(*avatarManager_, ids);
            blog().info("Avatar restore: " + std::to_string(restored)
                        + " avatars offline-resumed");
            // 恢复锚点播种（真实奖励持久化）：flush 收集以 Avatar 为 exp 真源——
            // 未播种的恢复锚首次 flush 会把现档 exp 覆写归零
            for (const auto id : ids) {
                PlayerData profile;
                if (!database_->loadPlayer(id, profile)) continue;
                if (auto avatar = avatarManager_->find(id)) {
                    avatar->add_progress(
                        apollo::game::base::AvatarRewardSink::kExpKey,
                        profile.exp);
                    avatar->clear_dirty();
                }
            }
            return true;
        });
    if (!recovery_->run() || !recovery_->ready()) {
        throw std::runtime_error(std::string("Recovery failed at phase: ") +
                                 apollo::game::base::to_string(recovery_->phase()));
    }

    // 启动保存队列
    saveQueue_->start();

    running_ = true;

    // M1 base 受理面（P4-4 批③）：m1Port 非零 = link M1 面（dispatchRequest
    // 直通——nng REP 已随批⑤退役[桩传输死面]）；装配失败降级（警告 + 不拦
    // 启动，RPC 面缺席）。
    if (config_.m1Enabled || config_.m1Port != 0) {
        PlayerStoreLinkServerConfig m1Config;
        m1Config.host = config_.host;
        m1Config.port = config_.m1Port;
        m1Config.route_json = config_.m1RouteJson;
        m1Config.self_name = config_.m1SelfName;
        m1Config.endpoint_name = config_.m1EndpointName;
        m1Server_ = makePlayerStoreLinkServer(
            std::move(m1Config), [this](const std::uint8_t* frame, std::size_t size) {
                try {
                    return dispatchRequest(std::vector<std::uint8_t>(frame, frame + size));
                } catch (const std::exception&) {
                    protocol::ErrorMessage error{};
                    error.code = 1;
                    error.message = "Base request rejected";
                    return protocol::MessageCodec::encode(error);
                }
            });
        if (m1Server_->start()) {
            blog().info("M1 base face listening on " + config_.host + ":"
                        + std::to_string(m1Server_->listenPort()));
        } else {
            blog().warning("M1 base face degraded: route table unavailable ("
                           + config_.m1RouteJson + "), rpc face stays off");
            m1Server_.reset();
        }
    }

    // 启动自动保存线程
    autoSaveThread_ = std::thread(&PlayerStore::autoSaveLoop, this);

    blog().info("Base storage ready; M1 port=" + std::to_string(m1_listen_port()));
}

void PlayerStore::stop() {
    // 幂等闸（G-3）：信号处理器与析构可能先后各调一次——CAS 保证五阶段
    // 只走一遍（此前二次调用会重复 flush 与 double database shutdown）
    bool expected = false;
    if (!shuttingDown_.compare_exchange_strong(expected, true)) {
        return;
    }

    // ① 停收新请求（§10.2-①）：维护位已立（dispatchRequest 回维护中），
    // M1 面收线（泵线程停驱 + 监听拆线；§10.2-④ 的 net 断开同拍语义）。
    running_ = false;
    if (m1Server_) {
        m1Server_->stop();
        m1Server_.reset();
    }

    // ② 最后一轮变更进 journal（§10.2-②）：先停自动保存生产者（此前
    // join 排在队列 drain 之后——生产者仍可能在 drain 后再入队的竞序），
    // 再全量 flush 脏锚点（flush 先 journal 后入队，write-ahead 语义）
    if (autoSaveThread_.joinable()) {
        autoSaveThread_.join();
    }
    flushDirtyAvatars("shutdown_flush");

    // ③ 持久链追平（§10.2-③）：队列全量排空（SaveQueue::stop 的 worker
    // 至队空方退）→ journal 循环 drain 至 pending==0，带超时上限——超时
    // 告警并继续（日志完整性优先于停机速度：残留条目留待重启 replay，幂等）
    saveQueue_->stop();
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(config_.shutdownFlushTimeoutMs);
    while ((journal_ ? journal_->pending() : 0) > 0) {
        const std::size_t applied = drainJournal();
        if ((journal_ ? journal_->pending() : 0) == 0) {
            break;
        }
        if (applied == 0) {
            // 定额内零进展：sink 拒绝（脏条目）——超时判定后告警放行
            if (std::chrono::steady_clock::now() >= deadline) {
                blog().error("journal drain timeout, " + std::to_string((journal_ ? journal_->pending() : 0))
                             + " entries left for restart replay");
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // ⑤ 模块按依赖逆序停（§10.2-⑤）：存储最内层最后关
    database_->shutdown();
    blog().info("Base server stopped (journal pending: "
                + std::to_string((journal_ ? journal_->pending() : 0)) + ")");
}

void PlayerStore::autoSaveLoop() {
    while (running_) {
        // 分片睡等（G-3）：running_ 翻 false 最迟一个分片内可观察——停机
        // join 不吃整个 autoSaveIntervalMs（§10.2 停机时延兜底；此前整段
        // sleep 使 SIGTERM 停机最长挂一个保存周期）
        int waited = 0;
        while (running_ && waited < config_.autoSaveIntervalMs) {
            const int slice = config_.autoSaveIntervalMs - waited < 50
                                  ? config_.autoSaveIntervalMs - waited
                                  : 50;
            std::this_thread::sleep_for(std::chrono::milliseconds(slice));
            waited += slice;
        }

        // 自动保存（P0-4「Saving 真做」）：把脏锚点送入保存队列（六态
        // Disconnected/Saving 由 flush 与回调维护），不再仅打印。
        flushDirtyAvatars("auto_save");

        // journal 消费侧（P1-5）：定额出队压档（attribute-sync §8.2
        // 每 tick 定比、有界队列）
        drainJournal();
    }
}

std::size_t PlayerStore::flushDirtyAvatars(const char* reason) {
    // 单临界区（串行化批）：态判定、基线读档、exp 收集、入段与入队原子
    // ——基线在锁内取（DatabaseService 缓存随每次 save 同步刷新，命中
    // 即取，无盘面 I/O 占锁；激活时已首载入缓存），锁内基线 + 锁内入队
    // 消灭「RPC 任务在基线读取与入队之间完成 → 旧字段覆盖新落档」的
    // 过期窗。锚点创建必经 activatePlayer（首载），故此处恒缓存命中。
    std::lock_guard<std::mutex> lock(storeMutex_);
    std::size_t flushed = 0;
    for (auto& avatar : avatarManager_->snapshot()) {
        if (!avatar || !avatar->needs_save()) {
            continue;
        }
        // 迁移中锚点跳过（P0-2 路 2 ① Transferring = 只读态）：写权已交
        // 新进程，老进程不再落盘（无双写不变量，§9.2）
        if (avatar->state() == apollo::game::base::AvatarState::Transferring) {
            continue;
        }

        SaveTask task;
        task.playerId = avatar->player_id();
        // 载荷以现档为基线（P1-4 落盘真做后，默认值载荷会清档）：
        // 无档即跳过——不凭空造档案（bootstrap 恶龙已废）。
        if (!database_->loadPlayer(task.playerId, task.data)) {
            blog().warning("Flush skipped for player " + std::to_string(task.playerId)
                            + " (no profile) (" + reason + ")");
            continue;
        }
        // exp 收集自 Avatar 长期态（真实奖励持久化）：奖励落锚点即真源，
        // flush 覆盖写回——不再仅重写旧档案丢增量（播种见 activatePlayer/
        // 启动恢复/adoptPlayer）。
        task.data.exp = avatar->progress(
            apollo::game::base::AvatarRewardSink::kExpKey);
        if (database_->ownsJournal()) {
            // 写端自持代次（L64 Base 半；database 半验单调）——基线即现档，
            // 作缺账播种真源，不另发读
            task.data.writeGen = nextWriteGen(task.playerId, &task.data.writeGen);
            if (database_->savePlayer(task.data)) {
                avatar->clear_dirty();
                ++flushed;
            }
            continue;
        }
        task.createdAtMs = getCurrentTimeMs();

        // 先置 Saving 再入段/入队（worker finalize 才有 Saving 可回写；
        // 后置会与快速完成竞态把锚点卡死 Saving）
        const auto prev = avatar->state();
        avatar->set_state(apollo::game::base::AvatarState::Saving);
        // write-ahead（P1-5）：同 handleDbSaveRequest——合并被拒时载荷
        // 由 journal 段承载（下轮 drain 经队列落档），锚点留脏
        task.journalSeq = appendJournal(task.data);
        task.callback = [this, playerId = task.playerId,
                         seq = task.journalSeq](bool success) {
            finalizeSave(playerId, success, seq);
        };
        if (!saveQueue_->enqueue(task)) {
            avatar->set_state(prev);  // 同玩家在途：合并跳过，下轮再取
            continue;
        }
        ++flushed;
        blog().info("Flush queued for player " + std::to_string(task.playerId)
                    + " (" + reason + ")");
    }
    return flushed;
}

int64_t PlayerStore::getCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

std::shared_ptr<apollo::game::base::Avatar> PlayerStore::activateAvatar(PlayerID playerId) {
    auto avatar = avatarManager_->activate(playerId);
    if (!avatar) {
        return nullptr;
    }

    if (avatar->state() == apollo::game::base::AvatarState::Loading) {
        avatar->set_state(apollo::game::base::AvatarState::Online);
    }

    return avatar;
}

void PlayerStore::finalizeSave(PlayerID playerId, bool success, std::uint64_t journalSeq) {
    // 串行化：态守卫判定与回写同一临界区——与 freeze/adopt 的只读闸原子
    // 交错，不得覆盖 Transferring（双写不变量破口的收口）
    std::lock_guard<std::mutex> lock(storeMutex_);
    auto avatar = avatarManager_->find(playerId);
    if (!avatar) {
        return;
    }
    // 落档水位记账（成功才推进）：journal 回放序 ≤ 此序的条目已被本任务
    // 载荷（或更新者）覆盖——drain 消费不落档，旧快照不倒灌
    if (success && journalSeq != 0) {
        auto& landed = landedSeqs_[playerId];
        if (journalSeq > landed) {
            landed = journalSeq;
        }
    }

    // 态守卫（P0-2 路 2 ①）：SaveTask 在途期间锚可能已被迁移停写置
    // Transferring（freeze 与 autoSave 的竞态窗口）——finalize 不得覆盖
    // 只读态（推回 Online = 双写不变量破口）。仅 Saving 态回写结果态；
    // 其它态只清/标脏，态留给持有者。
    if (avatar->state() != apollo::game::base::AvatarState::Saving) {
        if (success) {
            avatar->clear_dirty();
        } else {
            avatar->mark_dirty("save_failed");
        }
        return;
    }

    if (success) {
        avatar->clear_dirty();
        avatar->set_state(apollo::game::base::AvatarState::Online);
        return;
    }

    avatar->mark_dirty("save_failed");
    avatar->set_state(apollo::game::base::AvatarState::Disconnected);
}

} // namespace player_store

