// Battle Offload 测试（P3-1 批 2：offload 骨架 · 内存桩过渡）。
//
// 覆盖：
//   1. wire 编解码：五类消息 round-trip（契约字段序逐字段小端）+ 严格解码
//      （截断/尾部余字节/族外 id 整包拒）；
//   2. spawn 生命周期全链（内存桩两端直连）：request → ack-on-accept →
//      notify_ready（目录 Advertise 消费钩子）→ Running → 输入/投影/落回；
//   3. 落回单向（Result Dispatcher，§3）：result.player_ids/scores 逐玩家
//      进宿主 IRewardSink；replay_hash = 实例侧 hash 链尾；
//   4. 投影水位：snapshot_seq 严格递增，重复/倒退拒（attribute-sync 同源）；
//   5. 非法跃迁：重复 spawn/提前 ready/提前落回/平行数组不等长 全拒；
//   6. 崩溃即作废（§7(a)）：abandon → notify_crash → Voided，落回不再受理；
//   7. 受理拒绝路径：accepted=0 → Rejected 终态；
//   8. 跨边界确定性（§6）：同 seed 同输入，offload 链路 hash == 进程内直跑；
//   9. 自灭与单实例：settle 后自灭（宿主收割语义），二拉拒。

#include "apollo/game/battle/battle_runtime.hpp"
#include "apollo/game/battle/offload_host.hpp"
#include "apollo/game/battle/offload_node.hpp"

#include <iostream>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#define TEST_ASSERT(cond, msg)                                                           \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")" \
                      << std::endl;                                                      \
            return false;                                                                \
        }                                                                                \
    } while (0)

namespace {

using apollo::game::battle::BattleRuntime;
using apollo::game::battle::IRewardSink;
using apollo::game::battle::offload::BattleInputMsg;
using apollo::game::battle::offload::BattleResult;
using apollo::game::battle::offload::BattleStateSync;
using apollo::game::battle::offload::InstanceOffloadHost;
using apollo::game::battle::offload::InstanceOffloadNode;
using apollo::game::battle::offload::InstanceSpawnReply;
using apollo::game::battle::offload::InstanceSpawnRequest;
using apollo::game::battle::offload::MemoryOffloadTransport;
using apollo::game::battle::offload::OffloadEnvelope;
using MsgId = apollo::contract::gen::MsgId;
namespace acg = apollo::contract::gen;

// 宿主侧落回收集 sink（生产 = AvatarRewardSink；此处记序验证）
struct CollectingSink : public IRewardSink {
    std::vector<std::pair<std::uint64_t, std::int64_t>> rewards;
    void on_reward(std::uint64_t player_id, std::int64_t score) override {
        rewards.emplace_back(player_id, score);
    }
};

// 内存桩两端直连：host 下行 → node 上行（同步直呼，无队列）
struct Harness {
    MemoryOffloadTransport downlink; // host → node
    MemoryOffloadTransport uplink;   // node → host
    CollectingSink sink;
    std::unique_ptr<InstanceOffloadHost> host;
    std::unique_ptr<InstanceOffloadNode> node;

    explicit Harness(std::uint64_t world_seed = 42) {
        host = std::make_unique<InstanceOffloadHost>(downlink, &sink);
        node = std::make_unique<InstanceOffloadNode>(uplink, world_seed);
        downlink.attach([this](const OffloadEnvelope& e) { node->on_envelope(e); });
        uplink.attach([this](const OffloadEnvelope& e) { host->on_envelope(e); });
    }
};

InstanceSpawnRequest spawn_req(std::uint64_t instance_id) {
    InstanceSpawnRequest req;
    req.instance_id = instance_id;
    req.scene_id = 7;
    req.map_id = 1001;
    req.tick_interval_ms = 33; // 20-50ms 可配档位
    req.participant_count = 2;
    req.spawn_args = "battle-template";
    return req;
}

BattleInputMsg input_msg(std::uint64_t instance_id, std::uint64_t player_id,
                         std::int64_t tick, std::vector<std::int64_t> opcodes) {
    BattleInputMsg in;
    in.instance_id = instance_id;
    in.player_id = player_id;
    in.tick = tick;
    in.opcodes = std::move(opcodes);
    return in;
}

// 就绪前送输入（Host 侧 Running 前拒）
bool spawn_to_running(Harness& h, std::uint64_t instance_id) {
    if (!h.host->request_spawn(spawn_req(instance_id))) {
        return false;
    }
    if (h.host->phase(instance_id) != InstanceOffloadHost::InstancePhase::Accepted) {
        return false;
    }
    return h.host->notify_ready(instance_id);
}

// ---- 1. wire 编解码 ----

bool test_wire_roundtrip() {
    InstanceSpawnRequest req = spawn_req(77);
    req.spawn_args = "tpl --port 0 \0x"; // 含 NUL 的参数串
    InstanceSpawnRequest back;
    TEST_ASSERT(apollo::game::battle::offload::decode(
                    apollo::game::battle::offload::encode(req), back),
                "spawn_request round-trip");
    TEST_ASSERT(back.instance_id == 77 && back.scene_id == 7 && back.map_id == 1001 &&
                    back.tick_interval_ms == 33 && back.participant_count == 2 &&
                    back.spawn_args == req.spawn_args,
                "spawn_request 字段全等");

    InstanceSpawnReply reply;
    reply.instance_id = 78;
    reply.accepted = true;
    reply.error_message = "";
    InstanceSpawnReply reply_back;
    TEST_ASSERT(apollo::game::battle::offload::decode(
                    apollo::game::battle::offload::encode(reply), reply_back),
                "spawn_reply round-trip");
    TEST_ASSERT(reply_back.instance_id == 78 && reply_back.accepted &&
                    reply_back.error_message.empty(),
                "spawn_reply 字段全等");

    BattleInputMsg in = input_msg(1, 9001, 5, {1, 2, 3});
    BattleInputMsg in_back;
    TEST_ASSERT(apollo::game::battle::offload::decode(
                    apollo::game::battle::offload::encode(in), in_back),
                "battle_input round-trip");
    TEST_ASSERT(in_back.opcodes.size() == 3 && in_back.tick == 5 &&
                    in_back.player_id == 9001,
                "battle_input 字段全等");

    BattleStateSync sync;
    sync.instance_id = 1;
    sync.tick = 6;
    sync.snapshot_seq = 3;
    sync.players = {9001, 9002};
    sync.payload = {0x01, 0x02};
    BattleStateSync sync_back;
    TEST_ASSERT(apollo::game::battle::offload::decode(
                    apollo::game::battle::offload::encode(sync), sync_back),
                "battle_state_sync round-trip");
    TEST_ASSERT(sync_back.snapshot_seq == 3 && sync_back.players.size() == 2 &&
                    sync_back.payload.size() == 2,
                "battle_state_sync 字段全等");

    BattleResult result;
    result.instance_id = 1;
    result.tick = 9;
    result.replay_hash = -1; // hash 链尾可为任意 uint64 位型
    result.player_ids = {9001, 9002};
    result.scores = {120, 0};
    BattleResult result_back;
    TEST_ASSERT(apollo::game::battle::offload::decode(
                    apollo::game::battle::offload::encode(result), result_back),
                "battle_result round-trip");
    TEST_ASSERT(result_back.replay_hash == -1 && result_back.player_ids.size() == 2 &&
                    result_back.scores[1] == 0,
                "battle_result 字段全等");
    return true;
}

bool test_wire_strict() {
    // 截断拒：spawn_request 少 1 字节
    const auto full = apollo::game::battle::offload::encode(spawn_req(1));
    InstanceSpawnRequest req;
    TEST_ASSERT(!apollo::game::battle::offload::decode(
                    std::vector<std::uint8_t>(full.begin(), full.end() - 1), req),
                "截断整包拒");
    // 尾部余字节拒
    auto padded = full;
    padded.push_back(0);
    TEST_ASSERT(!apollo::game::battle::offload::decode(padded, req), "尾部余字节拒");
    // 空载荷拒
    TEST_ASSERT(!apollo::game::battle::offload::decode({}, req), "空载荷拒");

    // 信封：族外 id 拒（client 域 move=10 / 邻族 918/924）
    OffloadEnvelope env;
    OffloadEnvelope bad = env;
    bad.msg = static_cast<MsgId>(10);
    TEST_ASSERT(!apollo::game::battle::offload::decode_envelope(
                    apollo::game::battle::offload::encode_envelope(bad), env),
                "client 域 id 拒");
    bad.msg = static_cast<MsgId>(918);
    TEST_ASSERT(!apollo::game::battle::offload::decode_envelope(
                    apollo::game::battle::offload::encode_envelope(bad), env),
                "918 邻族拒");
    bad.msg = static_cast<MsgId>(924);
    TEST_ASSERT(!apollo::game::battle::offload::decode_envelope(
                    apollo::game::battle::offload::encode_envelope(bad), env),
                "924 越界拒");
    // 族内通行 + 拆包保载荷
    env.msg = MsgId::battle_result;
    env.payload = {1, 2, 3};
    OffloadEnvelope env_back;
    TEST_ASSERT(apollo::game::battle::offload::decode_envelope(
                    apollo::game::battle::offload::encode_envelope(env), env_back),
                "族内信封通行");
    TEST_ASSERT(env_back.msg == MsgId::battle_result && env_back.payload.size() == 3,
                "信封载荷保真");
    // 空截断
    TEST_ASSERT(!apollo::game::battle::offload::decode_envelope({}, env_back),
                "信封空截断拒");
    return true;
}

// ---- 2. spawn 生命周期全链 ----

bool test_lifecycle_full_chain() {
    Harness h;
    const std::uint64_t id = 100;
    TEST_ASSERT(h.host->request_spawn(spawn_req(id)), "request_spawn 受理");
    // ack-on-accept：同步回流 → Accepted（ADR-017：受理 ≠ 就绪）
    TEST_ASSERT(h.host->phase(id) == InstanceOffloadHost::InstancePhase::Accepted,
                "受理回执即 Accepted");
    TEST_ASSERT(h.node->spawned() && h.node->instance_id() == id &&
                    h.node->tick_interval_ms() == 33,
                "节点承载 spawn 参数");
    // 就绪前：输入/投影/落回均不受理
    TEST_ASSERT(!h.host->send_battle_input(input_msg(id, 9001, 0, {1})),
                "Accepted 态拒输入");
    // notify_ready（目录 Advertise 消费钩子）→ Running
    TEST_ASSERT(h.host->notify_ready(id), "notify_ready");
    TEST_ASSERT(h.host->phase(id) == InstanceOffloadHost::InstancePhase::Running,
                "就绪后 Running");

    // 输入注入 → 实例 tick 推进 → 投影回流
    TEST_ASSERT(h.host->send_battle_input(input_msg(id, 9001, 0, {1})),
                "Running 态收输入");
    TEST_ASSERT(h.host->send_battle_input(input_msg(id, 9002, 0, {1})),
                "第二名玩家输入");
    TEST_ASSERT(h.node->tick(0), "首拍推进");
    TEST_ASSERT(h.host->last_snapshot_seq(id) == 1, "投影水位=1");
    TEST_ASSERT(h.node->last_tick() == 0 && h.node->snapshot_seq() == 1,
                "实例 tick 轴起步");
    // 输入对齐实例 tick 轴：未来拍滞留、过去拍丢弃
    TEST_ASSERT(h.host->send_battle_input(input_msg(id, 9001, 2, {1})), "未来拍入队");
    TEST_ASSERT(h.node->tick(1), "第二拍");
    TEST_ASSERT(h.node->pending_inputs() == 1, "未来拍滞留");
    TEST_ASSERT(h.node->tick(2), "第三拍");
    TEST_ASSERT(h.node->pending_inputs() == 0, "到拍消费");
    return true;
}

// ---- 3. 落回单向 ----

bool test_result_fallback() {
    Harness h;
    const std::uint64_t id = 200;
    TEST_ASSERT(spawn_to_running(h, id), "就绪全链");
    // 双玩家各打一拍（懒入场：输入首现即 enter；分数=命中伤害合计，掷点
    // 决定——精确值对照归 test_cross_boundary_determinism）
    TEST_ASSERT(h.host->send_battle_input(input_msg(id, 9001, 0, {1})),
                "9001 输入");
    TEST_ASSERT(h.host->send_battle_input(input_msg(id, 9002, 0, {1})),
                "9002 输入");
    TEST_ASSERT(h.node->tick(0), "首拍推进");
    TEST_ASSERT(h.node->settle(), "结算落回");
    TEST_ASSERT(h.node->finished(), "实例自灭");
    TEST_ASSERT(h.host->phase(id) == InstanceOffloadHost::InstancePhase::Settled,
                "宿主终态 Settled");
    // 落回平行数组进宿主 sink（IRewardSink 语义；全参战者含 0 分）
    TEST_ASSERT(h.sink.rewards.size() == 2, "两名玩家逐个落账");
    // settle_rewards 按 player_id 全序落账（约束③）
    TEST_ASSERT(h.sink.rewards[0].first == 9001 && h.sink.rewards[1].first == 9002,
                "player_id 全序落账");
    TEST_ASSERT(h.sink.rewards[0].second >= 0 && h.sink.rewards[1].second >= 0,
                "分数非负（命中伤害合计骨架口径）");
    TEST_ASSERT(h.node->snapshot_seq() == 1, "一拍一投影");
    return true;
}

// ---- 4. 投影水位 ----

bool test_snapshot_watermark() {
    Harness h;
    const std::uint64_t id = 300;
    TEST_ASSERT(spawn_to_running(h, id), "就绪全链");
    TEST_ASSERT(h.host->send_battle_input(input_msg(id, 9001, 0, {1})), "输入");
    TEST_ASSERT(h.node->tick(0), "首拍");
    TEST_ASSERT(h.host->last_snapshot_seq(id) == 1, "水位 1");
    // 重复/倒退水位整包拒（attribute-sync seq 纪律同源）
    BattleStateSync stale;
    stale.instance_id = id;
    stale.tick = 0;
    stale.snapshot_seq = 1; // 重复
    TEST_ASSERT(!h.host->on_envelope({MsgId::battle_state_sync,
                                      apollo::game::battle::offload::encode(stale)}),
                "重复水位拒");
    stale.snapshot_seq = 0; // 倒退
    TEST_ASSERT(!h.host->on_envelope({MsgId::battle_state_sync,
                                      apollo::game::battle::offload::encode(stale)}),
                "倒退水位拒");
    stale.snapshot_seq = 2; // 前进
    TEST_ASSERT(h.host->on_envelope({MsgId::battle_state_sync,
                                     apollo::game::battle::offload::encode(stale)}),
                "前进水位收");
    TEST_ASSERT(h.host->last_snapshot_seq(id) == 2, "水位 2");
    return true;
}

// ---- 5. 非法跃迁 ----

bool test_illegal_transitions() {
    Harness h;
    const std::uint64_t id = 400;
    // 重复 spawn 拒（id 冲突）
    TEST_ASSERT(h.host->request_spawn(spawn_req(id)), "首次 spawn");
    TEST_ASSERT(!h.host->request_spawn(spawn_req(id)), "重复 spawn 拒");
    TEST_ASSERT(!h.host->request_spawn(spawn_req(0)), "零 id 拒");
    // 未受理先 ready 拒
    TEST_ASSERT(!h.host->notify_ready(999), "未知实例 ready 拒");
    // Accepted → Running 前落回拒
    BattleResult early;
    early.instance_id = id;
    early.player_ids = {9001};
    early.scores = {1};
    TEST_ASSERT(!h.host->on_envelope(
                    {MsgId::battle_result, apollo::game::battle::offload::encode(early)}),
                "Running 前落回拒");
    // 平行数组不等长拒（923 字段纪律）
    TEST_ASSERT(h.host->notify_ready(id), "就绪");
    BattleResult ragged;
    ragged.instance_id = id;
    ragged.player_ids = {9001, 9002};
    ragged.scores = {1};
    TEST_ASSERT(!h.host->on_envelope(
                    {MsgId::battle_result, apollo::game::battle::offload::encode(ragged)}),
                "平行数组不等长拒");
    // 未知实例的投影/落回拒
    BattleStateSync stray;
    stray.instance_id = 555;
    stray.snapshot_seq = 1;
    TEST_ASSERT(!h.host->on_envelope(
                    {MsgId::battle_state_sync, apollo::game::battle::offload::encode(stray)}),
                "未知实例投影拒");
    // 节点侧：未 spawn 先 tick 拒、族外信封拒
    MemoryOffloadTransport dead_uplink;
    InstanceOffloadNode orphan(dead_uplink);
    TEST_ASSERT(!orphan.tick(0), "未 spawn tick 拒");
    TEST_ASSERT(!orphan.on_envelope({MsgId::heartbeat, {}}), "族外信封拒");
    return true;
}

// ---- 6. 崩溃即作废 ----

bool test_crash_voids() {
    Harness h;
    const std::uint64_t id = 500;
    TEST_ASSERT(spawn_to_running(h, id), "就绪全链");
    TEST_ASSERT(h.host->send_battle_input(input_msg(id, 9001, 0, {1})), "输入");
    TEST_ASSERT(h.node->tick(0), "首拍");
    // 实例崩溃（abandon：不出落回）
    h.node->abandon();
    TEST_ASSERT(h.node->abandoned(), "节点作废");
    // 宿主经死亡面感知（生产 = machined 死亡事件钩子）
    TEST_ASSERT(h.host->notify_crash(id), "死亡面感知");
    TEST_ASSERT(h.host->phase(id) == InstanceOffloadHost::InstancePhase::Voided,
                "崩溃即作废");
    // 作废后：落回不再受理、输入拒、终态不可再改
    BattleResult late;
    late.instance_id = id;
    late.player_ids = {9001};
    late.scores = {1};
    TEST_ASSERT(!h.host->on_envelope(
                    {MsgId::battle_result, apollo::game::battle::offload::encode(late)}),
                "作废后落回拒");
    TEST_ASSERT(!h.host->send_battle_input(input_msg(id, 9001, 1, {1})),
                "作废后输入拒");
    TEST_ASSERT(!h.host->notify_crash(id), "终态不可再作废");
    TEST_ASSERT(!h.host->notify_ready(id), "终态不可再就绪");
    // 节点侧：作废后 settle 拒、信封拒
    TEST_ASSERT(!h.node->settle(), "作废后结算拒");
    TEST_ASSERT(!h.node->on_envelope(
                    {MsgId::battle_input, apollo::game::battle::offload::encode(
                                              input_msg(id, 9001, 1, {1}))}),
                "作废后信封拒");
    TEST_ASSERT(h.sink.rewards.empty(), "无落回出账");
    return true;
}

// ---- 7. 受理拒绝路径 ----

bool test_spawn_rejected() {
    // 拒绝面（骨架期恒受理之外的路径：满载/模板未注册——machined 控制面批
    // 落地面）：受理点回 accepted=0（ack-on-accept 语义的反例半边）
    MemoryOffloadTransport downlink;
    MemoryOffloadTransport uplink;
    CollectingSink sink;
    InstanceOffloadHost host(downlink, &sink);
    downlink.attach([&uplink](const OffloadEnvelope& e) {
        if (e.msg == MsgId::instance_spawn_request) {
            InstanceSpawnRequest req;
            apollo::game::battle::offload::decode(e.payload, req);
            InstanceSpawnReply reply;
            reply.instance_id = req.instance_id;
            reply.accepted = false;
            reply.error_message = "template not registered";
            uplink.send({MsgId::instance_spawn_reply,
                         apollo::game::battle::offload::encode(reply)});
        }
    });
    uplink.attach([&host](const OffloadEnvelope& e) { host.on_envelope(e); });

    const std::uint64_t id = 600;
    TEST_ASSERT(host.request_spawn(spawn_req(id)), "request 发出");
    TEST_ASSERT(host.phase(id) == InstanceOffloadHost::InstancePhase::Rejected,
                "拒绝回执 → Rejected 终态");
    // 终态冻结：ready/crash/输入全拒
    TEST_ASSERT(!host.notify_ready(id), "Rejected 后 ready 拒");
    TEST_ASSERT(!host.notify_crash(id), "Rejected 后 crash 拒");
    TEST_ASSERT(!host.send_battle_input(input_msg(id, 9001, 0, {1})),
                "Rejected 后输入拒");
    TEST_ASSERT(sink.rewards.empty(), "无落回出账");
    return true;
}

// ---- 8. 跨边界确定性 ----

bool test_cross_boundary_determinism() {
    const std::uint64_t seed = 2026;
    const std::uint64_t battle_id = 700;
    // 进程内直跑：同 seed 同输入同拓扑（只入 9001）
    CollectingSink direct_sink;
    BattleRuntime direct(battle_id, seed, &direct_sink);
    direct.enter_player(9001);
    direct.begin();
    for (std::uint32_t t = 0; t < 3; ++t) {
        std::vector<apollo::game::battle::BattleInput> batch;
        apollo::game::battle::BattleInput a;
        a.tick = t;
        a.player_id = 9001;
        a.opcode = 1;
        batch.push_back(a);
        direct.tick(t, batch);
    }
    direct.finish();

    // offload 链路：同 seed 同输入（wire 过界）；旁路截获落回信封验 hash 链尾
    MemoryOffloadTransport downlink;
    MemoryOffloadTransport uplink;
    CollectingSink sink;
    InstanceOffloadHost host(downlink, &sink);
    InstanceOffloadNode node(uplink, seed);
    std::optional<BattleResult> wire_result;
    bool uplink_ok = true; // 上行全受理标记（lambda 内不跳出宿主断言）
    downlink.attach([&node](const OffloadEnvelope& e) { node.on_envelope(e); });
    uplink.attach([&](const OffloadEnvelope& e) {
        if (e.msg == MsgId::battle_result) {
            BattleResult r;
            if (!apollo::game::battle::offload::decode(e.payload, r)) {
                uplink_ok = false;
                return;
            }
            wire_result = r;
        }
        if (!host.on_envelope(e)) {
            uplink_ok = false;
        }
    });

    const std::uint64_t id = battle_id;
    InstanceSpawnRequest req = spawn_req(id);
    TEST_ASSERT(host.request_spawn(req), "spawn");
    TEST_ASSERT(host.notify_ready(id), "就绪");
    for (std::int64_t t = 0; t < 3; ++t) {
        TEST_ASSERT(host.send_battle_input(input_msg(id, 9001, t, {1})), "输入");
        TEST_ASSERT(node.tick(static_cast<std::uint64_t>(t)), "拍推进");
    }
    TEST_ASSERT(node.settle(), "结算");
    TEST_ASSERT(uplink_ok, "上行信封全受理");
    TEST_ASSERT(host.phase(id) == InstanceOffloadHost::InstancePhase::Settled,
                "宿主终态");
    // 同四元组（同 seed 同输入同拓扑）→ 同事件流同 hash 链（battle-determinism
    // #16「跨机实例同 binary 即同确定域」的内存桩预演）
    TEST_ASSERT(wire_result.has_value(), "落回信封已截获");
    TEST_ASSERT(wire_result->replay_hash == static_cast<std::int64_t>(direct.hash_chain()),
                "hash 链尾与直跑一致");
    TEST_ASSERT(sink.rewards == direct_sink.rewards, "落回与直跑逐对相同");
    return true;
}

// ---- 9. 自灭与单实例 ----

bool test_teardown_self_extinguish() {
    Harness h;
    const std::uint64_t id = 800;
    TEST_ASSERT(spawn_to_running(h, id), "就绪全链");
    TEST_ASSERT(h.host->send_battle_input(input_msg(id, 9001, 0, {1})), "输入");
    TEST_ASSERT(h.node->tick(0), "首拍");
    TEST_ASSERT(h.node->settle(), "结算");
    // 自灭后：拍/结算/信封全拒（单 scene 单开进程：二拉拒）
    TEST_ASSERT(!h.node->tick(1), "自灭后 tick 拒");
    TEST_ASSERT(!h.node->settle(), "自灭后 settle 拒");
    TEST_ASSERT(!h.node->on_envelope({MsgId::instance_spawn_request,
                                      apollo::game::battle::offload::encode(
                                          spawn_req(801))}),
                "自灭后二拉拒");
    TEST_ASSERT(!h.node->on_envelope(
                    {MsgId::battle_input,
                     apollo::game::battle::offload::encode(input_msg(id, 9001, 1, {1}))}),
                "自灭后输入拒");
    // 宿主侧终态冻结：Settled 后 ready/crash/落回全拒
    TEST_ASSERT(!h.host->notify_ready(id), "Settled 后 ready 拒");
    TEST_ASSERT(!h.host->notify_crash(id), "Settled 后 crash 拒");
    BattleResult again;
    again.instance_id = id;
    TEST_ASSERT(!h.host->on_envelope(
                    {MsgId::battle_result, apollo::game::battle::offload::encode(again)}),
                "二次落回拒");
    return true;
}

struct Case {
    const char* name;
    bool (*fn)();
};

} // namespace

int main() {
    const Case cases[] = {
        {"wire_roundtrip", test_wire_roundtrip},
        {"wire_strict", test_wire_strict},
        {"lifecycle_full_chain", test_lifecycle_full_chain},
        {"result_fallback", test_result_fallback},
        {"snapshot_watermark", test_snapshot_watermark},
        {"illegal_transitions", test_illegal_transitions},
        {"crash_voids", test_crash_voids},
        {"spawn_rejected", test_spawn_rejected},
        {"cross_boundary_determinism", test_cross_boundary_determinism},
        {"teardown_self_extinguish", test_teardown_self_extinguish},
    };
    int failed = 0;
    for (const auto& c : cases) {
        if (!c.fn()) {
            std::cerr << "CASE FAILED: " << c.name << std::endl;
            ++failed;
        } else {
            std::cout << "PASS: " << c.name << std::endl;
        }
    }
    if (failed != 0) {
        std::cerr << failed << "/" << (sizeof(cases) / sizeof(cases[0]))
                  << " cases failed" << std::endl;
        return 1;
    }
    std::cout << "all " << (sizeof(cases) / sizeof(cases[0])) << " cases passed"
              << std::endl;
    return 0;
}
