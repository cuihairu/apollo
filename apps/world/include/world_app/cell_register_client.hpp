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
//（code 1），旧代次拒绝（code 3）原样显形给宿主。心跳失效/重注册窗口与
// 旧执行端隔离仍待做（L101 后续切片，超时重发随心跳批）。

#include <cstdint>
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
    // 握手代次（§5.7；0 = 进程自铸）。
    std::uint64_t authority_epoch = 0;
};

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
};

// 工厂：world_host:world_port 主动拨号，961 按 request_id 记账。
std::unique_ptr<CellRegisterClient> makeCellRegisterClient(CellRegisterConfig config);

} // namespace world_app
