# apollo 巡检台账（仓库根 todo.md）

> 分工：本文件 = 巡检需求清单；`docs/todo.md` = 重构任务书批次台账（权威任务源）。
> 验收口径：三树门禁（GAME_MODULE ON/OFF）ctest 全绿 + sanitizer 零报告；文档/设计稿不计入已实现。

## 待办（需求）

- [x] trace 四件套余件：RemoteEntityRef / RemoteMethodSchema / RouteResolver 与 RPC 链 trace（49d3d3a8，14 用例）
- [ ] Lua 脚本：`docs/design/scripting-lua.md`——modules 全树无 lua/sol2，vcpkg 无条目（未排期）
- [ ] 属性增量同步：管线核心七批已交付（c7b6c3dc/b4449d9d/0d0b6def/2fcb80c2 + SceneAoi 接线批 + §5 优先级与节流派 + §8.2 写后日志入口，80 用例），余 GameConnection Facade / slice（未排期）
- [ ] battle-verification-service：ADR-013 明示暂缓（随 Lua/玩法批）
- [ ] 外部存储接线（MySQL/Redis/ClickHouse）：ADR-007 现行=文件+journal 零外部依赖（未排期）
- [ ] Kafka 观测管道 + Prometheus/Grafana：架构决策 #14 计划，仓内零实现（未排期）
- [ ] protobuf+zstd 消息路径：protobuf 现为依赖+测试不在消息路径，zstd 未入 vcpkg.json（未排期）
- [ ] G-2 热备仲裁面：ADR-010 已裁「案 A 先行、standby 不立项」（有裁决搁置）

## 已完成（证据）

- [x] 核心框架（`apollo::core::di` + `ApplicationHost` 六态生命周期 + `ConfigRegistry`）：P0-1 起，fd61ff7f
- [x] 网络自持四层（net M1）：nng 迁移五批 2026-10-12 全清，9121ebc2
- [x] linkrpc 关联原语（契约 940/941 + corr_id + call_async/poll/wait 桥）：5ae86969
- [x] Scene/Instance 模型 + PlayerAnchor 权威 + 换幕 scene_transfer + PlayerDirectory + AOI 3×3 + 持久化栈 + Recovery/Reconnect + 确定性战斗 BattleRuntime + 契约系统 + Social/Guild/Party：P0-P2 各批（docs/todo.md 批记链）
- [x] cell 受理面 + 延时应答（队列化异步受理 + 水位/过载 + pending-reply 升级）：2ffc4711 / 6b48984a / 0b8273fc
- [x] 可观测性最小面（P3-3 批 A-L7）：日志四套收敛 + LoggerApp + L4 push 通道 + MetricRegistry + metrics admin 通道 + trace 子集 + 告警 notifier
- [x] 崩溃采集（crashpad + machined 对照链 + 符号化）：批①②③④
- [x] 进程编队（G-1）：machined 监督面 + discovery 库 + 目录镜像 + FleetRecoveryCoordinator + 死亡行窗口处置
- [x] 玩家迁移（backup-revive §9.2）：契约族 933-938（62d53d35）+ 服务器双面（22b0431e）+ zone 迁移 pull on handoff（fb0cba36）+ directory moved epoch（2372c3fc）+ manager 编排器（af884542）+ migrate 命令接线（e4404801）+ 编排演练（897cfabf）
- [x] 更新排空与演练工具：drain 闸 SIGUSR1/2（fa201213）+ fleetctl probe（8b155a04）+ route1 更新演练（33e577be）
- [x] 性能基准：三模型 benchmark 桩 + 受理面实测 ~0.1µs/意图 + flush 段实测（934f5df2）
- [x] DevEx：dev_fleet.sh 一键起停 + gateway ingress 接线 + 编队成员注册批 0de62ea9（21/21 绿）
- [x] 测试面：tests/ 80 个 test_*.cpp（gated by ctest，ctest -N 44 项）
- [x] P3-2 / P3-3 / P3-4 全部收口：2026-10-13（批次台账 28/28 全勾）
- [x] P4-4 术语承接：697c49d5（`APOLLO_USE_NNG|nng_` 代码 0 残余）
- [x] 移交件② bare 924-928 双受理退役：9eac60d8
- [x] 移交件③ collector 归档滚动：67fa7fd8
