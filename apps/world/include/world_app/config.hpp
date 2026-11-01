#pragma once

#include <string>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "world_app/world_types.hpp"

// 契约路由表 JSON 的构建期默认路径（CMake 注入生成物路径；裸编时为空 =
// M1 端点装配失败显形，不带隐式回退）。
#ifndef APOLLO_WORLD_APP_CONTRACT_ROUTE_JSON
#define APOLLO_WORLD_APP_CONTRACT_ROUTE_JSON ""
#endif

namespace world_app {

struct WorldAppConfig {
    std::uint32_t worldId = 1;  // 执行进程所属 World 逻辑 ID
    // world 管理端角色（L104 收口片一）：manager 是唯一受理角色（缺省）。
    // 执行端由 cellServer 独立入口承担（不再 --role=cell 委托）。解析见
    // role_options.hpp——未知/缺值/cell 入口已显式拒绝；管理/执行职责随
    // P2 设计切分，role 先行入账（日志/配置可见，不静默丢弃）。
    std::string role = "manager";
    // 小队副本容量（P2）：本 World 可同时承载的独立 Instance 数上限。
    std::size_t maxInstances = 16;
    // 监听配置
    std::string host = "0.0.0.0";

    // net M1 接收端（M1 上层接线批 2b-ii；2b-iii 起 WorldApp 唯一受理入口）：
    // InterServerLink passive 端点，网关经它投递 cell 意图（924-928）
    uint16_t m1Port = 9101;
    std::string m1RouteJson = APOLLO_WORLD_APP_CONTRACT_ROUTE_JSON;
    // 进程代次透传（§5.7；0 = 进程自铸）——真进程不配（自铸即不同）；同进程
    // 模拟替换（e2e）在此注入不同代次。
    std::uint64_t m1AuthorityEpoch = 0;

    // cell 角色注册拨号（L101 次切片；契约 960/961）：启动后主动拨所属
    // World 的 M1 受理面注册（逻辑 ID + 启动代次 + 自报面）。port = 0 =
    // 不注册（缺省单机 manager 形态，配置面缺席即静默）。
    std::string registerHost;
    std::uint16_t registerPort = 0;
    // 逻辑 cell ID（注册开则必须非 0；PID/IP 不当身份，重启保留同 ID 换代次）。
    CellExecutorId cellId = 0;

    // login 服务注册拨号（L123 拨号次切片，manager 角色）：启动后主动向
    // loginServer 服务注册面（964/965）登记逻辑身份（kind=World，
    // logical_id=worldId，代次自铸/注入）。port = 0 = 不注册（缺省单机
    // 形态静默；装配失败降级警告不拦启动，同 cell 注册面惯例）。
    std::string serviceRegisterHost;
    std::uint16_t serviceRegisterPort = 0;

    // 注册表过期窗毫秒（L101 心跳批，manager 侧单方裁决）：登记条目
    // last-heard（registered_at_ms，每次受理刷新）超窗未刷新即整条摘除——
    // cell 侧周期心跳（960 重发）维持存活。0 = 永不过期（单机形态无注册面
    // 时无差；cell 全停发 30s 后摘除是缺省策略）。
    std::uint64_t cellExpiryMs = 30000;

    // 代次墓碑日志（L101 管理日志切片，manager 侧）：append-only 文本行
    // `cell_id|start_epoch`，装载取每 cell 最大值重建水位。空 = 关（现状
    // 内存水位，重启忘却）；配置即受理时追加落盘，旧执行端隔离跨重启成立。
    std::string worldJournalPath;

    // cell 侧失联宽限窗毫秒（L101 失联策略切片）：注册链路 down 超窗即冻结
    // 内核（停模拟 + 关受理闸——§4 失联策略；解冻只随 961 复核受理）。0 =
    // 永不冻结。缺省 15000 = 3 心跳拍（默认 5000）容忍，低于过期窗 30000
    // ——cell 先自冻结、World 后摘除。
    std::uint64_t linkGraceMs = 15000;

    // World 进程启动时刻（steady 毫秒）。manager 装配时注入，作为 journal
    // 装载主缺少历史时间戳时的重注册窗口基准——与 registered_at_ms 和
    // cellExpiryMs 使用同一单调域与同一裁决窗，不引入第二套过期口径。
    std::uint64_t bootAtMs = 0;

    // Space 分配池（L101 分配面首切片，manager 侧）：池内 Space 分配给注册
    // 合格 cell——合格 = 就绪、不自报排空、未被 World 单方缩容、且声明能力
    // 覆盖本 Space 的 needs（容量申报裁决），同窗存在负载更低的合格候选
    // 则让位；池内无合格候选的 Space 留池不认领（world-cell-management §5
    // 「没有容量明确拒绝」——不落到不合格 cell 上凑数），后续注册不重分
    //（重平衡/回收/拓扑版本随 L108）。空 = 分配关（缺省单机形态不变）。
    struct SpaceSlot {
        std::uint64_t space_id = 0;
        // 能力位要求（容量申报裁决）：cell 声明的 960 capabilities 须覆盖
        // 此位掩码的全部位才有资格认领；0 = 无要求（任何合格 cell 均可，
        // 含未声明 capabilities=0 的——现状语义）。
        std::int32_t needs = 0;
    };
    std::vector<SpaceSlot> assignSpaces;

    // Base 业务受理面（P1 结算迁入）：结算落账 = 返回业务请求拨 base
    // store M1 面（958/959）。port = 0 = 面关（结算挂账降级态——L19 资料
    // 归属切换后 World 无锚点解析器）。
    std::string baseBusinessHost = "127.0.0.1";
    std::uint16_t baseBusinessPort = 0;

    // 结算凭据目录（L67 去重凭据面生产接线）：(小队, 玩家, 分值) 凭据
    // 持久提交——settle 与 release 崩溃窗内重启装载回填，重放不双记。
    // 空 = 不持久化（凭据仅进程内存，降级态）。
    std::string settlementCredentialDir;

    // 空间配置
    std::string spaceName = "main_world";
    float spaceWidth = 1000.0f;
    float spaceHeight = 1000.0f;
    float gridSize = 100.0f;

    // AOI 配置
    float viewRadius = 200.0f;

    // 多 Space 声明面（L100）：cell 进程启动窗装配的额外 Space（默认 Space 1
    // 之外，每 Space 一个 scene + 路由，Instance-less——实例随玩法副本流程
    // 自建）。几何面沿用上方全局尺寸字段，map 资产引用共用唯一在册资产。
    // 重复声明幂等（add_space 口径）；CLI 入口另行拒绝重复/非法 id。
    struct SpaceDecl {
        std::uint64_t space_id = 0;
        std::string name;
    };
    std::vector<SpaceDecl> spaces;

    // Tick 配置
    int tickRateMs = 50;  // 20 ticks per second

    // 脚本宿主（scripting-lua §2/§5.1）：启动装载序 = handler 面 → 契约 →
    // boot 源（契约装载闸要求 handler 先行在位，boot 源可读 contract 全局）。
    // 各文件路径空 = 不加载；任一步加载/装载失败不停机（fault 走 runtime
    // 事件面，cell 继续服务）。
    std::string scriptHandersFile;
    std::string scriptContractFile;
    std::string scriptBootFile;

};

} // namespace world_app
