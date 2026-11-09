#pragma once

// m1_link_server 拆分单元内面（LAY-008）：M1LinkServerImpl 全类声明 + 意图翻译
// 共享面（m1_link_detail）。原单文件实现按职责分 TU——生命周期/装配在
// m1_link_server.cpp，回执与命令发送面（_send）、裸帧受理面（_recv）、932 意图
// 翻译（_wire）各占一 TU。src 本地头，不进安装面。

#include "world_app/m1_link_server.hpp"
#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/linktrace/envelope.hpp"
#include "apollo/net/linktrace/span_emitter.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace world_app {

// world_app 域共享别名（勿上提全局——::link 是 POSIX unistd 函数名；
// acceptance_queue.hpp 已带 trace 同名同目标别名，重复声明同目标别名合法）
namespace link = apollo::net::link;
namespace trace = apollo::net::trace;
namespace wire = apollo::net::wire;

namespace m1_link_detail {

// 契约 MsgId → u16 id（原匿名域别名收拢，拆分 TU 共享）
using apollo::contract::gen::MsgId;
constexpr std::uint16_t id_of(MsgId id) {
    return static_cast<std::uint16_t>(id);
}

// 契约 msg id → legacy 类型 + 帧字节（定义在 m1_link_server_wire.cpp）
bool build_legacy_intent(std::uint16_t msg_id, const std::vector<wire::FieldValue>& f,
                         IntentEnvelope& out);

} // namespace m1_link_detail

using m1_link_detail::MsgId;
using m1_link_detail::build_legacy_intent;
using m1_link_detail::id_of;

class M1LinkServerImpl final : public M1LinkServer {
public:
    M1LinkServerImpl(M1LinkConfig config, std::function<void(IntentEnvelope)> on_intent,
                     std::function<void(const std::vector<std::uint8_t>&)> on_intent_raw,
                     std::function<void(const std::string&, const DebugEvalRequest&)> on_debug_eval,
                     std::function<void(const std::string&, const ModuleRolloutRequest&)>
                         on_module_rollout,
                     std::function<void(const std::string&, const WorldDrainRequest&)>
                         on_world_drain,
                     std::function<void(const std::string&, const CellRegisterRequest&)>
                         on_cell_register,
                     std::function<void(const std::string&, const CellRetireRequest&)>
                         on_cell_retire,
                     std::function<void(const std::string&, const CellSpaceReport&)>
                         on_cell_space_report,
                     std::function<void(const std::string&, const CellEvalResult&)>
                         on_cell_eval_result,
                     std::function<void(const std::string&, const CellRolloutReply&)>
                         on_cell_rollout_reply,
                     std::function<void(const std::string&, const CellDrainReply&)>
                         on_cell_drain_reply,
                     std::function<void(const std::string&, const CellIntentAck&)>
                         on_cell_intent_ack);

    bool start() override;
    void stop() override;
    void pump(std::uint64_t now_ms) override;
    void on_peer_down(std::function<void(const std::string& peer)> fn) override;
    std::vector<link::LinkSnapshot> snapshot_links() const override;

    void sendIntentAck(std::uint64_t request_id, std::int32_t code,
                       const std::string& message) override;
    void sendEvalResult(const std::string& peer, std::uint64_t eval_id, bool ok,
                        const std::string& result, const std::string& error) override;
    void sendModuleRolloutReply(const std::string& peer, std::uint64_t request_id,
                                std::int32_t op, bool ok, std::int32_t state,
                                std::uint64_t window_faults,
                                std::uint64_t module_version,
                                const std::string& detail) override;
    void sendClientDeliver(std::uint64_t session_id, std::uint32_t gateway_id,
                           const std::vector<std::uint8_t>& payload) override;
    void sendWorldDrainReply(const std::string& peer, std::uint64_t request_id,
                             std::int32_t op, bool ok, std::int32_t state,
                             std::uint64_t active_instances,
                             const std::string& detail) override;
    void sendCellRegisterReply(const std::string& peer, std::uint64_t request_id,
                               bool accepted, std::int32_t code,
                               std::uint64_t management_epoch,
                               std::uint64_t assigned_cell_id,
                               const std::string& detail) override;
    void sendCellRetireReply(const std::string& peer, std::uint64_t request_id,
                             bool accepted, std::int32_t code,
                             std::int64_t management_epoch, std::uint64_t cell_id,
                             const std::string& detail) override;
    void sendCellSpaceAssign(const std::string& peer, std::uint64_t assignment_version,
                             std::uint64_t management_epoch, std::uint64_t space_id,
                             std::uint64_t assigned_cell_id, bool ready) override;
    bool sendCellDebugEvalRequest(const std::string& peer,
                                  const DebugEvalRequest& request) override;
    bool sendCellModuleRolloutRequest(const std::string& peer,
                                      const ModuleRolloutRequest& request) override;
    bool sendCellWorldDrainRequest(const std::string& peer,
                                   const WorldDrainRequest& request) override;
    bool sendCellIntentRaw(const std::string& peer,
                           const std::vector<std::uint8_t>& payload) override;

    std::uint16_t listenPort() const override;
    bool gatewayConnected() const override;
    std::uint64_t receivedIntents() const override;
    std::uint64_t rejectedFrames() const override;

private:
    link::InterServerLink* gateway_link();
    bool load_route_table(link::RouteTable& table);
    void install_dispatching(link::InterServerLink& link);
    void handle_cell_client_deliver_frame(const std::uint8_t* data, std::size_t size);
    void handle_debug_eval_frame(const std::string& peer, const std::uint8_t* data,
                                 std::size_t size);
    void handle_module_rollout_frame(const std::string& peer, const std::uint8_t* data,
                                     std::size_t size);
    void handle_world_drain_frame(const std::string& peer, const std::uint8_t* data,
                                  std::size_t size);
    void handle_cell_register_frame(const std::string& peer, const std::uint8_t* data,
                                    std::size_t size);
    void handle_cell_retire_frame(const std::string& peer, const std::uint8_t* data,
                                  std::size_t size);
    void handle_cell_space_report_frame(const std::string& peer, const std::uint8_t* data,
                                        std::size_t size);
    void handle_cell_eval_result_frame(const std::string& peer, const std::uint8_t* data,
                                       std::size_t size);
    void handle_cell_rollout_reply_frame(const std::string& peer, const std::uint8_t* data,
                                         std::size_t size);
    void handle_cell_drain_reply_frame(const std::string& peer, const std::uint8_t* data,
                                       std::size_t size);
    void handle_cell_intent_ack_frame(const std::string& peer, const std::uint8_t* data,
                                      std::size_t size);
    void handle_envelope_frame(std::uint16_t /*msg_id*/, const std::uint8_t* data,
                               std::size_t size);

    M1LinkConfig config_;
    std::function<void(IntentEnvelope)> on_intent_;
    std::function<void(const std::vector<std::uint8_t>&)> on_intent_raw_;
    std::function<void(const std::string&, const DebugEvalRequest&)> on_debug_eval_;
    std::function<void(const std::string&, const ModuleRolloutRequest&)> on_module_rollout_;
    std::function<void(const std::string&, const WorldDrainRequest&)> on_world_drain_;
    std::function<void(const std::string&, const CellRegisterRequest&)> on_cell_register_;
    std::function<void(const std::string&, const CellRetireRequest&)> on_cell_retire_;
    std::function<void(const std::string&, const CellSpaceReport&)> on_cell_space_report_;
    std::function<void(const std::string&, const CellEvalResult&)> on_cell_eval_result_;
    std::function<void(const std::string&, const CellRolloutReply&)> on_cell_rollout_reply_;
    std::function<void(const std::string&, const CellDrainReply&)> on_cell_drain_reply_;
    std::function<void(const std::string&, const CellIntentAck&)> on_cell_intent_ack_;
    // 对端断连转报（L101 断连存疑切片）：peer 自报名 → WorldApp 存疑标记
    std::function<void(const std::string&)> on_peer_down_fn_;
    std::unique_ptr<link::RouteTable> route_table_;
    // 原子共享所有权：start 在宿主线程写、pump/pre_tick 在管理泵/内核线程
    // 读，约 20 处 send*/listen/gateway 访问与 start/stop 并发。shared_ptr
    // 的 atomic 读写让读侧持本地副本保活 manager_（stop 重置不悬垂），
    // 且不引入递归 shared_mutex（原 ddf14375 只护 pump/snapshot，未覆盖
    // 其余访问面——CR 复核发现）。
    std::atomic<std::shared_ptr<link::LinkManager>> manager_;
    std::atomic<bool> started_{false};
    std::atomic<std::uint64_t> received_{0};
    std::atomic<std::uint64_t> rejected_{0};
};

} // namespace world_app
