#pragma once

// cell 侧注册客户端（L101 注册次切片）：cell 进程主动拨所属 World 的 M1
// 受理面，发 960 cell_register_request、收 961 cell_register_reply 记账。
//
// 与 M1LinkServer 的分界：M1LinkServer 是被动受理端（其它进程拨入），本类
// 是主动客户端（cell→World 单方向注册）——两缝各自的 LinkManager 互不共享；
// World 端按链路 peer 自报名（cell-<cell_id>）记入注册表并按名回 961（批㉒
// 受理面）。裁决只在受理面：本类原样记录 code（1/2/3/4，acceptance_queue
// 契约词汇），不重裁、不重试风暴。
//
// 缝纪律同 M1LinkServer（宿主泵驱动，机制零内部线程）：pump() 由宿主
// gameLoop tick 驱动；线程契约 = start/stop/pump 归宿主单线程（pre-tick
// 钩子上下文），state()/setReportedDrainState 任意线程（账面 mutex 守）。
// POSIX 真实实现 / Windows 退 null 桩由工厂收口，消费面不被 #if 污染。
//
// 生命周期（注册即幂等，重发无害）：链路 kUp 跃迁（首连与断链重连）各发一
// 次 960——同代次重注册由受理面裁决为幂等刷新（code 2），新代次替换
//（code 1），旧代次拒绝（code 3）原样显形给宿主。心跳失效/重注册窗口已
// 落地（heartbeat_ms 周期重发 960 = last-heard 刷新 + 回执丢失重试；World
// 侧 cellExpiryMs 超窗摘除）。失联策略已落地（L101 失联切片）：链路 down
// 超 link_grace_ms 或收到 code 3 触发冻结监听（宿主停模拟关闸），解冻只随
// 961 复核受理——链路恢复本身不解冻。

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace world_app {

struct CellRegisterConfig {
    // World M1 受理地址；host 空 / port 0 = 不注册（缺省单机 manager 形态）。
    std::string world_host;
    std::uint16_t world_port = 0;
    // 逻辑 cell ID（注册开则必须非 0；PID/IP 不当身份，重启保留同 ID 换代次）。
    std::uint64_t cell_id = 0;
    // 自报监听地址/端口（World 回连用，本批受理面仅记录不回连）。
    std::string advertise_host;
    std::uint16_t advertise_port = 0;
    // 进程启动代次（0 = 自铸：构造时刻 steady 毫秒——CLOCK_MONOTONIC 同机
    // 单调，重启必增，旧代次不可覆盖新代次的裁决由此成立）。
    std::uint64_t start_epoch = 0;
    // 契约路由表 JSON（空路径 = 装配失败显形，不带隐式回退）。
    std::string route_json;
    // 周期心跳间隔毫秒（心跳批）：链路 up 期间每间隔重发一次 960（幂等刷新
    // World 侧 last-heard，兼作 961 回执丢失的重试）。0 = 关（仅 kUp 跃迁
    // 单发——注册即幂等，重发无害）。
    std::uint64_t heartbeat_ms = 5000;
    // 失联宽限窗毫秒（L101 失联策略切片）：控制链路 down 超窗即触发冻结
    // 监听器一次（宿主停模拟关闸——§4 宽限期冻结）；0 = 永不冻结。缺省
    // 15000 = 3 心跳拍容忍（默认心跳 5000），低于 World 过期窗 30000——
    // cell 先自冻结、World 后摘除（冻结先于除名，避免摘除后旧端仍写）。
    std::uint64_t link_grace_ms = 15000;
    // 握手代次（§5.7；0 = 进程自铸）。
    std::uint64_t authority_epoch = 0;
};

// 分配记录（962 cell_space_assign 应用面）：监听器只在版本前进时触发一次
//（去重在客户端——World 每次受理/心跳都重推当前分配，版本不变即幂等）。
struct SpaceAssignRecord {
    std::uint64_t space_id = 0;
    std::uint64_t version = 0;
    std::uint64_t assigned_cell_id = 0;
    bool ready = false;
};
using SpaceAssignListener = std::function<void(const SpaceAssignRecord&)>;

// 失联冻结原因（L101 失联策略切片）：kGraceExpired——链路 down 超宽限窗
//（link_grace_ms）；kAdjudicationRejected——961 code 3（旧代次被替换，
// World 已隔离本执行端——本端自冻结，不得继续产生权威副作用）。
enum class LinkFreezeReason {
    kGraceExpired,
    kAdjudicationRejected,
};
using LinkFreezeListener = std::function<void(LinkFreezeReason)>;
using ReadmitListener = std::function<void()>;

// 注册账快照（泵上下文写，宿主/测试跨线程读——mutex 守）。
struct CellRegisterState {
    bool sent = false;    // 至少发出过一次 960
    bool replied = false; // 收到过 961
    bool accepted = false; // 最近一次回执 accepted
    std::int32_t code = 0; // 最近一次裁决码（受理面原样记录，见 acceptance_queue）
    std::uint64_t management_epoch = 0; // World 管理代次回带（§5.7）
    std::uint64_t assigned_cell_id = 0; // World 确认的 cell ID
    std::string detail;                 // World 回执明细
    std::uint64_t attempts = 0;         // 960 发送次数（首连 + 断链重拨累计）
    std::uint64_t replies = 0;          // 961 回执次数
    bool frozen = false;                // 失联冻结已触发（解冻随 961 复核受理）
};

class CellRegisterClient {
public:
    virtual ~CellRegisterClient() = default;

    // 装配 + 首拨（异步连接；false = 配置/路由表装配失败——入口显形）。
    // 已在运行 = true（幂等）。stop 后可再 start（重拨同代次 → 幂等刷新）。
    virtual bool start() { return true; }

    // 停机（链路随管理器析构关闭）。宿主泵线程内调用。
    virtual void stop() {}

    // 宿主泵：链路一拍（连接器/链路收发/重连）；kUp 跃迁即发 960。
    virtual void pump(std::uint64_t now_ms) { (void)now_ms; }

    // 链路已握手（观测面；未装配 = false）。
    virtual bool linkUp() const { return false; }

    // 注册账快照（任意线程）。
    virtual CellRegisterState state() const { return {}; }

    // 自报排空状态（宿主注入；0=serving 1=draining 2=drained，同 956/957
    // 口径。默认 0——未上报排空态即按 serving 登记）。
    virtual void setReportedDrainState(std::int32_t drain_state) {
        (void)drain_state;
    }

    // 自报负载（宿主注入；0-100 百分制，受理面最小负载优先裁决用。默认 0
    // ——未上报即按空载登记，同 960 负载字段缺省口径）。
    virtual void setReportedLoad(std::int32_t load) { (void)load; }

    // 自报就绪（宿主注入；false = 不认领新 Space 但保留既有归属。默认 true
    // ——未接线宿主按可分配登记，与既有行为一致）。
    virtual void setReportedReady(bool ready) { (void)ready; }

    // Space 分配监听（962；版本前进才触发，重推幂等）。监听器在泵上下文
    // 同步回调（与 961 同纪律——锁外回调，不得重入 pump/state）。
    virtual void setSpaceAssignListener(SpaceAssignListener fn) { (void)fn; }

    // 已记账（版本前进）分配条数（观测面）。
    virtual std::size_t space_assign_count() const { return 0; }

    // 失联冻结监听（L101 失联策略切片）：链路 down 超 link_grace_ms 触发
    // kGraceExpired 一次/失联期；961 code 3 触发 kAdjudicationRejected（旧
    // 执行端自隔离）。readmit：961 accepted（code 1/2，World 重新受理本端）
    // 解冻——链路恢复本身不解冻（§4 不能自行解冻，复核以裁决为准）。均
    // 泵上下文锁外同步回调（同 962 纪律——不得重入 pump/state）。
    virtual void set_link_freeze_listener(LinkFreezeListener fn) { (void)fn; }
    virtual void set_readmit_listener(ReadmitListener fn) { (void)fn; }
};

// 工厂：world_host:world_port 主动拨号，961 按 request_id 记账。
std::unique_ptr<CellRegisterClient> makeCellRegisterClient(CellRegisterConfig config);

} // namespace world_app
