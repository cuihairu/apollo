# Apollo 实施清单

唯一目标命名：`Login`、`Gateway`、`World` 是进程职责；`Scene` 是 World 内部模拟单元；`Instance` 是几名玩家组队创建、完成刷怪与结算后销毁的临时副本。程序 `world-app`、入口 `WorldApp`、命名空间 `world_app` 是 World 职责在代码中的形式。

本文仅跟踪当前工作，不保留作废命名或按日期追加的讨论记录。目标与实现状态必须分开；状态机、网络原语或单测存在不代表完整生产玩法已接通。

## P0：文档与命名一致性

- [x] 在中英文架构入口、词汇表、玩家模型和 README 中统一 World / Scene / Instance 的职责。
- [x] 明确副本流程：小队发起、World 分配、创建、进入、运行、目标完成、结算、离开、销毁及资源释放。
- [x] 文档使用 VitePress locales：中文在 docs 根目录，英文在 docs/en。
- [x] 按 VitePress 标准修正文档国际化：语言目录页面配对、独立导航与主题文字、英文页面路由、无语言后缀页面；将配对和路由检查加入文档 CI。
- [x] 修正被误改的外部引擎源码名称；第三方资料引用真实上游标识。
- [x] 恢复当前有效 ADR 核心规则及编号含义，修正本轮涉及的关键引用。
- [x] 逐项复核其他设计稿引用的旧章节号和批次说明，改为当前规则与实现状态，不保留作废结论。
- [x] 检查 234 对中英文页面及页面路由；仓库外隔离目录 VitePress 正式构建通过，本地化页面与搜索索引已生成。
- [x] 核对 GitHub About 描述和 topics 与最终定位一致。

## P1：World 应用接线

- [x] 将游戏逻辑应用统一为 world-app / WorldApp / world_app，并同步 Gateway 配置、CLI、链路身份、日志指标、开发编队及测试。
- [x] 完成全量构建及 76 项 CTest（含契约 golden 与真实托管服务集成检查）。
- [x] 将玩家资料、会话绑定和持久化迁入 World 托管的 PlayerStore，删除独立同义进程入口；真实 TCP 验证激活、会话绑定、档案保留与服务缺席拒绝登录。
- [x] 将在线目录、落点和迁移编排归入 Login 托管的 OnlineDirectory，开发编队只启动 Login / Gateway / World 业务进程；玩家游戏档案不写入 Login。

## P2：小队副本完整流程

- [x] 复用现有 World、Scene、Instance 和 Instance 八态状态机，不重写同义生命周期。
- [x] 补齐底层回收约束：同一 Scene 只绑定一个 Instance，活动副本保护 Scene，终态回收同时移除两项注册，取消流程不发成功奖励、玩家退出后才允许销毁；通过回归测试。
- [x] 接通小队创建请求：验证队伍、成员、人数和准入条件，选择有容量的 World，创建独立 Instance 与临时 Scene。（d4d55f65：apps/common SquadInstanceService 受理面——队伍存在/队长/人数上限/在线谓词/成员忙校验，本地优先按容量选 World，独立 Instance + 临时 Scene 一对一创建，终态释放双回收；SquadInstanceTests 6/6，门禁 77/77 绿。客户端→Gateway→World 真实上行仍留待后项。）
- [x] 接通成员进入和准备完成后启动玩法，隔离副本内实体、AOI、任务与脚本状态。（476512c2：受理快照校验进入、首进驱动 Create→Initialize→Waiting、全员到齐队长 Waiting→Running、实体/AOI 场级隔离——独立 Scene 私有实体集与 AOI，SquadInstanceTests 9/9、门禁 77/77 绿。副本私有脚本状态随 scene 挂载收口：ScriptRuntime 归属 gameLoop 单写者，场景幂等补挂（206bbfb4）遍历 world 全部 scene——临时副本 scene 建立次拍自动入列，started/closing 钩子留给宿主扩展点；真实上行链路 e2e（918255c3）经真进程验证副本 scene 创建与玩法启动；任务隔离随任务系统属后续能力。）
- [x] 接通刷怪、目标完成与失败判定，驱动 Instance 进入结算阶段。（860472fd：SquadInstanceService 玩法面——Running 期设目标（击杀配额+超时 tick，基线取 Instance 玩法时钟）、配额封顶刷怪入副本私有 Scene、击杀上报只认本副本怪且达标 finish() 进结算（成功线）、超时 cancel() 进结算不发成功奖励（失败线）、结算期拒绝迟到玩法输入、进度查询随绑定释放清除；SquadInstanceTests 12/12，门禁 77/77 绿，build-off 全量重建绿。gameLoop 驱动与客户端击杀上报的宿主接线随真实上行项。）
- [x] 结算通过既有奖励/持久化接口幂等落账，定义重试、失败与恢复处理。（77f03803：SettlementLedger 受理即开账（名册快照）；battle 与 instance 同生——enter 段收参战者、tick 段随玩法时钟、finish 段结算经 IRewardSink → AnchorRewardSink 落锚点 exp + mark_dirty 随 SaveQueue 落档（既有持久化接口）；幂等口径 (instance,player) 一条账，applied 后重复落账/重试/恢复重放一律拒绝；挂账重试只补曾尝试未落条目；restore 回填已落标记防重启双记；超时失败线不结算不发奖励；宿主经 PlayerStore::findAnchor 惰性解析装配；SquadInstanceTests 15/15、门禁 77/77 绿、build-off 全量重建绿。battle 分值输入路径与账本跨进程持久化随真实上行/排空项。）
- [x] 结算后移出玩家、拒绝迟到输入，释放实体、AOI、脚本/任务及临时 Scene，销毁 Instance 并从 World 注册表移除。（470d8528：SquadInstanceService::dismiss 收尾面——入结算（Finishing 起）才可收尾，按受理名册移出玩家 → 结算链推进至终态 → release 终态双回收（实体/AOI/临时 Scene 随 Scene 释放、closing 钩子宿主销毁脚本/任务残留）→ 绑定/玩法/结算账面清除、容量归还；enter() 迟到进入在结算期改报 StateRejected（原误报 Duplicate），start/刷怪/上报/tick 均 NotRunning 拒绝，收尾后全入口 NoBinding；成功线与失败线（超时）收尾均覆盖；SquadInstanceTests 18/18、门禁 77/77 绿、build-off 全量重建绿。）
- [x] 覆盖无人进入、全员退出、掉线、队伍解散、超时和进程异常的收尾策略及测试。（26abe6ed：无人进入/队伍解散/宿主排空走 abort 策略收尾（Create..Running 取消线，不发成功奖励，收尾即释放绑定）；Running 期全员退出 = 失败判定（leave 面按名册校验，掉线同口径，末位退出自动进结算，中途退出可重进）；超时线此前项已覆盖；进程异常口径 = 副本临时态随进程消亡、重启全新受理、结算账以 restore 回填防重放双记（重启用例覆盖跨进程不双记）；SquadInstanceTests 22/22、门禁 77/77 绿、build-off 全量重建绿。）
- [x] 接通并验证客户端 → Gateway → World → Scene/Instance 的真实上行、结果和状态下行；现有 broadcastToViewers 日志不能作为下行完成证据。（918255c3 全链真进程 e2e（SquadWireE2eTests）：真 gateway M1Egress 经 loopback TCP 直连真 WorldApp——上行 CELL_CREATE_ENTITY 会话进图、建队/入队/开启/进入/启动/击杀上报全族契约翻译 932 意图、gameLoop tick 边界受理；下行结果帧（PartyResult/SquadResult）与状态帧（SquadState 含存活怪列表）经 939 client_deliver → egress 客户端投递口回带——断言事实源 = 投递口收到的协议帧内容（日志不算），终态 Destroyed 回执到齐全员三会话；击杀分值经 battle 输入序落锚点 exp（上报人归属），结算后绑定与容量归还（active_count/used_capacity 归零）；契约层 ff6e5032（客户端族 0x0070-0x0078 + 内部 939/946-951 + 错误码 1100-1117）、协议层 9fd9690b（native 编解码 + 截断拒收，6/6）、执行层 031a0ee5（击杀→battle 输入→结算，目标完成 defer 一拍让分值先入序，23/23）、网关层 f27d6b8d、门禁 build-examples/build-off 双树 78/78 全绿。副本私有脚本状态随 scene 挂载在 gameLoop 收口（见上一条勾销）；账本跨进程持久化随排空项。）
- [x] 验证单 World 多副本隔离，以及多个 World 的分配与选路。（92b08a10：并发隔离用例——同 World 两副本并发 Running，怪号全局发号两集合不相交、交错击杀进度各进各账、单边达标结算收尾后邻副本绑定/进度/存活怪全无感、收官后绑定与容量全数归还；多 World 选路用例——本地满载溢出按 world_id 升序取首个空位（注册顺序无关）、远端派生钩子按选路结果各调一次且 id 透传、容量逐 World 独立记账、全满 NoWorldCapacity、本地槽回收后本地优先复用不漂移（abort/dismiss 本地面，远端绑定实际回收随跨进程条目）；SquadInstanceTests 25/25、门禁 build-examples/build-off 双树 78/78 全绿。）

## P3：玩家迁移与会话恢复

- [x] 将目录镜像、死亡订阅、全量重报与恢复协调接入 Login / World 当前入口，并迁移进程演练脚本；不能用底层原语测试替代当前进程链路验收。（f8d88f81 Login 承接 baseappmgr 三面——镜像 owner（目录事件 → Delta + 周期快照，snapshot published 日志面）、恢复 intake（FullReport 收包 + FleetRecoveryCoordinator 收敛裁决 + 准入闸排他）、machined 死亡订阅（zone/gateway 行窗口处置 + 窗口满 sweep）；2376f9a0 World 承接 base-app 报告方——锚点快照 → FullReport 启动即报 + 周期刷新，fleet 组件号同源一进程一身份；c116f73b 真实进程链验收 FleetRehomeChainTests：真 LoginServer + 真 WorldApp + machined 替身全 UDP 回环——恢复排他期门闸拒 → 启动重报收敛放行、锚点重报 intake 落目录、镜像消费侧投影（真 Delta/快照 wire）、死亡通知 zone 反查 Suspended、窗口满 sweep 条目终结 + SessionDown 达镜像、非 zone 行不越界；14285c37 drill_kill9 迁 login-app/world-app 实测全链断言过（死亡检测 0.02s/重启 1.0s/重报 intake 1.9s/镜像重发 2.9s），migration/update 演练挂待迁注记归各自账面条目；门禁 build-examples/build-off 双树 79/79、san 树 78/78 全绿。）
- [x] 明确跨服场景设计预留：区服归属与 World 运行位置分开，复用 Scene/Instance，定义准入、单权威交接、结算与返回规则；暂不启用跨服部署。
- [ ] 跨服需求启用时，扩展契约中的原区服、World 进程代次、玩家权威代次、准入凭证与返回位置；区服身份不得直接复用 home_zone_id。
- [ ] 跨服需求启用时，接通区服分组、跨服 World 容量分配、两区服同场景准入、幂等结算和返回原区服；覆盖原 World 不可用、安全回退、掉线及提交结果不明的恢复。
- [x] 复用同进程 SceneTransfer；分别验证跨 World 的源端冻结、导出、目标确认、路由切换、源端清理和失败恢复。（b358cd90/f51baad8：真实进程迁移链验收 test_migration_chain——真 LoginServer（目录 owner + 942 受理面）+ 两真 WorldApp（源 zone 2 / 目标 zone 5，真 PlayerStore M1 面 933/935/936/937 经 StoreDial 真拨）：失败恢复 A 源不可达干净拒绝（目录条目逐字段同迁移前）、失败 B 目标不可达冻结已生效源停留 Transferring 且目录不改指、主线 ①→④ 冻结/导出/接权/改指+代次推进（目标锚 Online、原区服保留、会话双锚恢复、落盘档案字段与源一致、SessionMoved+④ 立即推达镜像 wire）；b358cd90 报告两道防回卷——报告条目 zone=承载域（非原区服 home zone，迁移改指不被周期重报回卷）+ 源冻结锚点（Transferring）离在册集不再上报（否则目录在两报告方间来回翻转，测试构造序使采样可判，去过滤确定失败）；源端清理=⑤ 停留 Transferring 如实边界（§9.9-2 gateway 缓冲未接线，无 unfreeze 面）；d3a5e9c1 drill_migration re-home 版（login-app 编排面 + 两 world-app，fleetctl 942 驱动，镜像观察端 APD2 wire 断言 + 存档 sha256 恒等 + 30s 周期快照回卷断言 + 源重报 sessions=0 冻结离册）实测全过（freeze 44ms/handoff 130ms/CLI 0.195s）；门禁 build-examples/build-off 双树 80/80、san 树 79/79 全绿。）
- [x] 用迁移标识、权威代次和业务幂等拒绝重复接管、迟到输入及重复结算。（71318a48/8adeef57：迁移标识幂等——目录按 (player, request_id) 缓存成功 943 回执，同标识重放原样回带不重跑编排（失败不缓存保留 ops 重试语义；request_id=0 不进账；fleetctl 补单调标识）；重复接管拒绝——目标店权威代次高水位 adoptedEpochs_，937 new_epoch ≤ 已接代拒于拉取之前（迟到旧交权不得按旧档二次 adopt 覆盖目标侧新写）；迟到输入拒绝（冻结只读三道）——源店 bindSession 拒绑（防 Transferring 翻回 Online 重获写权）、DB 存档 wire 拒（防旧路径置回写态+入段）、锚点 add_progress 冻结静默拒（防脏）；迟到结算拒绝——SettlementLedger 冻结锚点 AnchorUnavailable 挂账不落（与既有 (instance,player) 幂等两道合口）；test_migration_chain 新增 6/7/8 三段断言（重放全不动：epoch/目标锚/落盘档案逐项；旧代次 937 直拨拒于 duplicate takeover；绑定/存档/进度/结算四写面全拒），先红后绿（重放推进 epoch→修复后绿）；门禁 build-examples/build-off 双树 80/80、san 树 79/79，drill_migration 与 drill_kill9 复跑全过。）
- [ ] 完成跨 Gateway 重连后的会话恢复，明确重复登录、旧连接失效和路由持久性。

## P4：两种更新方案与排空

- [ ] 协议更新：补齐版本协商、新旧契约兼容窗口及兼容检查，分别验证兼容变更和破坏性变更的发布/回滚。
- [ ] 仅代码更新：复用 Lua prepare/commit/canary/rollback 和现有 ModuleRollout 原语，补齐跨 World 的可靠投递、观察和失败回滚接线。
- [ ] Gateway 排空：停止接受新连接，保留既有连接服务至自然退出或明确期限；定义强停结果。
- [ ] World 排空：停止分配新副本，等待活动副本结算并销毁；定义超时、强停与恢复策略。C++ 更新通过进程替换完成。
- [ ] 分别验收服务持续接入、免重新登录、TCP 连接连续和活动副本连续；未经端到端验证不宣称无感热更。
- [ ] 建立小队副本销毁、跨 World 迁移、Gateway/World 排空及升级回滚的可重复演练。

## 后续能力：保持暂缓，需求触发再实施

- [ ] 外部存储（MySQL/Redis/ClickHouse）：具体部署或容量要求触发后接入；当前仍是本地档案和 journal。
- [ ] protobuf/zstd 进入真实消息路径：跨语言互操作或带宽实证触发后实施；现有依赖和生成产物不代表运行路径已使用。
- [ ] Kafka / Prometheus / Grafana 等外部观测基建：实际观测部署需求触发后实施。
- [ ] 战斗验证：有具体玩法内容可复算、验证规则与业务目标后实施。

跨机热备与仲裁维持未立项目标；machined 的监督重启不作为热备能力验收。
