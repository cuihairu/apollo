#pragma once

// World 侧执行位置投影发布客户端（L115 位置投影首切片，契约 968）：World
// 管理泵在归属变更提交点（journal A/R 行已落盘）调 publish()，客户端经
// login 服务注册同址拨入的链路逐条发 968（fire-and-forget，水位幂等即裁决
// 载体，无回执面）。
//
// 重放纪律（D05 §1.2 目录重启重建）：kUp 跃迁（首连与断链重连）即重放全量
// 镜像——镜像由 publish 逐条维护（claim 入表 / 墓碑抹除），等于 World 当前
// 已提交视图；目录侧重启/断链缺口的收敛由同版本幂等保证（重放不重复记账）。
// 镜像先于链路：start 前的 publish 只记账镜像不发送，kUp 重放补投——World
// 重启窗（start 先于链路拨通）由首连重放覆盖。
//
// 线程纪律：与 WorldApp 管理泵同线程——publish（分配/释放路径，管理泵上下
// 文）与 pump（main 经 set_manager_pre_tick 挂泵拍）同线程串行，类不加锁；
// state() 同线程读（测试/宿主同拍）。镜像 seeding：WorldApp 启动首拍经
// publish 重放 journal 装载的已提交归属（同线程）。
//
// POSIX 真实现 / Windows null 桩由工厂收口（start false = 面关），消费面
// 不被 #if 污染（null 桩 publish 记账镜像照常，发不出 = face off）。

#include <cstdint>
#include <memory>
#include <string>

namespace world_app {

struct M1PositionProjectionConfig {
    // login 服务注册受理地址（与 964 拨号同址；host 空 / port 0 = 面关）。
    std::string login_host;
    std::uint16_t login_port = 0;
    // 投影源 World 号（968 world_id；0 = 装配显形拒绝——身份缺位不发布）。
    std::int32_t world_id = 0;
    // 契约路由表 JSON（空路径 = 装配失败显形，不带隐式回退）。
    std::string route_json;
    // 链路自报名（空 = "world-proj-<world_id>" 自铸；与 964 注册链路分链，
    // 各自独立拨号重连，互不牵连）。
    std::string self_name;
};

// 发布账快照（管理泵上下文写；测试/宿主同线程读）。
struct M1PositionProjectionState {
    std::uint64_t sent = 0;     // 968 发送次数（publish 直发 + kUp 重放累计）
    std::uint64_t replays = 0;  // kUp 跃迁全量重放次数
    std::uint64_t mirrored = 0; // 镜像条目数（当前已提交视图）
};

class M1PositionProjectionClient {
public:
    virtual ~M1PositionProjectionClient() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;

    // 宿主泵（管理泵线程）：链路推进 + kUp 跃迁全量镜像重放。
    virtual void pump(std::uint64_t now_ms) = 0;

    virtual bool linkUp() const = 0;

    // 发布已提交归属：claim = cell_id≠0 且 ready=true；释放墓碑 = cell_id=0
    // 且 ready=false（镜像抹除 + 目录抹除面）。management_epoch 与 962 同源
    //（World 管理代次，逐帧透传）。未启动时镜像照记（kUp 重放补投）。
    virtual void publish(std::uint64_t space_id, std::uint64_t cell_id,
                         std::uint64_t assignment_version,
                         std::uint64_t management_epoch, bool ready) = 0;

    virtual M1PositionProjectionState state() const = 0;
};

// 工厂：POSIX 真实现 / Windows null 桩（start false = 面关）。
std::unique_ptr<M1PositionProjectionClient> makeM1PositionProjectionClient(
    M1PositionProjectionConfig config);

} // namespace world_app
