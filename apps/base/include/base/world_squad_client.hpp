#pragma once

// Base→World M1 小队准入链路客户端（D11 issuer=Base 切片批①，契约
// 972-976）：Base 主动拨 World 管理面（m1_link_server self_name "world"，
// cell_register_client 同型拨装），承载「受理面代申请」路径的双向往返——
// 入向 972 squad_validation_request（World 转 cell 受理请求）与 975
// squad_capacity_grant（World 预留裁决回执）；出向 973 squad_validation_
// reply（资格盖章回执）、974 squad_capacity_request（容量预留申请）、976
// squad_credential_bundle（逐成员凭证下推，967 每条目一帧惯例）。
//
// 与 m1_service_register_client 同缝纪律：宿主泵驱动，机制零内部线程——
// pump() 由宿主 tick 钩子调用；线程契约 = start/stop/pump 归宿主单线程，
// state() 任意线程（计数账面 mutex 守，不跨 manager_ pump() 持锁——入向
// handler 在 pump 内同步回调，宿主回调里可直接调 send_*（同泵上下文），
// 但不得重入 pump()）。
//
// 本批（批①）只落链路与 wire 面（未启用）：装配缺省不拨（config 缺位），
// 生产行为切换（受理面自签→代申请）随批④ flag 门控接入。裁决只在宿主：
// 本类是哑管道——入向解码即回调、出向编码即发送，不记账不重试不重裁
// （重试/超时语义随批② World 预留面定形）。
//
// POSIX 真实实现 / Windows null 桩由工厂收口（start false = 不拨），
// 消费面不被 #if 污染。

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace base {

// 入向 972（字段序与 messages.xml 全口径对齐）。
struct SquadValidationRequest {
    std::uint64_t request_id = 0;  // 受理方关联键
    std::uint64_t party_id = 0;
    std::uint64_t instance_id = 0;
    std::uint64_t cell_id = 0;
    std::string members;  // 名册快照（player_id 逗号分隔原样，拆分归宿主）
};

// 入向 975。
struct SquadCapacityGrant {
    std::uint64_t request_id = 0;
    bool granted = false;
    std::uint64_t reservation_id = 0;  // granted=1 有效
    std::uint64_t expires_at = 0;      // 预留过期（ms；0=不设窗）
    std::int32_t code = 0;             // 0=已预留；1=无容量；2=实例未知；3=排空中；4=窗已过
};

// 链路账面快照（泵上下文写，宿主/测试跨线程读——mutex 守）。计数是累计
// 面（对端缺席不回零），供观测/测试断言。
struct WorldSquadLinkState {
    std::uint64_t validation_requests_in = 0;   // 收 972
    std::uint64_t validation_replies_out = 0;   // 发 973
    std::uint64_t capacity_requests_out = 0;    // 发 974
    std::uint64_t capacity_grants_in = 0;       // 收 975
    std::uint64_t credentials_out = 0;          // 发 976（逐成员帧计）
    std::uint64_t bad_frames_in = 0;            // 解码残帧（不回调）
};

class WorldSquadClient {
public:
    virtual ~WorldSquadClient() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;

    // 宿主泵：链路推进 + 入向分派（handler 在本调用内同步回调）。
    virtual void pump(std::uint64_t now_ms) = 0;

    virtual bool linkUp() const = 0;
    virtual WorldSquadLinkState state() const = 0;

    // 入向回调（泵上下文同步调用；空 = 收帧只计数不回调）。宿主回调内可
    // 直接调对应 send_*（同泵上下文，见线程契约）。
    virtual void set_validation_handler(
        std::function<void(const SquadValidationRequest&)> fn) = 0;
    virtual void set_grant_handler(
        std::function<void(const SquadCapacityGrant&)> fn) = 0;

    // 出向 973：资格盖章回执（字段序同 973 定义）。链路未 up = 拒发
    // （返回 false，宿主决策重试——哑管道不自动重试）。
    virtual bool send_validation_reply(std::uint64_t request_id, bool accepted,
                                       const std::string& roster_hash,
                                       const std::string& failed_members,
                                       std::int32_t code) = 0;
    // 出向 974：容量预留申请。
    virtual bool send_capacity_request(std::uint64_t request_id,
                                       std::uint64_t party_id,
                                       std::uint64_t instance_id,
                                       std::uint64_t cell_id,
                                       const std::string& roster_hash,
                                       std::int32_t slots) = 0;
    // 出向 976：逐成员凭证帧。
    virtual bool send_credential_bundle(
        std::uint64_t request_id, std::uint64_t reservation_id,
        std::uint64_t credential_id, std::uint64_t player_id,
        std::uint64_t squad_id, std::uint64_t instance_id,
        std::uint64_t cell_id, const std::string& roster_hash,
        std::uint64_t issued_at, std::uint64_t expires_at,
        const std::string& issuer) = 0;
};

struct WorldSquadClientConfig {
    // World M1 管理面地址；host 空 / port 0 = 不拨（缺省未接线形态）。
    std::string world_host;
    std::uint16_t world_port = 0;
    // 本 Base 区服号（观测面用；0 = 装配显形拒绝——不拨无身份链路）。
    std::uint64_t base_id = 0;
    // 契约路由表 JSON（空路径 = 装配失败显形，不带隐式回退）。
    std::string route_json;
    // 链路自报名（peer 寻路键；空 = "base-squad-<base_id>" 自铸）。
    std::string self_name;
};

// 工厂：POSIX 真实现 / Windows null 桩（start false = 不拨）。
std::unique_ptr<WorldSquadClient> makeWorldSquadClient(
    WorldSquadClientConfig config);

} // namespace base
