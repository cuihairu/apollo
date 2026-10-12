# apollo 巡检台账（仓库根 todo.md，patrol 建账 2026-10-13，同日随远端 46 commit 重盘）

> 盘点依据：README.md、`docs/architecture/adr.md`（ADR-001..017，ADR-014 已撤销）、
> `docs/design/` 权威设计稿、`docs/todo.md`（重构任务书 P0–P4 批次台账）、代码实读。
>
> **与 docs/todo.md 的分工**：`docs/todo.md` = 重构任务书批次流水账（本仓权威任务源，巡检不改写其结论，
> 仅在证据充分时代为翻牌过期 checkbox）；本文件 = 巡检三档盘点，记录「设计稿 ≠ 已完成」的审计口径。
>
> **统一验收口径**：批次台账既有验收 = 三树门禁（GAME_MODULE ON/OFF）ctest 全绿 + sanitizer 树零报告
> + 活体编队实证（docs/todo.md 各批次记录在案）；本机复核口径 = ctest 全绿。文档/设计稿一律不计入已实现。
>
> **2026-10-13 并行会话说明**：本轮盘点初版基于 696f37ca；期间远端两波前移共 ~54 commit
>（可观测性 L4–L9 全链、玩家迁移、更新/迁移演练、benchmark flush+persist 段、BI 分流、
> P3-2/P3-3/P3-4 收口、ADR-014 撤销回改等），本地无独有提交，已同步至新 HEAD 并按其全量重盘。
> 本文件旧版结论中与新 HEAD 不符者以本版为准；本轮本地增量=编队成员注册批（f4b5f79d，
> rebase 后 0de62ea9）。

## 一、已实现（代码 + 测试通过；批次 commit 证据见 docs/todo.md 批记链）

- [x] **核心框架**：`apollo::core::di` 构造注入容器 + `ApplicationHost` 六态生命周期 + `IHostedService`
  + `ConfigRegistry` —— P0-1（fd61ff7f）起持续交付。
- [x] **网络自持四层（net M1）**：modules/net/link 自持内核；nng 依赖替换迁移五批（批⓪–⑤）2026-10-12
  全清（9121ebc2 收口），gateway 准入/login↔base RPC/manager 控制面/cell 受理全链 M1 换装；
  验收四树 ctest 全绿 + sanitizer 零报告 + 活体编队实证。
- [x] **linkrpc 关联原语**（5ae86969）：契约 940/941 关联信封 + corr_id + call_async/poll/wait 桥。
- [x] **Scene/Instance 模型** + **PlayerAnchor 权威** + **换幕 scene_transfer** + **PlayerDirectory**
  + **AOI 3×3 网格** + **持久化栈**（JSON 档案/SaveQueue/PersistJournal）+ **Recovery/Reconnect**
  + **确定性战斗 BattleRuntime** + **契约系统**（XML+XSD+apollo_gen+golden）+ **Social/Guild/Party**
  —— P0/P1/P2 各批（docs/todo.md 批记链 25+ 项 commit 证据）。
- [x] **cell 受理面 + 延时应答**：队列化异步受理 + 水位/过载（ADR-015/016）+ pending-reply 升级
  （ADR-017 路径落地：2ffc4711 按 request_id 回 929、6b48984a gateway pending 关联映射+超时清扫、
  0b8273fc 受理入口唯一化收口 2b-ii/2b-iii）——P3-2 ②③ 随此消账。
- [x] **可观测性最小面（P3-3 批 A–L7 全链）**：日志四套收敛单套 + 结构化行格式（六键固定序+引号规则）
  + LoggerApp 立项接线（L1–L3）+ L4 push 通道+断连降级（8d8a1dc0→d8499672）+ MetricRegistry
  （Counter/Gauge/Histogram 三型，并发无丢失）+ metrics admin 通道（931 契约+admin exporter+编队接线，
  08641684/816f4b75/50b480b9）+ trace 子集（932 契约+linktrace 模块+cell 接收面+gateway 发送面 span，
  cef1b87e→4aade72b）+ 告警 notifier（规则表+静默窗+速率上限+webhook 重试签收，e24e8fe9/3fe6d499）。
- [x] **崩溃采集**：crash_capture 接入 crashpad + machined 对照链 + 符号化实跑（dump_syms/
  minidump_stackwalk build-id 三方互证）——批①②③④合并交付。
- [x] **进程编队（G-1）**：machined 监督面（roster+线性退避+超限放弃+死亡事件四型）+ discovery 库
  （Beacon/Registry/UDP 传输/发现层/死亡面 DeathSubscribe/DeathNotify）+ 目录镜像（APD2 wire 族，
  DirectoryMirror/DirectoryPublisher）+ FleetRecoveryCoordinator（排他相位+全量重报+双判据收敛）
  + 死亡行窗口处置（suspend window + gateway 组件映射）。
- [x] **玩家迁移（backup-revive §9.2）**：契约族 933–938（62d53d35）+ 服务器双面（22b0431e）+
  zone 迁移 pull on handoff（fb0cba36）+ directory moved epoch（2372c3fc）+ manager 编排器
  （af884542）+ migrate 命令接线（e4404801）+ 编排演练（897cfabf）。
- [x] **更新排空与演练工具**：drain 受理闸 SIGUSR1/2（fa201213）+ fleetctl probe（8b155a04）+
  route1 更新演练（33e577be）+ drill_update/drill_migration/drill_kill9 脚本。
- [x] **性能基准**：三模型 benchmark 桩（A/B/C 场景）+ 受理面实测 ~0.1µs/意图（批 D）+
  flush 段实测（934f5df2，B 场景 flush 为最大单项段——锚点入 capacity-and-benchmark §2.3）。
- [x] **DevEx**：dev_fleet.sh 一键起停（P3-4 批A）+ gateway ingress 接线（gateway_ingress_tests
  9 例，并行会话）+ **编队成员注册批（2026-10-13 本轮，0de62ea9）**——FleetMember 驱动（首拍心跳即
  注册/interval 节流/stop 幂等注销，测试先行 2 组 21/21 绿）+ 五服务 app 接线 + machined 成员身份
  日志 + dev_fleet roster 组件号列；活体实证 members=5（2=baseappmgr/10=base/11=cell/12=gateway/
  13=login 全身份可见）。
- [x] **测试面**：tests/ 70 个 test_*.cpp（gated by ctest，零依赖断言式框架；GTest 批默认关）。

## 二、部分实现（批次未收口，验收口径与期次随 docs/todo.md 批次）

- [ ] **P3-2/P3-3/P3-4 已全部收口**（2026-10-13，并行会话 + 本轮）：P3-2 ①persist 段（fd46aaf9）、
  P3-3 L1–L9（BI 分流 207f36a5、proc 存活+采样装配、trace 采样率编译期装配 0295e754，a3eec736 收口）、
  P3-4（编队成员注册=本轮 0de62ea9 + ingress 接线/L8）。批次台账 28/28 全勾。
- [ ] **移交余件**（P3-3 收口声明明记解除条件者，均具备动工条件）：①trace 四件套余件
  （RemoteEntityRef/RemoteMethodSchema/RouteResolver 与 RPC 链 trace——nng 底座已收口）；
  ②bare 924-928 双受理退役（消费面清零后）；③collector 归档滚动（单文件累加，L8 边界挂后续批）。
  验收：逐件按 logging.md/observability 对应节落门禁。期次：随编排。
- [ ] **P4-4 术语承接**：已由 697c49d5 关闭（term-contract 整体删除、BigWorld 术语对齐、
  glossary master registry ec21d9d3）——glossary 与代码/文档的持续对账属常态维护，不再单列。

## 三、只有设计（docs/design/ 有稿、无代码）

- [ ] **Lua 脚本**：`docs/design/scripting-lua.md`——modules 全树无 lua/sol2 痕迹，vcpkg 无条目。
  验收：scripting 模块立项 + attach/watcher/profile 语义最小实现 + 门禁。期次：未排期。
- [ ] **属性增量同步**：`attribute-sync.md`——`attr_batch` 契约已在 messages.xml，但 AttributeContainer
  的 per-viewer delta 管线未接。验收：属性变更→delta 事件→gateway 下行打通。期次：未排期。
- [ ] **battle-verification-service**：ADR-013 明示 VerifierApp 暂缓（随 Lua/玩法批）。期次：暂缓（有裁决）。
- [ ] **外部存储接线（MySQL/Redis/ClickHouse）**：ADR-007 现行=文件+journal 零外部依赖；全仓
  grep mysql/hiredis 零命中。验收：按数据稿定连接面后立项。期次：未排期。
- [ ] **Kafka 观测管道 + Prometheus/Grafana**：架构决策 #14 计划，仓内零实现（现行=admin exporter
  HTTP 面自拉）。期次：未排期。
- [ ] **protobuf+zstd 消息路径**：protobuf 现为「依赖+测试」不在消息路径（手写编码现行）；
  zstd 未入 vcpkg.json。验收：L1 帧+字典/压缩切换按 sdk-contract.md 落地。期次：未排期。
- [ ] **G-2 热备仲裁面**：ADR-010 已裁「案 A 先行、standby 不立项」——镜像流/仲裁两增量保持设计件。
  期次：有裁决搁置。

## 四、本轮验证记录（2026-10-13）

- 远端同步：696f37ca → 306419a8（46 commit，fast-forward，本地无独有提交；并行会话工作区
  /home/cui/workspaces/apollo 与本仓同源，双方改面无交叠——本批=编队成员注册，彼批=benchmark flush 段）。
- ADR-014 撤销复核：adr.md 状态行明示「已撤销（2026-10-12 用户令）……现行口径 = 优先复用
  BigWorld 现成词」；b17b96cc revert: restore bigworld process names 实证。旧版台账「进程改名」条目作废。
- 代码残余复核：`grep -rn "APOLLO_USE_NNG|nng_"` 代码 0 命中；mysql/hiredis/sol2/lua 0 命中。
- 编队成员注册批活体实证（本机编队）：`dev_fleet.sh up` → machined log `members=5` +
  `member component=2/10/11/12/13` 身份明细（port 9105/9104/9101/9002/9001）；`down` 五 app
  优雅停（Server stopped ×5）无伪死亡事件。
- 本机构建（vcpkg x64-linux，Release，Ninja，GAME_MODULE=ON）：ON 树全量构建 + ctest 结果
  见 docs/todo.md 编队成员注册批批记（本轮门禁）。

## 五、补充验证（构建完成后回填）

- ON 树（GAME_MODULE=ON，vcpkg x64-linux Release/Ninja）全量构建绿 + ctest **56/56 全绿**
  （2026-10-13 本机，含编队成员注册批新面 DiscoveryTests 21/21；跑于 306419a8+本批工作树上，
  未覆盖并行会话其后 8 commit 的 BI/persist 改面——该面以彼会话三树门禁批记为准）。
- OFF/sanitizer 树：本批改面（discovery 库/app 壳/machined 日志/dev_fleet.sh）不触及
  GAME_MODULE 门内代码路径，随下批全量跑（与本机并行会话的 asan 树错峰）。
