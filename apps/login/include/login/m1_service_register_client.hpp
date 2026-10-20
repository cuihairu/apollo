#pragma once

// Base/World 侧服务注册拨号客户端（L123 拨号次切片，契约 964/965）：服务
// 进程主动拨 loginServer 服务注册受理面（m1_service_register_server），
// 发 964 service_register_request、收 965 记账。
//
// 与 cell_register_client（cell→World 960/961）同缝纪律：宿主泵驱动，机制
// 零内部线程——pump() 由宿主 tick 钩子调用；线程契约 = start/stop/pump 归
// 宿主单线程，state()/set_ready 任意线程（账面 mutex 守，不跨 manager_
// pump() 持锁——965 handler 在 pump 内同步回调，重入取锁即死锁）。
//
// 生命周期（注册即幂等，重发无害）：链路 kUp 跃迁（首连与断链重连）各发
// 一次 964；up 期间 heartbeat_ms 周期重发——幂等刷新 login 侧 last-heard
// （registry.registered_at_ms），兼作 965 回执丢失的重试。裁决只在受理面：
// 本类原样记录 code（1/2/3/4），不重裁、不重试风暴、不停机自裁——旧代次
// 被拒（code 3）由宿主读 state() 决策（服务被替换，本端不再权威）。
//
// POSIX 真实实现 / Windows null 桩由工厂收口（start false = 不注册），
// 消费面不被 #if 污染。

#include "login/service_registry.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace login {

struct M1ServiceRegisterClientConfig {
    // login 服务注册受理地址；host 空 / port 0 = 不注册（缺省单服形态）。
    std::string login_host;
    std::uint16_t login_port = 0;
    // 逻辑身份（964）：kind 限 kServiceKindBase/kServiceKindWorld（受理面
    // 驳未知值）；logical_id = Base 区服号 / World 世界号（0 = 装配显形拒绝）。
    std::int32_t service_kind = kServiceKindBase;
    std::uint64_t logical_id = 0;
    // 进程代次（0 = 自铸：构造时刻 steady 毫秒——CLOCK_MONOTONIC 同机单调，
    // 重启必增，旧代次不可覆盖新代次的裁决由此成立；跨机部署须显式配）。
    std::uint64_t incarnation = 0;
    // 自报管理/服务面地址（964 host/port；受理面记录不回连）。
    std::string advertise_host;
    std::uint16_t advertise_port = 0;
    // 兼容事实投影（记录不裁决——受理面同口径）。
    std::int64_t compat = 0;
    // 契约路由表 JSON（空路径 = 装配失败显形，不带隐式回退）。
    std::string route_json;
    // 周期心跳间隔毫秒：链路 up 期间每间隔重发一次 964。0 = 关（仅 kUp
    // 跃迁单发——注册即幂等，重发无害）。
    std::uint64_t heartbeat_ms = 5000;
    // 链路自报名（回执按名寻路；空 = "svc-<kind>-<logical_id>" 自铸）。
    std::string self_name;
};

// 注册账快照（泵上下文写，宿主/测试跨线程读——mutex 守）。
struct M1ServiceRegisterState {
    bool sent = false;     // 至少发出过一次 964
    bool replied = false;  // 收到过 965
    bool accepted = false; // 最近一次回执 accepted
    std::int32_t code = 0;  // 最近一次裁决码（受理面原样记录）
    std::int32_t service_kind = 0;  // 回带确认身份
    std::uint64_t logical_id = 0;
    std::string detail;    // 受理面回执明细
    std::uint64_t attempts = 0;  // 964 发送次数（kUp 跃迁 + 心跳累计）
    std::uint64_t replies = 0;   // 965 回执次数
};

class M1ServiceRegisterClient {
public:
    virtual ~M1ServiceRegisterClient() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;

    // 宿主泵：链路推进 + kUp 跃迁/心跳 964 发送 + 965 记账（handler 在本
    // 调用内同步回调）。
    virtual void pump(std::uint64_t now_ms) = 0;

    virtual bool linkUp() const = 0;
    virtual M1ServiceRegisterState state() const = 0;

    // 就绪面变化上报：置位即生效，下一次发送（kUp 跃迁或心跳拍）携带；
    // 不打断节奏——注册纪律里就绪不是新事件而是自报面字段。
    virtual void set_ready(bool ready) = 0;
};

// 工厂：POSIX 真实现 / Windows null 桩（start false = 不注册）。
std::unique_ptr<M1ServiceRegisterClient> makeM1ServiceRegisterClient(
    M1ServiceRegisterClientConfig config);

} // namespace login
