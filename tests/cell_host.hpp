#pragma once

// 切片3 测试宿主：独立执行端（CellApp）+ 注册客户端 + M1 传输泵。
//
// WorldApp 只承载管理面；集成测试需要模拟宿主时以本接线复现生产形态：
// cell 主动拨 World 注册，World 经注册连接反向转发意图/管理命令；下行与
// 延时应答经同一注册连接回程，World 再转 gateway。
#include "world_app/cell_app.hpp"
#include "world_app/cell_register_client.hpp"
#include "world_app/m1_link_server.hpp"
#include "apollo/script/module_rollout.hpp"

#include <chrono>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>

namespace cell_test {

struct CellLinkEgress final : public world_app::IM1Egress {
    world_app::M1LinkServer* link = nullptr;
    world_app::CellRegisterClient* register_client = nullptr;
    world_app::M1LinkServer* world_link = nullptr;

    void sendClientDeliver(std::uint64_t session_id, std::uint32_t gateway_id,
                           const std::vector<std::uint8_t>& payload) override {
        if (world_link) {
            // 下行经 World M1 端点转发 gateway（生产 cell 宿主经注册连接回
            // World，World 再 gateway_link() 转发；测试 cell 无 gateway 直连，
            // 用 World 端点代转）。
            world_link->sendClientDeliver(session_id, gateway_id, payload);
        } else if (link) {
            link->sendClientDeliver(session_id, gateway_id, payload);
        }
    }
    void sendIntentAck(std::uint64_t request_id, std::int32_t code,
                       const std::string& message) override {
        if (register_client) {
            register_client->sendIntentAck(request_id, code, message);
        }
    }
};

struct CellHost {
    world_app::WorldAppConfig config;
    std::unique_ptr<world_app::CellApp> cell;
    CellLinkEgress egress;
    std::unique_ptr<world_app::M1LinkServer> link;
    std::unique_ptr<world_app::CellRegisterClient> register_client;
    world_app::M1LinkServer* world_link = nullptr;
    std::unique_ptr<apollo::script::ModuleRollout> module_rollout;
    std::string last_rollout_name;
    // 944 debug eval 在途账（scene_id → eval_id 集；post_tick 收账，原
    // WorldApp 内嵌形态同型）。
    std::map<std::uint64_t, std::set<std::uint64_t>> eval_pending;
    std::int32_t drain_state = 0;
    std::uint64_t drain_deadline_ms = 0;
    std::string drain_detail;

    bool start(std::uint16_t world_m1_port, std::uint64_t cell_id = 21,
               std::uint16_t port = 0,
               world_app::M1LinkServer* world_m1_link = nullptr) {
        world_link = world_m1_link;
        if (config.host.empty()) {
            config.host = "127.0.0.1";
        }
        // cell 自身监听口由 port 参数决定（0 = 内核分配）；config 缺省值
        //（9101）不隐式生效——测试与生产拓扑的端口面显式覆盖。
        config.m1Port = port;
        config.m1RouteJson = config.m1RouteJson.empty()
                                 ? "sdks/cpp/generated/contract_route.json"
                                 : config.m1RouteJson;
        if (config.registerHost.empty()) {
            config.registerHost = "127.0.0.1";
        }
        config.registerPort = world_m1_port;
        config.cellId = cell_id;
        if (config.tickRateMs == 0) {
            config.tickRateMs = 20;
        }
        cell = std::make_unique<world_app::CellApp>(config);
        egress.register_client = nullptr;
        egress.link = nullptr;
        egress.world_link = world_m1_link;
        cell->set_egress(&egress);
        cell->set_pre_tick([this](std::uint64_t now_ms) {
            if (link) {
                link->pump(now_ms);
            }
            if (register_client) {
                auto* squads = cell->squadInstanceService();
                register_client->setReportedDrainState(
                    squads != nullptr && squads->draining() ? 1 : 0);
                const auto cap = cell->intentQueueCap();
                register_client->setReportedLoad(
                    cap == 0 ? 0 : static_cast<std::int32_t>(std::min(
                                          std::size_t{100},
                                          cell->pendingIntentCount() * 100 / cap)));
                register_client->setReportedReady(!cell->frozen());
                register_client->pump(now_ms);
            }
        });
        cell->set_post_tick([this](double) {
            // 944 eval 收账（Scene tick 边界已执行在途 eval——回 945 经注册
            // 连接 → World → fleetctl peer，原 WorldApp drainDebugEvalResults
            // 同型）。
            for (auto it = eval_pending.begin(); it != eval_pending.end();) {
                auto* scene = cell->find_scene(it->first);
                if (scene == nullptr) {
                    it = eval_pending.erase(it);
                    continue;
                }
                for (const auto& r : scene->drain_debug_eval_results()) {
                    const auto peer = it->second.find(r.id);
                    if (peer == it->second.end()) {
                        continue;
                    }
                    it->second.erase(peer);
                    world_app::CellDebugEvalResult reply;
                    reply.eval_id = r.id;
                    reply.ok = r.ok;
                    reply.result = r.ok ? r.result : "";
                    reply.error = r.ok ? "" : r.result;
                    register_client->sendDebugEvalResult(reply);
                }
                if (it->second.empty()) {
                    it = eval_pending.erase(it);
                } else {
                    ++it;
                }
            }
            // module rollout 观察窗推进（test 宿主最小面；生产 cell 宿主经
            // register_client 反向 953，观察窗推进随拍自驱）。
            if (module_rollout) {
                auto* script = cell->script_runtime();
                if (script) {
                    for (const auto& name : {last_rollout_name}) {
                        if (!name.empty()) {
                            (void)script->call_module(name, "onTick");
                        }
                    }
                    const auto st = module_rollout->state();
                    if (st == apollo::script::RolloutState::kObserving) {
                        module_rollout->observe_target("cell");
                    } else if (st == apollo::script::RolloutState::kPromoting) {
                        for (const auto& k : module_rollout->pending_promote()) {
                            module_rollout->commit_target(k);
                        }
                    } else if (st == apollo::script::RolloutState::kRollingBack) {
                        for (const auto& k : module_rollout->pending_rollback()) {
                            module_rollout->rollback_target(k);
                        }
                    }
                }
            }
            auto* squads = cell->squadInstanceService();
            if (squads == nullptr || drain_state != 1) {
                return;
            }
            if (squads->active_count() == 0) {
                drain_state = 2;
                drain_detail = "drained: all instances settled";
                return;
            }
            if (drain_deadline_ms == 0
                || world_app::steady_now_ms() < drain_deadline_ms) {
                return;
            }
            std::size_t force_stopped = 0;
            for (const auto& binding : squads->activeBindings()) {
                if (squads->abort(binding.party_id)
                    || squads->dismiss(binding.party_id)) {
                    ++force_stopped;
                }
            }
            if (squads->active_count() == 0) {
                drain_state = 2;
                drain_detail = "drained: deadline reached, "
                               + std::to_string(force_stopped)
                               + " instances force-aborted";
            }
        });
        link = world_app::makeM1LinkServer(
            world_app::M1LinkConfig{config.host, config.m1Port,
                                    config.m1RouteJson,
                                    config.m1AuthorityEpoch},
            [this](world_app::IntentEnvelope intent) {
                cell->onIntent(std::move(intent));
            });
        egress.link = link.get();
        // 下行经 World M1 端点转发（见 CellLinkEgress::sendClientDeliver）。
        world_link = nullptr;
        cell->start();
        if (!link->start()) {
            return false;
        }

        world_app::CellRegisterConfig rc;
        rc.world_host = config.registerHost;
        rc.world_port = config.registerPort;
        rc.cell_id = config.cellId;
        rc.advertise_host = config.host;
        rc.advertise_port = link->listenPort();
        rc.route_json = config.m1RouteJson;
        rc.heartbeat_ms = 500;
        register_client = world_app::makeCellRegisterClient(std::move(rc));
        if (!register_client) {
            return false;
        }
        register_client->set_intent_listener(
            [this](const std::vector<std::uint8_t>& payload) {
                world_app::IntentEnvelope intent;
                if (world_app::decodeEnvelopeIntent(payload, intent)) {
                    cell->onIntent(std::move(intent));
                }
            });
        register_client->set_world_drain_listener(
            [this](const world_app::CellWorldDrainRequest& request) {
                if (request.request_id == 0) {
                    return;
                }
                auto* squads = cell->squadInstanceService();
                world_app::CellWorldDrainReply reply;
                reply.request_id = request.request_id;
                reply.op = request.op;
                if (squads == nullptr) {
                    reply.ok = false;
                    reply.detail = "squad service not ready";
                } else if (request.op == 1) {
                    squads->set_drain(true);
                    if (drain_state == 0) {
                        drain_state = 1;
                        drain_deadline_ms =
                            request.deadline_s > 0
                                ? world_app::steady_now_ms()
                                      + static_cast<std::uint64_t>(request.deadline_s)
                                            * 1000U
                                : 0;
                    } else if (request.deadline_s > 0) {
                        const auto deadline =
                            world_app::steady_now_ms()
                            + static_cast<std::uint64_t>(request.deadline_s) * 1000U;
                        if (drain_deadline_ms == 0 || deadline < drain_deadline_ms) {
                            drain_deadline_ms = deadline;
                        }
                    }
                    reply.ok = true;
                    reply.state = drain_state;
                    reply.active_instances = squads->active_count();
                    reply.detail = "draining: "
                                   + std::to_string(squads->active_count())
                                   + " active instances settling";
                } else if (request.op == 2) {
                    reply.ok = true;
                    reply.state = drain_state;
                    reply.active_instances = squads->active_count();
                    reply.detail = drain_state == 2
                                       ? drain_detail
                                       : drain_state == 1
                                             ? "draining: "
                                               + std::to_string(squads->active_count())
                                               + " active instances settling"
                                             : std::string("serving");
                } else {
                    reply.ok = false;
                    reply.detail = "unknown op " + std::to_string(request.op);
                }
                register_client->sendWorldDrainReply(reply);
            });
        register_client->set_module_rollout_listener(
            [this](const world_app::CellModuleRolloutRequest& req) {
                if (req.request_id == 0) {
                    return;
                }
                auto* script = cell->script_runtime();
                auto reply = [&](bool ok, std::int32_t state, std::uint64_t faults,
                                 std::uint64_t version, const std::string& detail) {
                    world_app::CellModuleRolloutReply rep;
                    rep.request_id = req.request_id;
                    rep.op = req.op;
                    rep.ok = ok;
                    rep.state = state;
                    rep.window_faults = faults;
                    rep.module_version = version;
                    rep.detail = detail;
                    register_client->sendModuleRolloutReply(rep);
                };
                if (script == nullptr) {
                    reply(false, -1, 0, 0, "script runtime not ready");
                    return;
                }
                const auto state_of = [&]() -> std::int32_t {
                    return module_rollout
                               ? static_cast<std::int32_t>(module_rollout->state())
                               : 0;
                };
                const auto detail_of = [&]() -> std::string {
                    if (!module_rollout) {
                        return std::string("no rollout in flight");
                    }
                    const auto errs = module_rollout->errors();
                    return errs.empty() ? std::string("no errors")
                                        : errs[0];
                };
                switch (req.op) {
                    case 1: {
                        if (module_rollout == nullptr) {
                            module_rollout = std::make_unique<apollo::script::ModuleRollout>(
                                std::vector<apollo::script::RolloutTarget>{
                                    {"cell", script}});
                        }
                        apollo::script::RolloutModule mod;
                        mod.name = req.name;
                        mod.version = req.version;
                        mod.deps = [&] {
                            std::vector<std::string> out;
                            std::string tok;
                            for (const char c : req.deps) {
                                if (c == ',') {
                                    if (!tok.empty()) out.push_back(tok);
                                    tok.clear();
                                } else if (c != ' ' && c != '\t' && c != '\n'
                                           && c != '\r') {
                                    tok.push_back(c);
                                }
                            }
                            if (!tok.empty()) out.push_back(tok);
                            return out;
                        }();
                        mod.source = req.source;
                        const bool ok = module_rollout->prepare(
                            {mod}, {"cell"},
                            apollo::script::CanaryParams{req.observe_ticks,
                                                         req.fault_threshold});
                        // 驱动名在 commit 成功后才登记（onTick 只对已激活
                        // 版本——prepare 后提前 call_module 会因模块未提交
                        // 产生 fault 污染 canary 窗，判坏误回滚）。
                        reply(ok, state_of(), 0, script->module_version(req.name),
                              detail_of());
                        return;
                    }
                    case 2: {
                        if (module_rollout == nullptr) {
                            reply(false, 0, 0, script->module_version(req.name),
                                  "no rollout in flight");
                            return;
                        }
                        if (module_rollout->state() ==
                            apollo::script::RolloutState::kCanaryPending) {
                            module_rollout->commit_target("cell");
                        }
                        const bool ok = module_rollout->state() ==
                                        apollo::script::RolloutState::kObserving;
                        if (ok) {
                            last_rollout_name = req.name;
                        }
                        reply(ok, state_of(), 0,
                              script->module_version(req.name), detail_of());
                        return;
                    }
                    case 3: {
                        if (module_rollout == nullptr) {
                            reply(true, 0, 0, script->module_version(req.name),
                                  "no rollout in flight");
                            return;
                        }
                        if (module_rollout->state() ==
                            apollo::script::RolloutState::kCanaryPending) {
                            module_rollout.reset();
                            reply(true, 0, 0, script->module_version(req.name),
                                  "aborted: nothing committed");
                            return;
                        }
                        module_rollout->rollback_target("cell");
                        const bool rb_ok = module_rollout->state() ==
                                           apollo::script::RolloutState::kRolledBack;
                        reply(rb_ok, state_of(), 0,
                              script->module_version(req.name), detail_of());
                        return;
                    }
                    default: {
                        std::uint64_t faults = 0;
                        const auto st = module_rollout
                                            ? module_rollout->statuses()
                                            : std::vector<apollo::script::TargetStatus>{};
                        if (!st.empty()) {
                            faults = st[0].window_faults;
                        }
                        reply(true, state_of(), faults,
                              script->module_version(req.name), detail_of());
                        return;
                    }
                }
            });
        egress.register_client = register_client.get();
        register_client->set_debug_eval_listener(
            [this](const world_app::CellDebugEvalRequest& request) {
                if (request.eval_id == 0) {
                    return;
                }
                const auto scene_id = request.scene_id != 0
                                          ? request.scene_id
                                          : cell->default_scene_id();
                auto* scene = cell->find_scene(scene_id);
                if (scene == nullptr || !scene->script_attached()
                    || !scene->enqueue_debug_eval(request.eval_id,
                                                  request.source)) {
                    world_app::CellDebugEvalResult reply;
                    reply.eval_id = request.eval_id;
                    reply.ok = false;
                    reply.error = scene == nullptr || !scene->script_attached()
                                      ? "scene not attached"
                                      : "eval queue full";
                    register_client->sendDebugEvalResult(reply);
                    return;
                }
                eval_pending[scene_id].insert(request.eval_id);
            });
        (void)world_m1_port;
        const bool ok = register_client->start();
        if (!ok) {
        }
        return ok;
    }

    bool registered() const {
        return register_client && register_client->state().accepted;
    }

    world_app::CellRegisterClient& client() { return *register_client; }

    void set_world_link(world_app::M1LinkServer* wl) {
        egress.world_link = wl;
    }

    void pump_until_registered(int timeout_ms = 5000) {
        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(timeout_ms);
        while (!registered() && std::chrono::steady_clock::now() < deadline) {
            // 注册泵由 cell 内核线程的 pre-tick 钩子驱动（单线程纪律）；
            // 测试线程只轮询观测面，不手动泵注册客户端（双泵即竞态）。
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    squad_instance::SquadInstanceService* squads() {
        return cell ? cell->squadInstanceService() : nullptr;
    }

    std::uint64_t resolveIntentPlayer(std::uint64_t session_id) const {
        return cell ? cell->resolveIntentPlayer(session_id) : 0;
    }

    void stop() {
        // 停机顺序与装配相反：先停内核（关闸/尾扫/join，pre-tick 钩子仍
        // 驱动注册泵），再停注册客户端与 M1 端点——保证 pre-tick 钩子访问
        // 的注册客户端对象在内核 join 后仍存活。
        if (cell) {
            cell->stop();
            cell.reset();
        }
        if (register_client) {
            register_client->stop();
            register_client.reset();
        }
        if (link) {
            link->stop();
            link.reset();
        }
    }
};

} // namespace cell_test
