# Apollo 实施清单

> 已解冻 2026-10-22，恢复逐批推进。

目标职责与程序短名：gateway、login、base、world、database、machined；cell 是 world 同一二进制的执行角色。world 管理执行进程与 Space/Cell，cell 执行模拟；Cell 是空间分区，不是进程。Space 对应现有 Scene 的空间语义，Instance 保留临时玩法生命周期，不增加同义容器或擅自改代码名。

## P0：文档与命名

- [x] 建立 BigWorld 原词 → Apollo 角色 → 现有代码 → 实现缺口的中英文对照；保留真实第三方名称，清除冲突目标，不保留历史备选。
- [x] 明确 worldServer/CellAppMgr、cell/CellApp、Space/Scene、Cell 分区及 Instance 玩法边界；默认一个执行进程，每 Space 一个 Cell。
- [x] 明确 Base 长期对象、Real/Cell 实体、Witness 部分原语及 Ghost 预留，不声称完整 BigWorld 已实现。
- [x] 明确注册、拓扑权威与位置投影，machined 只监督本机，World/cell 独立部署不要求同机 machined。
- [x] 区分支持协议更新的进程替换与仅代码更新；Base、cell、World 管理端各交接不同状态，TCP/会话/活动连续分别验收。
- [x] 保持 VitePress 原生 locales：中文根目录、英文 en，各自正文与导航，新增对照入口并纳入文档 CI。
- [x] 专用 Profiler、OpenTelemetry SDK/OTLP exporter 标为未集成，自研日志、MetricRegistry、span/trace 不作为集成证据。
- [x] README 主标题未居中，改用 h1 align=center（README.md 与 README.zh.md 顶部主标题改 `<h1 align="center">Apollo</h1>`，commit 27674136（hostSimulation 移除）+ 2fa54622（world 目标移除 cell_main + 文档刷新））
  - 证据：commit 27674136

## P0.1：角色实体与应用命名迁移（待实施）

依据：[玩家对象模型与行业引用](docs/design/player-object-model.md)、[World/cell 设计草案](docs/design/world-cell-management.md)。本节按本轮讨论登记，尚未修改代码或验收；下面章节中的旧程序/类名描述当前实现，实施后同步清理。行业通用语义为角色实体的 Base/Cell 部分；具体 C++ 命名空间是 Apollo 的实现约定，不宣称是上游原名。

- [x] 固定代码迁移表（todo L21 + docs/design/player-object-model.md §命名 L30 双源钉死）：`apollo::game::session::PlayerAnchor` → `apollo::game::base::Avatar`；`apollo::game::world::Avatar` → `apollo::game::cell::Avatar`。
- [x] Base 部分头源与命名空间已迁到独立 Base 模块（modules/game/base，`apollo::game::base` / target `apollo::game_base`，链接 `apollo::game_core`；`apollo::game_session` PUBLIC 链接 base，消费方经传递获得）。
  - 证据：测试 86/86
- [x] Cell 部分头源已迁到独立 Cell 模块（modules/game/cell，`apollo::game::cell` / target `apollo::game_cell`）：`modules/game/world/src/avatar.cpp` + `include/apollo/game/world/avatar.hpp` → `modules/game/cell/src/avatar.cpp` + `include/apollo/game/cell/avatar.hpp`，`apollo::game::cell::Avatar/AvatarState/AvatarPtr` 三符号随迁（仅依赖 game_core + game_base，cell→base 单向）；world 模块删 avatar.cpp 并 PUBLIC 链接 `apollo::game_cell`（+`apollo::game_base`），`modules/game/CMakeLists.txt` 注册 cell 于 world 之前。
  - 证据：测试 86/86
- [x] 类型引用全量更新已随 L22/L23 完成（PlayerStore/apps/common、WorldApp、Scene/Instance、迁移 migration_wire+scene_transfer、恢复 recovery、奖励接收端 squad_instance set_avatar_resolver/AvatarResolveFn、全部测试与 examples）。
  - 证据：commit a32dec42；测试 85/85
- [x] 迁移对稳定标识零影响（3802d827+a32dec42 diff 复查）：PlayerID/PlayerId/EntityId 强分型未动；PlayerData 及存档 JSON 键未动（database_service.hpp 0 diff）；sdks/contract/messages.xml 0 diff（方法编号 933-938/958-959 等全保持）；modules/protocol/messages.hpp 唯一改动是注释（AnchorState 字符串 → AvatarState 字符串）；wire 状态串值逐字节保持（toAvatarStateString：loading/online/transferring/disconnected/saving/offline，与迁移前 toAnchorStateString 输出一致）；anchor_epoch 契约字段保留原值语义。
  - 证据：commit a32dec42；测试 85/85
- [x] 应用目录与目标已统一：`apps/gateway-app`→`apps/gateway`（target gateway-app→gateway）、`apps/login-app`→`apps/login`（login-app→login）、`apps/base-server`→`apps/base`（baseServer→base）、`apps/world-app`→`apps/world`（world-app→world）；database/machined/logger/admin/fleetctl 名实一致未动；`apollo_package_crash_handler` 随目标改名重挂（build 输出确认 crashpad handler 打包跟新名）。
  - 证据：测试 86/86
- [x] world 共用二进制分角色：无参数或未指定角色默认管理端，`world --role=cell` 启动执行端；错误角色/缺值明确拒绝，cell 不递归派生。
  - 证据：测试 87/87
- [x] 将 Base 离线激活、并发登录复用已有对象、动态迁移与当前归属解析列为后续行为验收：离线不创建第二写入者；支付/结算按稳定玩家身份投递，订单不永久绑定 Base 进程 ID；资料归属不等于进程不变。
- [x] 同步中英文词汇表、架构图、类型映射与开发说明；迁移完成后删掉旧目标名称、旧类型名、过渡对照和死链接，不保留生产代码别名/转发头或两套命令。
  - 证据：测试 87/87
- [x] 验收真实默认配置与 game 模块配置的构建/测试，覆盖 Base 激活/恢复/持久化、Cell 入场/离场/模拟、结算和迁移；Windows/Linux/macOS CI、双角色真进程启动及文档国际化/构建/内链均通过。
  - 证据：commit 704eee8d；测试 86/86

## P0.2：项目初始化 CLI 与开发脚手架（待实施）

现状：仓库有 CMake、scripts/dev_fleet.sh、编队运维和 CI，但现有服务 CLI 没有 init 项，不能称为已交付项目初始化。参考 [Mistlane 核对](docs/analysis/mistlane-review.md)的实际入口，按 Apollo 目标角色生成，不搬旧项目的角色名或游戏业务。

- [x] 明确 init 子命令的唯一 CLI 入口、默认模板和帮助说明；区分运行参数、项目生成与测试/压测工具，不向每个服务复制一套初始化器，不把 CI 描述成在线服务。
- [x] 生成最小项目的配置、Lua 入口、契约入口、目录说明、忽略规则和启停脚本：gateway/login/base/world/database，world 自动维护 cell；machined/独立 interfaces 按部署需求启用。
  - 证据：测试 87/87
- [x] 统一项目根路径和逐实例运行目录；模板不依赖启动时 cwd，角色、端口、构建版本、日志/资料/管理记录路径明确，多个 cell 不共写目录。
- [x] 提供有真实 readiness 的最小编队启动/停止/状态命令；处理依赖失败、超时及只回滚本次新启动成员，保留已有实例，避免固定 sleep 冒充就绪。
- [x] 生成可审查的 CMake/CI 配置与开发验证入口，覆盖构建、契约生成、测试和文档；CI 由仓库 workflow 执行，不装入业务处理循环。
- [x] 验收非空目录拒绝覆盖、非法名称/路径、从其他 cwd 启动、文件权限、无秘密配置、最小客户端登录/进入玩法、停止清理及失败回滚；文档状态段已如实翻面。

## P0.3：两类外部入口与图文接线（待实施）

- [x] 游戏 gateway 与 HTTP interfaces 均支持按监听独立启用 WS/WSS，允许多个监听及明文/TLS 共存；游戏 WebSocket 进入游戏会话，HTTP 服务的 WebSocket 使用专用 upgrade 路由和连接生命周期，普通 HTTP 请求仍走 HTTP handler。

- [x] 游戏 TCP/WS/WSS 接入接到 gateway 的同一游戏帧/会话/意图路径，分别路由 Base 与 cell；WebSocket 升级不进入 interfaces 的 HTTP 业务 handler，内部协议与传输封装分离。
- [x] 为游戏 WS/WSS 补客户端消息到 Lua/场景逻辑、稳定 session 寻址下行、服务端主动推送/广播、AOI 同步、有界积压、慢读处理及重复/乱序规则；不能以单次 echo 请求/响应或已有通用 websocket 库作为完整接线证据。
- [x] 分别验收 TCP、WS、WSS 的登录、Base 业务、cell 输入/异步下行、断线重连和迁移路由刷新；WSS 覆盖证书链/主机名错误拒绝和密钥配置，协议更新与 TCP/WS 会话连续分别记录。
- [x] 实现订单/通知可靠受理与持久待办，确认收到不等于发货完成；通过稳定玩家/角色身份关联订单，再查询当前 Base 和归属代次。
- [x] Base 唯一激活/写权、订单校验、奖励与发货去重凭据原子持久提交，覆盖通知重复/乱序、回执丢失、离线激活、并发登录、Base 迁移及接口/存储重启；当前 DB_LOAD/DB_SAVE 原型不能代替订单事务。
- [x] 首页及架构图分别展示游戏 WS/WSS、HTTP 回调、动态 Base 查询与持久发货；区分运行进程、CLI 工具和 CI，并标注实现/规划。

## P1：独立 Base 与 database

- [x] 完成 baseServer 在默认编队中的资料归属（205cac3d）、基础业务与结算迁入（4fe7a014）；独立入口已具备，离场/Instance 销毁不卸载长期资料已验收（857847e9：成员中途离场 + 成功线销毁三段后，锚点仍可解析、存量分值原样、在场者结算分值照常落账；真实 Base 侧销毁断言随 squad_wire_e2e——Destroyed 后名册锚点全 Online + killer exp > 0；World 域无卸载通道：PlayerStore 无卸载 API，唯一写口 = 958 结算入账）。
  - 证据：commit 857847e9
- [x] 基础业务与结算迁入落地（4fe7a014）：契约 958/959 player_score_credit request/reply + business_wire 编解码 + PlayerStore M1 dispatch 受理（accepted=0 语义 = 无锚点/Transferring 挂账拒绝）；SettlementLedger 增 BusinessSink 出口（Ok=applied 收口，其余挂账保留分值 retry 同路径重提——幂等归调用方账本，Base 只受理不记重放）；WorldApp 装配 StoreDial（--base-business-host/port，dev_fleet 拨 9201，面关 = 结算挂账降级态）；squad_wire_e2e 锚点落账断言回归 Base 侧（killer exp > 0）+ 专项 BaseBusinessWireTests 五步（常规落账/无锚拒绝/冻结拒绝/账本幂等双投/面停挂账）；contract_handlers.lua 补 958/959 占位行过同步闸。
  - 证据：commit 4fe7a014；测试 85/85
- [x] baseServer/database 进默认编队先落地：dev_fleet.sh 拉起 database 9200（data-dir/journal 落编队运行目录）+ baseServer 9201（存储后端 database 9200，存储不可达即拒起、由 machined 退避重试；编队位置在 database 之后）。
- [x] 资料归属切换落地（205cac3d）：WorldApp 弃档案宿主——store/报告面/结算锚点解析器/credential 目录全撤（config/main/gameLoop 同步清场），档案宿主 = baseServer 9201（dev_fleet 档案客户端拨 9201）；baseServer 装备 store 报告面（--report-host/port/component-id/zone-id/interval-ms）+ collect_mirror_entries 收集面（zone = 承载域、Transferring 过滤）；六测试套重构（hosted_services/fleet_rehome/migration_chain 以 StoreHost 替身同口径装配、squad_wire_e2e/world_drain/update_continuity 弃档案种子——结算挂账为设计降级态，收尾不阻塞，锚点落账断言随基础业务迁入回归 Base 侧）。
  - 证据：commit 205cac3d；测试 85/85
- [x] 完善 database 的有界异步保存队列（磁盘 I/O 移出 RPC 泵线程）、存储路由与写入代次（三款全落地）：有界队列 + worker 面 = 446626fd（满载背压拒收不丢数据）；存储路由 + 串行化 = 499111be（PlayerStorage 注入路由本地/远程，单进程 storeMutex_ 串行 + SaveQueue 按玩家分槽 FIFO）；写入代次 = database 半（writeGen 单调校验/同代幂等/低拒）+ Base 半（6eada1b7 写端自持代次账）。
  - 证据：commit 6eada1b7
- [x] 有界保存队列先落地（446626fd）：SaveQueue 排队+在途容量上限（saveQueueCapacity，默认 4096，0=不设界），满载背压拒收不丢数据——载荷 journal 先段，脏位/下轮 flush 重取；磁盘 I/O 已在独立 worker 面（P1-4）、存储路由与写入代次已随 PlayerStorage 接口与 journalSeq 水位落地（499111be）。
  - 证据：commit 499111be
- [x] 从当前 WorldApp/模拟宿主移出完整档案、长期写权和直接存储依赖；定义只读玩法快照与返回业务请求，禁止跨界使用本地 Anchor 指针修改资料。
- [x] Base 串行修改资料，database 对玩家保存排序并验证写入代次/版本，拒绝旧进程与旧快照覆盖。
  - 证据：commit 6eada1b7；测试 85/85
- [x] 单进程存档串行化先落地（499111be）：PlayerStore 以 storeMutex_ 为单一串行点，自动保存/RPC 受理/保存回调/迁移冻结四线原子交错，落盘 I/O 全在锁外；SaveQueue 按玩家分槽（playerId % 槽数）同玩家 FIFO 落盘、在途合并拒绝；PersistJournal 类内互斥、sink 锁外执行、append 返回段全局序；SaveTask 携带 journalSeq，landedSeqs_/adoptWatermarks_ 双水位令 drain 回放拒绝旧快照倒灌；freeze/adopt 先等排空再同步写回。
  - 证据：commit 499111be
- [x] 跨进程写入代次 database 半先落地：DbSaveRequest.writeGen 上 wire（0=无版本写端兼容受理）、PlayerData.writeGen 随档案持久化（重启即恢复地板）。
  - 证据：测试 10/10
- [x] 真实保存载荷接入 database，资料变更与去重凭据持久提交；区分传输、受理、执行和持久结果，覆盖重试、乱序、失败与重启。
  - 证据：commit 95f8d46a；测试 85/85
- [x] database 崩溃恢复契约已钉（E2E）：断链期保存如实失败（传输失败不假受理成功）、锚点真值保留；database 同存储同端口重启 + base 重启后载回最后一次已落盘真值，断链期失败写入不复活，复活后新写照常落盘。
- [x] 当前单进程口径先落地（d2574505）：exp 以 Anchor 为持久真源，activate/启动恢复/adopt 三路播种、flush/freeze 收集覆盖写回、adopt 以快照差值收敛；结算凭据 settlement_<party>.cred 持久提交，重启 ingest 回填、重开副本重放拒绝、未落账补齐、终局回收；build-off/build-examples 各 83/83 绿，含凭据 roundtrip/跨重启去重/挂账 6 例新增与迁移链播种不变量更新。
  - 证据：commit d2574505；测试 83/83

- [x] 恢复 baseServer 独立可执行入口，通过 PlayerStorage 注入远程 database，按需激活 PlayerAnchor；远程模式不访问本地档案与存储 journal，无存储不可达时的本地回退。
- [x] 新增 database 独立入口，复用现有 DB_LOAD/DB_SAVE 与 M1 信封，提供文件加载/保存、journal 先写/恢复、身份校验及写入完成回执；尚非完整存储分区与异步执行队列。
- [x] 保存队列按玩家 ID 去重，允许同线程槽的不同玩家排队；停机排完已受理任务，不因关闭 running 标志提前丢弃。
- [x] BaseDatabaseTests、BaseDatabaseProcessSmoke、BaseAnchorTests、PersistJournalTests、PersistenceChainTests、RecoveryTests、HostedServicesTests 共 7 个定向门禁通过；独立双进程分别重启、Base 无本地档案及不可达拒起已验收。
- [x] 拆分旧 PlayerData 的长期字段与模拟位置/运行字段，移除默认 WorldApp 的旧档案宿主及直接存储路径；独立原型不作为最终 Base/Cell 对象契约完成证据。

## P2：worldServer 管理与 cell 执行分离

- [x] 从当前 WorldApp 分离 worldServer 管理职责与 cell 模拟入口；模拟循环、WorldHost/Scene/Avatar、AOI、战斗与 Lua 迁入执行端，World 保留注册、拓扑、容量和迁移管理。
  - 证据：commit 763185b1；测试 83/83
- [x] 明确 Space 对应现有 Scene 的实现映射，分开空间 ID、Cell 分区 ID 与执行进程 ID；保留静态 SceneDescriptor 与 Instance 八态，不新增 Scene→Space 同义层。
- [x] 默认一个 cell 进程，每 Space 一个完整 Cell；同进程承载多个 Space，复用既有副本流程，验证隔离与临时资源回收。
  - 证据：测试 15/15
- [x] cell 向所属 World 注册地址、代次、版本、就绪、负载与排空状态；World 分配完整 Space/Cell，定义心跳失效、重注册、旧执行端隔离和控制面失联策略。
  - 证据：本次补齐认证凭据面（960 credential + World registerToken 逐字节匹配，拒伪造注册）与混编兼容范围白名单（compatWhitelist，声明投影在「本进程投影 ∪ 白名单」内受理）两缺口；拓扑版本/重平衡策略属下一行（World 管理拓扑、邻接与分配版本），不在本行范围。测试 89/89。
- [ ] World 管理拓扑、邻接与分配版本，单实体只有一个有效模拟代次；Instance 共享目标/时钟/结算明确唯一逻辑负责人。（BLOCKED）
- [x] 保留有界意图队列、模拟线程写入、过载拒收及停机尾扫；不得由 IO、管理端或后台线程直接改模拟状态。
  - 证据：测试 15/15
- [x] 受理单写者契约已钉（P2 分离前置回归底线）：有界队列顶格拒收不丢旧、两级水位滞回、过载拒收、tick 边界 drain、G-3 停机尾扫、维护/预启动闸六例既有验收；新增第七例——模拟线程外 submitIntent 只入队不写 world（提交即刻探测会话表无绑定），会话绑定仅在 gameThread tick 边界 drain 后显形（resolveIntentPlayer 提为公开只读观测面，互斥会话表跨线程安全）；code-verified：意图消费仅 gameLoop tick 边界与停机尾扫两处，M1 泵/回执/管理面（rollout/drain/debugEval/fleet 上报/metrics）全在 gameLoop 驱动。
- [x] 更新真实 CMake 入口、CLI、配置、脚本和开发编队，删除冲突宿主接线与别名；文档只标记真实存在的程序，不虚构目录。
  - 证据：测试 15/15

## P3：服务目录、Gateway 与登录

- [ ] Base/World 向 loginServer 注册逻辑身份、管理地址、代次、兼容与就绪；World 发布已提交执行位置投影，目录不另行分配 Cell。（BLOCKED）
- [ ] 玩家记录分开原区服、Base/归属版本、World/Space/Cell/执行进程/可选 Instance/模拟代次，以及 gateway/session/会话版本。（BLOCKED）
- [ ] 分开进程代次、资料写入代次、实体权威代次、拓扑版本和路由/会话版本；扩展 WorldAssignment/远程引用与真实 wire，不把现有字段当完整契约。（BLOCKED）
- [ ] gateway 基础业务接所属 Base，模拟输入接当前 cell，下行绑定真实会话；缓存失效、迟到位置投影、重定向和旧代次拒绝可恢复，正常消息不逐条经目录或 World 管理端代理。（BLOCKED）
- [x] 登录串联认证 → gateway 会话 → Base 经 database 激活 → World 准入/分配 → cell 创建实体；未进玩法也可保有 Base 对象。
  - 证据：测试 84/84
- [ ] 多 gateway 重连、重复登录、旧会话撤销与路由持久性接真实网络，恢复原 Base 与当前模拟位置，不创建第二个资料写入者。（BLOCKED）
- [x] 验证 World/cell 无同机 machined 独立注册与运行；machined 不知道各服资料位置，也不裁决跨服去向。
  - 证据：测试 13/13

## P4：副本结算、模拟迁移与跨服

- [ ] Base 校验小队、资格和版本，向 World 申请容量；cell 准入可信绑定各成员原 Base、快照、会话和有期限凭证。（BLOCKED）
- [ ] Instance 结果接 cell → 各原 Base → database，稳定玩法/结算 ID 校验模拟代次；多 Base 分别确认，定义部分成功、补偿、待重试和最终结束。（BLOCKED）
- [ ] 未持久确认保留唯一结算与恢复记录，覆盖 Base 暂不可达、断线、解散、超时和异常；失败取消不发成功奖励。（BLOCKED）
- [ ] 真实链路覆盖多 Space/Instance 隔离、多个执行进程选路及远端回收，不沿用 World 间完整档案搬迁作为场景迁移。（BLOCKED）
- [ ] 明确显式换 Space 与同 Space 跨 Cell 两条路径；现有 SceneTransfer 离场重建 Avatar 可用于换场原语，不能实现连续 Cell 交接。（BLOCKED）
- [ ] 迁移完整必要运行状态，保持实体 ID、血量、冷却、时钟和必要脚本状态；源冻结、目标未激活准备、条件提交、路由投影更新、目标激活与源清理逐步确认。（BLOCKED）
- [ ] 迁移决定持久可查询，跨 World 有唯一协调提交依据；结果不明先对账，隔离旧源，覆盖每步超时、源/目标及管理端重启，禁止双端解冻。（BLOCKED）
- [x] 跨服设计预留：原区服/Base/存储归属稳定，目的地持各成员原 Base 关联，不获取各服数据库凭证；持续 Space 与临时 Instance 均有边界。
- [ ] 跨服需求启用时，接两区服两 Base 同场、分组、容量、准入、逐 Base 幂等结算与安全返回；home_zone_id 不直接当业务区服 ID。（BLOCKED）

## P5：两种更新方案

- [ ] 协议更新验证真实新旧业务解码、状态转换、未升级服务/客户端兼容；握手 schema_hash 白名单不能代替兼容实现，存储结构演进另行验收。（BLOCKED）
- [x] 未知/未受理消息型受理面契约先钉（切片）：新增代次消息型（真实增量先例 9fd9690b squad 族 0x0072）与枚举外裸值到达未升级受理面 = 结构化 ERROR 回包（非崩溃、非断链），sessionId 保留、错误原因非空，且受理面不被污染——后续已知类型照常受理；可证边界：scripts/drill_proto_update.sh 仍为 schema_hash 指纹（仅换代次不做真实新旧解码兼容），真实字段演进兼容验收仍开放。
  - 证据：commit 9fd9690b
- [x] 真实新旧解码兼容面先钉（切片）：合法帧头下畸形 body（非 JSON）此前向泵线程抛 parse_error 炸进程，现结构化 ERROR 回包（会话号保留，dispatchRequest 补受理兜底）；旧形最小载荷（缺字段全默认）容忍解码为结构化响应；档案载荷未知未来字段忽略不污染、缺字段默认不炸。
- [x] cell 受理面畸形意图兜底先钉：残帧头/合法帧头+残 body 的意图此前在 gameLoop drain 抛异常炸进程，现 drain 逐意图兜底——坏意图丢弃（已出队不堵队列）、同拍与后续好意图照常执行、受理面不污染；坏意图无延时回执（请求方按超时收敛），回执纪律余量随 P2 分离面定。
- [x] 档案序列化转义与垃圾档兜底已修（L69 存储结构演进前置）：PlayerData toJson/fromJson 由裸字符串手写解析改为 nlohmann（真转义、类型化容忍、未知字段忽略、缺字段结构体默认）；此前含 "exp": 片段的用户名可污染字段解析甚至经 std::stof 崩溃读档方、垃圾档案静默载回全默认档——现特殊字符对称无损、垃圾档 loadPlayer 如实失败、journal 脏条目保留人工排查；nlohmann 依赖经 apollo_app_services PUBLIC + 直编测试目标自取守卫接线（三树重建过）。
- [ ] cell 更新：新旧并存、停止源新分配、tick 边界冻结、目标准备、World 提交权威/路由、按序恢复输入、源清理后退出，原 Base 与存储归属不变。（BLOCKED）
- [ ] 完整活动连续包含成员、实体、怪物、目标、计时、随机与必要脚本状态；只搬玩家、自然结束或强停不能作为连续迁移证据。（BLOCKED）
- [ ] Base 更新：排平存储、交接长期对象/会话/待办、提交写权代次、刷新 Base 路由、隔离旧写及清理；数据库归属不变。（BLOCKED）
- [ ] World 管理端更新：交接拓扑、注册代次、容量/排空、在途创建/迁移决定与提交记录；冻结冲突分配、隔离旧管理端并恢复执行端关联，不让双管理端分配同一 Space/Cell。（BLOCKED）
- [ ] 定义管理失联期间执行权限/租约与继续 tick 的期限，区分故障停止、重新接管与已有活动恢复，不能用单纯进程重启证明管理升级。（BLOCKED）
- [ ] 各更新阶段覆盖超时、提交不明、双方重启、不可逆转换及回滚边界，已生效请求不能跨代次盲目重放。（BLOCKED）
- [ ] 仅代码更新保持编号、字段与 wire；Lua 在执行边界处理模块私有状态、旧协程和回滚副作用，C++ 复用对应角色进程替换。（BLOCKED）
- [ ] gateway 持续持有 TCP；其自身替换分别验收自然排空、期限强停、免重登恢复与新连接持续接入；TCP 无断开需独立连接交接实现。（BLOCKED）
- [ ] 分别记录服务接入、免重登、TCP 连续及活动连续，使用真实客户端、多个 Base/World/cell 和失败回滚验证。（BLOCKED）

## P6：预留接口与最终验收

- [ ] 在当前单 Cell 契约中表达同 Space 多 Cell、全空间坐标、稳定实体 ID、分区范围/邻接/拓扑版本和实体权威代次，避免假定 Space 永远等于一个进程。（BLOCKED）
- [ ] 预留 Real/Ghost 与观察者视图、邻接复制的版本/失效、可见集连续/去重、跨 Cell 请求裁决/顺序/幂等与故障对账边界；Ghost 无模拟和持久写权，迁移快照不当作 Ghost。（BLOCKED）
- [ ] 将契约、真实网络、持久化、故障重启、排空、两种更新演练重接最终拓扑；资料、管理与模拟恢复分别验证。（BLOCKED）
- [ ] 清理冲突配置、脚本、死命令和注释，完成适当构建与回归后按证据更新文档，不以旧档案搬迁日志验收新模型。（BLOCKED）

## 后续能力：需求触发后实施

- [ ] 实际无缝地图：空间切分/动态负载、邻接 Ghost、连续可见性、跨边界战斗和故障接管；基于上述同一体系扩展，当前不实现算法。（BLOCKED）
- [ ] 外部 MySQL/Redis/ClickHouse 后端；database 职责先独立，不以产品选择为前提。（BLOCKED）
- [ ] protobuf/zstd 真实消息路径；生成产物不代表运行接线完成。（BLOCKED）
- [ ] 专用 Profiler，验证采样开销与启停；当前未集成。（BLOCKED）
- [ ] OpenTelemetry SDK/OTLP exporter，验证上下文、采样、背压和降级；当前未集成。（BLOCKED）
- [ ] Kafka/Prometheus/Grafana 部署及具体玩法战斗验证，按实际需求实施。（BLOCKED）

## 审核发现

- [x] 审核发现：world --role=cell 委托路径重复打印 Configuration 日志块（main.cpp:286 与 cell_main.cpp:214 各一次），且两处 Role 描述矛盾（"shared binary, L27" vs "cellapp process boundary"），旧内联路径只打印一次。
  - 证据：commit 0f8bba7a；测试 11/11
- [x] 审核发现：world --role=cell 委托路径对 manager 专用 flag（--assign-spaces/--world-journal/--cell-expiry-ms/--max-instances/--base-business-*/--settlement-credential-dir/--service-register-*）由旧「接受并忽略」变为 load_cell_config 未知参数 exit(2)（cell_main.cpp:164-167），与头注释「语义与旧 world --role=cell 完全一致」及 todo 批㊰「零破坏」声明不符。
  - 证据：commit 0f8bba7a；测试 11/11
- [x] 审核发现：--m1-authority-epoch 只被 load_cell_config（cell_main.cpp:131）识别、main.cpp loadConfig 不识别（main.cpp:227-232 未知参数守卫），故 world --role=cell 委托路径无法使用该 flag（先被 main 的 loadConfig 拒），与「与 cellServer 同源/单一装配面」宣称不符。
  - 证据：测试 11/11
- [x] 审核发现：cellServer 独立入口不校验 --role 值——load_cell_config 只放行不解析（cell_main.cpp:134-141），注释称「值校验仍由 caller 负责」但 cellServer 路径（cell_server_main → run_cell_entry → run_cell_entry_body）全程无 parse_world_role，`cellServer --role=worker` 会静默按 cell 启动。
  - 证据：测试 11/11
- [x] 审核发现：docs/zh/design/world-cell-management.md:19 与 en 版同段自相矛盾——同段声明「不带 Server/-server/-app 后缀」「没有保留另一套可用别名」，紧接着却引入带 Server 后缀的 cellServer 并保留 world --role=cell 兼容入口。
  - 证据：测试 11/11
- [x] 审核发现：docs/zh/design/world-cell-management.md:7 与 :77（en:7）仍称「管理和执行共用 world 二进制，用启动角色区分」，与 §1.1（:19）新称执行端为独立 cellServer 二进制冲突。
  - 证据：测试 11/11
- [x] 审核发现：todo.md 新增批㊽（:129）与既有批㊽（:135）批号重复。
  - 证据：测试 11/11
- [x] 审核发现：docs/zh/design/world-cell-management.md:79 刚把 §4.1 启动形式从「这是待实现的 CLI」改为「已实现」，但示例 :83-84 仍用 `--config=<...>`——该 flag 在 `world`（main.cpp:73-237）与 `cellServer`（cell_main.cpp:77-180）的解析面均无分支，实测 `world --config=foo` 与 `world --role=cell --config=foo` 都 exit=2 报 `unknown arg --config`，示例与实现不一致（英文 :79,83-84 同）。
  - 证据：测试 11/11
- [x] 审核发现：docs/zh/design/world-cell-management.md:9、:186、:215、:221 仍以「共享/共用/同一二进制」描述 World/cell 两角色（:221「共用二进制两个角色真进程启动」为验收项），与本次 §1（:7）和 §4.1 改为「执行端默认独立 cellServer」不一致（英文 :9,186,215,221 同）。
  - 证据：测试 11/11
- [x] 审核发现：删除 `world --role=cell` 委托后，`world` 仍把 `--register-host/--register-port/--cell-id` 标成 manager 参数并继续接受这些 cell 专用 flag，`--link-grace-ms` 也没有 manager 消费方，按 help 配置会被静默忽略（apps/world/src/main.cpp:200）。
  - 证据：测试 10/10
- [x] 审核发现：英文文档仍称 `world --role=cell` 保留兼容委托（第 7、9、19、26、30 行），与已删除委托的实现及中文同文档相反（docs/zh/en/design/world-cell-management.md:9）。
  - 证据：测试 10/10
- [x] 审核发现：§4.1 仍写 `world` 的 `--role` 接受 `manager|cell`，但实现已显式拒绝 `cell`，中英文档均与入口契约不符（docs/zh/design/world-cell-management.md:88）。
  - 证据：测试 10/10
- [x] 审核发现：解析器注释仍承诺重复 `--role` 后到覆盖，但实现遇到先出现的 `cell` 会立即失败，且测试删除了 `--role=cell --role=manager` 的覆盖场景（apps/world/include/world_app/role_options.hpp:13）。
- [x] 审核发现：`--role` 缺值错误仍提示 `(manager|cell)`，而 `cell` 已被拒绝，错误信息会误导用户传入必失败的取值（apps/world/include/world_app/role_options.hpp:26）。
- [x] 审核发现：`world --role=cell` 委托已删除（role_options.hpp:33-35 显式拒绝、main.cpp:67-70 exit 2），但 docs/zh/design/bigworld-apollo-mapping.md:13、:85 与 docs/zh/en/design/bigworld-apollo-mapping.md:13、:85 仍称「world --role=cell 委托同一执行端运行体 / delegates to the same executor body」，与实现不符——本批只同步了 world-cell-management.md，漏改此档（docs/zh/design/bigworld-apollo-mapping.md:13）。
- [x] 审核发现：同上——docs/zh/Directory_Structure.md:34 与 docs/zh/en/Directory_Structure.md:34 仍写「world --role=cell 兼容委托同一执行端运行体 / delegates to the same executor body」，且把 cell 专用 flag `--link-grace-ms`（及 register 三件套）列在 `world` 行——world 已删解析拒收（main.cpp 移除四 flag），与入口契约不符（docs/zh/Directory_Structure.md:34）。
- [x] 审核发现：同上——docs/zh/architecture/adr.md:59 与 docs/zh/en/architecture/adr.md:59 仍写「增加已讨论的 cell 执行职责（world 二进制 `--role=cell`）」，而 world 已不再提供 cell 角色（cellServer 独立入口承担），ADR 与实现不符（docs/zh/architecture/adr.md:59）。
- [x] 审核发现：同上——docs/public/architecture/system-zh.svg:13、:58、system-zh-mobile.svg:10、:82、system-en.svg:13、:58、system-en-mobile.svg:10、:82 仍以「--role=cell」标注 cell 执行入口已落地，实际执行端为 cellServer 独立入口（docs/public/architecture/system-zh.svg:13）。
- [x] 审核发现：cell_app.hpp L108 新块（:149-152）定义「执行进程身份 = 本进程注册到所属 World 的逻辑 cell ID（config.cellId）」，相邻 L99 块（:163-164）仍定义「执行进程身份 = 本进程（worldId；注册后由 worldServer 分配）」——同文件两处对「执行进程身份」给出不同定义；且两处均称 World「注册后分配」身份，实际为执行端 `--cell-id` 自报、World 仅回显（world_app.cpp:560 sendCellRegisterReply 传 request.cell_id；cell_register_client.cpp:335 自比 config_.cell_id），World 不分配（apps/world/include/world_app/cell_app.hpp:163）。
  - 证据：测试 15/15
- [x] 审核发现：partition_id_of_space（cell_app.hpp:154-156）经 resolve_scene_id 对未注册 space 静默回落默认 scene 的 partition（cell_app.cpp:1219-1224 回落 defaultSceneId_），新注释块未注明该回落——与 hosts_space（:189-191）「回落默认语义不适合作持有判据」的既有告诫相反，观测面把「未注册空间」与「默认分区」混同；测试仅覆盖已注册 space 7，未覆盖未知空间边界（apps/world/include/world_app/cell_app.hpp:154）。
  - 证据：测试 15/15
- [x] 审核发现：tests/test_cell_app.cpp:1157-1158 新增断言 `partition_id_of_space(999)==default_scene_id()` 在未调用 `cell.start()` 时求值，`ensureDefaultScene()` 未执行、`defaultSceneId_` 保持 0，断言退化为 0==0 恒真，未实际验证「未知空间回落默认分区」语义（apps/world/include/world_app/cell_app.hpp:146）。
  - 证据：测试 15/15
- [x] 审核发现：tests/world_cell_independent_smoke.py:147-148 machined 断言仅检查已捕获的三进程输出（cell_out 在 start() 返回后不再增量读取），无法证明 machined 未启动/未参与——若 machined 以独立进程运行其输出不在任何缓冲中，弱于文档声称的「machined 不启动、不参与」（tests/world_cell_independent_smoke.py:147）。
- [x] 审核发现：docs/zh/Directory_Structure.md:34（及 docs/zh/en/Directory_Structure.md:34）把 `--add-space` 仅归 cellServer 执行端入口，但 apps/world/src/main.cpp:150 显示 world manager 入口同样解析并生效 `--add-space`（经内嵌 CellApp 的 config.spaces，cell_app.cpp:154 启动窗创建），实现与文档不符（docs/zh/Directory_Structure.md:34）。
- [x] 审核发现：tests/world_cell_independent_smoke.py:35-38 `free_port()` 串行取空闲端口后立即关闭 socket，子进程 bind 前存在 TOCTOU 窗口，端口可能被抢占致测试偶发失败；且 CMake 未设 RUN_SERIAL（apps/world/CMakeLists.txt:140），并行 ctest 下端口冲突风险更高（tests/world_cell_independent_smoke.py:35）。
  - 证据：测试 3/3
- [x] 审核发现：`--service-register-port` 帮助文本自相矛盾——同串写「0 = kernel assigned」与「default: 0 = off, not assembled」，而代码在 0 时仍装配（内核分配），「off」表述已不成立（apps/login/src/main.cpp:120）。
  - 证据：测试 3/3
- [x] 审核发现：login_server.hpp 成员注释仍写「serviceRegisterPort = 0 = 不装配」，与新行为（0 = 内核分配并始终装配）矛盾，config.hpp:49-53 已改而此处未同步（apps/login/include/login/login_server.hpp:208）。
  - 证据：测试 3/3
- [x] 审核发现：login 服务注册受理面改为无条件装配（删除 `if (serviceRegisterPort != 0)` 守卫），service-register 面从此无法显式关闭，所有 login 实例（含未传该 flag 的既有测试/部署）都在随机端口打开受理面；路由表无效时每次启动均打「service register degraded」降级警告，属缺省行为回归（apps/login/src/login_server.cpp:417）。
  - 证据：测试 3/3
- [x] 审核发现：test_id_domains_are_separate 所在块注释仍称「不启内核——观测面在构造后即可读（start 前注册窗合法）」，但测试现调用 cell.start()/stop() 启动内核，注释与实现不符（tests/test_cell_app.cpp:1133）。
  - 证据：测试 15/15
- [x] 审核发现：test_id_domains_are_separate 在 cell.start() 后从测试线程读取 default_scene_id()/partition_id_of_space()（defaultSceneId_ 非原子、spaceToScene_ 映射，注释声明为内核线程上下文/单写者纪律访问器），且 40ms sleep 并非断言所需（ensureDefaultScene 在 start() 内同步执行）；跨线程读依赖游戏线程此刻不写该表，仅偶然成立，属线程契约脆弱点（tests/test_cell_app.cpp:1160）。
  - 证据：测试 15/15
- [x] 审核发现：dev_fleet 默认启用 interfaces 与 S08 决策一致，但未落实决策「立即执行动作」所列「验证监听、合法配置与端口占用报错」——8080 被占时仅 machined 日志显形，编队 up 不报端口冲突（scripts/dev_fleet.sh:62）。

- [x] 审核发现：英文镜像未随中文「已代决」同步——`docs/zh/en/design/world-reconciliation.md:51,67-70` 仍写「open decision in §6」「Open (implementation parameters, decided after review)」「advisory (warn only) or mandatory boot refusal」，`docs/zh/en/design/world-cell-management.md:195` 仍写「Parameters pending review/measurement: journal durability, heartbeat/grace intervals...」，而本区间改的中文版（`docs/zh/design/world-reconciliation.md:51,67-71`、`docs/zh/design/world-cell-management.md:195`）已定为 S04/S05/S06「已代决（强制拒启/受控人工确认/A 行 fsync、R/B 行 flush）」，违反 todo.md L32「zh/en 双语 51 文件同步」约定（docs/zh/en/design/world-reconciliation.md:67）。
  - 证据：commit 24a40754
- [x] 审核发现：新动作设计稿的 todo.md 行号引用已过期——`docs/review/2026-10-30-actions/07-credentials-capacity.md:3`（D02→todo.md:113 实为 115、S02→113/138）、`08-conflict-lock-report-artifacts.md:3`（S04/S06/S07→126/133/134、S09→150 实为约 148）、`09-user-input-questionnaires.md:3`（D18→168 实为 202、D33→196 实为 260、D38→201 实为 275）——这些行号在 e521f6f5 及 4d6f2f82 时即不对应所引决策条目（todo.md:113 为批㉝非 D02）（docs/review/2026-10-30-actions/09-user-input-questionnaires.md:3）。
  - 证据：commit 24a40754；测试 102/102
- [x] 审核发现：`00-index.md` 汇总计数漏列 D22——状态总表把 D22 标为「✅设计；⏳依赖 L154/L155」（docs/review/2026-10-30-actions/00-index.md:33），⏳ 行也列 D22 为「设计本身已完成」（:67），但「✅ 设计稿/事实完成：...（42 项）」枚举（:64）不含 D22（计 42 实为 43，或应与 ⏳ 口径一致）（docs/review/2026-10-30-actions/00-index.md:64）。
  - 证据：commit 24a40754
- [x] 审核发现：S08 状态与同区间实现矛盾——`docs/review/2026-10-30-decisions.md` S08 执行注「监听与端口占用验证由 dev_fleet 属主会话进行中」（:815）与 `00-index.md:57`「验证/监听由 dev_fleet 属主会话补」，但本区间 `318efb64` 已在 `scripts/dev_fleet.sh:73-90` 落地 `check_interfaces_port_free` 且 todo.md:313 记录「已核实并修复+手工验证」（docs/review/2026-10-30-decisions.md:815）。
  - 证据：commit 24a40754
- [x] 审核发现：`scripts/dev_fleet.sh:84` 端口预检误报——`ss -H -ltn "sport = :$port"` 不按本地地址过滤，host=127.0.0.1 时若 10.0.0.5:8080（或其他特定非环回地址）被占会误报「已被占用」并阻断 `up`，实际绑定 127.0.0.1:8080 可成功；与 :71 注释「无歧义判占用」不符（过度保守，属误报）（scripts/dev_fleet.sh:84）。
  - 证据：commit 24a40754；测试 102/102
- [x] 审核发现：`scripts/dev_fleet.sh:95` 按本地地址过滤的 awk 分支未处理 `ss` 对 IPv6 dual-stack 监听（`[::]:<port>`，Linux 默认 bindv6only=0）显示的 `*:<port>` 通配地址——`*` 不匹配 127.0.0.1/0.0.0.0/::1 任一分支，预检误判「空闲」；但 `[::]:port` dual-stack 实际占用 IPv4 与 IPv6 地址空间（实测 bind 127.0.0.1:port 与 0.0.0.0:port 均 EADDRINUSE），导致端口冲突漏报，违背「up 前显式报冲突」目标（interfaces 进程启动才报错，非预检拦截）。
  - 证据：commit 2a8baace；测试 102/102
- [x] 审核发现：`docs/review/2026-10-30-actions/09-user-input-questionnaires.md:4` D34 的 todo.md 行号引用仍为 197，但 todo.md:197 是「立即动作设计稿已落...」（已代决附属行）而非 D34 原条目「外部 MySQL/Redis/ClickHouse 后端...」（实际在 todo.md:263），与同文件 D18→202/D33→260/D38→275 均指原条目口径不一致；本 diff 声称「已核实并修复」行号引用但遗漏 D34。
  - 证据：commit 2a8baace；测试 102/102
- [x] 审核发现：`ArchitectureDiagram.vue:15` system 窄屏渲染回退——`narrowSource` 对 system 复用 1280×1915 全尺寸图（`systemSource`），而非原先专用的 480×1440 `system-{lang}-mobile.svg`；窄屏（容器 max-width:520px）下正文 15px 缩至约 5.6px、名称 23px 缩至约 8.6px，文字近乎不可读，属移动端可读性回退（docs/.vitepress/theme/components/ArchitectureDiagram.vue:15）。
- [x] 审核发现：`architecture-{lang}.svg:4` 箭头 marker 用 `fill="context-stroke"`，MDN BCD 显示 Safari 不支持该值（version_added=false），Safari 下箭头回退为默认黑色而非沿用边线颜色，与 Chrome 124+/Firefox 111+ 渲染不一致（docs/public/images/architecture-en.svg:4）。
- [x] 审核发现：`ArchitectureDiagram.vue:15` 将 system 图源切至 `/images/architecture-{lang}.svg` 后，旧的 `docs/public/architecture/system-{en,zh}.svg` 及 `system-{en,zh}-mobile.svg` 仍被 git 跟踪但组件不再引用（仅 todo.md:301 历史文字提及），成为死资产，建议清理或确认废弃（docs/.vitepress/theme/components/ArchitectureDiagram.vue:15）。
- [x] 审核发现：本轮 diff 未见问题（4bd4185e..HEAD）。
  - 证据：commit 4bd4185e；测试 501/502

- [x] 审核发现：`scripts/docs/render_architecture.py:283` 从生成循环移除 `system` 后，`system()` 函数（行 57）成为无调用死代码；且本 diff 新增/编辑的 `docs/public/images/architecture-{en,zh}[,-mobile].svg`（含窄屏移动版）不在 `--check` 校验的 12 份 `architecture/{space,cross-server,callbacks}*.svg` 之列、无再生成源，架构图今后可静默漂移（scripts/docs/render_architecture.py:283）。
  - 证据：commit acffdda4；测试 16/16
- [x] 审核发现：测试目标改名（a21826ea，`baseappmgr_tests`→`online_directory_tests`、`baseappmgr_m1_control_tests`→`online_directory_m1_control_tests`）后遗留两处旧目标名注释——`CMakeLists.txt:438`「session_world_tests / base_anchor_tests / baseappmgr_tests 全部静默失联」与 `tests/test_baseappmgr_m1_control.cpp:82`「baseappmgr_tests 同例」均引用已不存在的目标名（现为 `online_directory_tests`），与「仅目标名无行为变化」同步口径不符（CMakeLists.txt:438）。
  - 证据：commit acffdda4
- [x] 审核发现：新增源模板 scripts/docs/templates/architecture-zh-mobile.svg 的正文（body/label）文本仍为英文（如 L11 "Login · Input · State"、L38 "Player profiles & base gameplay"、L44 "Register · Topology · Capacity · Migrate"、L50 "Scene · Avatar · AOI · Battle · Lua" 等），仅节点名与图例已中文化，与已全量中文化的宽屏 architecture-zh.svg 不一致，中文窄屏图呈现英文正文（scripts/docs/templates/architecture-zh-mobile.svg:11）。
- [x] 审核发现：scripts/docs/render_architecture.py:237 非 `--check` 生成分支只对 OUT（docs/public/architecture）做 `mkdir`，未对新目标 IMG（docs/public/images）做 `mkdir`；若 docs/public/images 目录缺失，写入 system 图将抛 FileNotFoundError 而非自动重建，与 OUT 处理不对称（scripts/docs/render_architecture.py:237）。
  - 证据：测试 16/16
- [x] 审核发现：本轮 diff 未见问题（178839d8..HEAD）。
  - 证据：commit 410591bc
- [x] 审核发现：WorldApp::running_ 为普通 bool（world_app.hpp:332），start/stop 在管理线程写、managerThread_ 泵线程在 while(running_)（world_app.cpp:257）读，hostSimulation=false 时存在跨线程非同步数据竞争（未用 std::atomic，与 CellApp::running_ 用 std::atomic 不一致）（apps/world/include/world_app/world_app.hpp:332）。
  - 证据：commit ed979476；测试 83/83
- [x] 审核发现：onIntent 在 cell_==nullptr（生产 hostSimulation=false）时静默丢弃意图（world_app.cpp:205），不回 2001/2101 即时回执，违反 ADR-017「顶格拒收/不受理即时回」契约，且与 onM1DebugEval 对无模拟宿主显式回 "simulation not hosted" 处理不一致；网关投递意图将无响应（apps/world/src/world_app.cpp:205）。
  - 证据：commit ed979476；测试 83/83

- [x] 审核发现：切片2 production world（cell_==nullptr）收到 932 意图时被同时转发给就绪 cell 又回 2001 "simulation not hosted"——`M1LinkServerImpl::handle_envelope_frame` 无条件先后调 `on_intent_raw_` 与 `on_intent_`，而 WorldApp 注册的 `on_intent_` 在 cell_==nullptr 分支恒回 `kIntentAckInternal`(2001)；故就绪 cell 存在时意图既被转发执行、网关又立刻收到「不受理」即时回执，且 cell 稍后经 onCellIntentAck 再回真实 929——同一 request_id 双回执且首回执语义与转发事实矛盾。
  - 证据：commit a235a57d
- [x] 审核发现：`firstReadyCellPeer()` 只认 `ready && drain_state==0` 的登记 cell，切片2 用它寻址 952/956 转发——cell 自报排空（drain_state=1）或冻结（ready=false）后，fleetctl 的 956 status / 重复 begin（更紧期限）与 952 status 查询经转发即找不到合格目标，回「squad service not ready / script runtime not ready」而非当前排空/编排状态，与内嵌形态（直接读自身状态）行为分叉。
  - 证据：commit a235a57d
- [x] 审核发现：在途转发关联账无过期清扫——`evalReplyPeers_`/`rolloutReplyPeers_`/`drainReplyPeers_` 条目只在 cell 回执时擦除，cell 断连/崩溃不回执则永留（onPeerLinkDown 只置 suspect 不清账）；eval 账被 `kM1EvalBacklogMax`(64) 封顶后残留条目永久占额，cell 死亡携在途 eval 后所有后续 eval 恒回「eval backlog full」，rollout/drain 账则无界增长。
  - 证据：commit a235a57d
- [x] 审核发现：cell 执行端模块编排无观察窗自驱——cell_main.cpp 的 post_tick 只做 eval 收账与 drain 推进，缺 WorldApp 内嵌 `driveModuleRollout` 等价面（无 `call_module(name,"onTick")`、无 `observe_target`/`pending_promote`/`pending_rollback` 推进），prepare→commit 后模块停于 `kObserving` 永不晋级 `kPromoted`，canary 故障计数（window_faults）永不累积。
  - 证据：commit a235a57d
- [x] 审核发现：`--tick-rate 0` 使启动日志 `1000 / config.tickRateMs` 整数除零（SIGFPE）崩溃——load_cell_config 对 tickRateMs 无 0/负值校验。
  - 证据：commit a235a57d
- [x] 审核发现：`--world-id` 用 `std::stoul`（可抛 std::invalid_argument/out_of_range）且 `load_cell_config` 在 `run_cell_entry_body` 的 try 块外调用——非数字输入抛未捕获异常经 `std::terminate` 崩溃，而非走 catch 的「Error:」显式路径；同函数其余数值参数（--cell-id/--link-grace-ms 等）用 strtoull 无此问题，解析面不一致。
  - 证据：commit a235a57d
- [x] 审核发现：cell 侧 956 begin 的 deadline 只在 `cell_drain_state==0` 登记，重复 begin（已排空更紧期限）不收紧——与内嵌 WorldApp `onM1WorldDrain`「重复 begin 幂等，期限只紧不松」语义分叉。
  - 证据：commit a235a57d
- [x] 审核发现：`cell_eval_peers`（cell_main.cpp:289）与 `cell_last_rollout_name`（cell_main.cpp:288,523）为死变量——前者在 debug_eval 监听捕获后从未读写，后者 prepare 赋值后无任何消费（cell 侧无内嵌 `lastRolloutName_` 提供的 promoted/rolled-back 落证日志面）。
  - 证据：commit a235a57d
- [x] 审核发现：cell 执行端模块编排 op 3（rollback/abort）在非 kCanaryPending 分支无条件回执 ok=true（apps/world/src/cell_main.cpp:623），即使 rollback_target 未达 kRolledBack（如在 kObserving 期被调、state 停在 kObserving/kRollingBack）仍报成功——与原 WorldApp onM1ModuleRollout「ok = state == kRolledBack」判定分叉，fleetctl/admin 会误判回滚完成。
- [x] 审核发现：cell 执行端 cell_last_rollout_name 在 prepare 成功即登记（apps/world/src/cell_main.cpp:579），而 post_tick 无条件按它 call_module(name,"onTick")（apps/world/src/cell_main.cpp:383-385）——commit 前（kCanaryPending）模块尚未换入目标 runtime，call_module 落「module not a table」fault 入 module_fault_log_，污染 canary 观察窗/熔断器，可致误判坏提前回滚；rollback/abort 后该名也未清空，onTick 持续打在已回滚模块上。
- [x] 审核发现：README.zh.md 新增 docs 徽章 href 为 `https://cuihairu.github.io/apollo/zh//`，但 VitePress `rewrites` 把 `zh/:path*` 重写到根 `:path*`、构建产物无 `zh/` 目录——中文文档实际部署于 `https://cuihairu.github.io/apollo/`，`/apollo/zh/` 为 404（另含尾双斜杠），与中文文档位置错配（README.zh.md:4）。
- [x] 审核发现：README.md 新增 docs 徽章 href 为 `https://cuihairu.github.io/apollo//`（尾双斜杠，浏览器规整为 `/apollo/`），指向中文根文档而非英文文档——英文 README 的 docs 徽章应链接英文文档 `https://cuihairu.github.io/apollo/en/`（VitePress 英文 locale 挂载 `/en/`），语言错配且含双斜杠（README.md:4）。
- [x] 审核发现：本轮 `cell_main.cpp` 的 rollout ok-flag 与驱动名登记修复没有生产 `cellServer` 路径测试，`WorldAppM1LinkTests`/`UpdateContinuityTests` 实际驱动 `tests/cell_host.hpp` 平行实现，`WorldCellIndependentSmoke` 也未覆盖 module rollout（todo.md:347）。
  - 证据：测试 84/84
- [x] 审核发现：README.md docs 徽章 href 由 `https://cuihairu.github.io/apollo/en/`（49e385ff 时正确，英文 locale 实际部署位置）改为 `https://cuihairu.github.io/apollo/`——VitePress root locale=简体中文（`link '/'`）、英文 locale 挂载 `/en/`，英文 README 的 docs 徽章现指向中文根文档而非英文文档，语言错配回归（README.md:12）。
  - 证据：commit 9b8a125d
- [x] 审核发现：README.zh.md docs 徽章 href 改为 `https://cuihairu.github.io/apollo/zh/`，但 VitePress `rewrites: 'zh/:path*': ':path*'` 把 zh/ 前缀重写到根、构建产物无 `zh/` 目录，该 URL 为 404；改前 `/apollo/` 为中文文档实际部署位置，与中文文档位置错配（README.zh.md:12）。
  - 证据：commit 9b8a125d
- [x] 审核发现：覆盖范围 49e385ff0793..HEAD（4 文件）已全审——tests/world_cell_independent_smoke.py 新增 `RolloutM1Client` 逐项与实现核对（crc32c 反射 0x82F63B78 测试向量 0xE3069283、帧线形 16B 头+载荷+4B 尾、Hello 握手 0x4C494E4B+u64 route_version+u64 epoch+u32 名长+名、952/953 字段序/类型含 Bool=1 字节、RolloutState 枚举 2/3/6/7、world 952→953 转发链 world_app.cpp:1129-1198、提交日志串 cell_main.cpp:600、TIMEOUT 40/RUN_SERIAL 接线），均与实现一致未见问题；README 两条 docs 徽章回归（8b1d9d8d/08df2b5c 引入，与上一轮 todo.md:347-350 修对的 href 相反，按仓库 config.mts 的 rewrites/locales 判定）已在 9b8a125d 修复。
  - 证据：commit 9b8a125d

## 内部状态 Dashboard(用户令 2026-11-02)

- [x] DASH-1 [P1] 内部状态 dashboard:除 Prometheus 指标外,提供人看的内部状态页。

- [x] DASH-1 阶段1（responder 与装配，docs/design/dashboard.md §8 阶段1）已落地：`DashboardResponder`（apps/world/include/world_app/dashboard.hpp + src/dashboard.cpp）——MetricsHttp 同型泵驱动（accept/read/write 全在 pump 内、宿主泵驱动、零内部线程、连接占用超时、超上限拒收、只 accept 不拨号、只读无状态变更端点）；路由 `GET /` 200 text/html（内嵌单页骨架）+ `GET /api/state` 200 application/json（状态回调产出，缺省空对象）+ 其余 404；token 面（`--dashboard-token`，未设放行/设置后全路由要求 `Authorization: Bearer <t>` 否则 401，不用查询串）；装配于 world/cellServer（`--dashboard-host`/`--dashboard-port`/`--dashboard-token`，缺省 0 = 关闭不占端口，默认绑 127.0.0.1）。
  - 证据：测试 3/3
- [x] DASH-1 阶段2（数据源接线，docs/design/dashboard.md §8 阶段2）已落地：三件只读查询面补齐——① `ILinkTransport::isTls()`（记录层标记，TLS 客户端/服务端 override true）+ `InterServerLink::isTls()` 代理 + `LinkManager::LinkSnapshot`（平台无关纯数据）+ `LinkManager::snapshot_links()`（全部端点逐链路快照：peer/state/tls/tx/rx/queued_bytes）+ `M1LinkServer::snapshot_links()` 虚代理 + `CellRegisterClient::snapshot_links()` 虚代理；② `ScriptRuntime::active_modules()`（既有接口——装载表 name→version 直接复用，不重复造面）；③ dev_fleet readiness：machined `Supervisor` 加 `run_dir` 落 `.run/<name>.pid`（fork 成功写、死亡删、目录缺失创建），`dev_fleet.sh` up 传 `--run-dir`，dashboard 读 fleet.roster/pid/log 尾判定 ready（就绪日志行 = "Server is running."/"ready port="/"listening on"，与 start.sh 口径同源）。
  - 证据：测试 4/4

- [x] DASH-1 阶段3（页面与文档，docs/design/dashboard.md §8 阶段3）已落地：内嵌单页 HTML 已含实时刷新 JS（`setInterval(refresh, 2000)` 2s 轮询）与日志尾渲染（`f.log_tail` join 进 `pre`）；`docs/design/dashboard.md` 状态翻「已实现」；Directory_Structure/apps-index 双语登记（world/ 行补 `--dashboard-host/--dashboard-port/--dashboard-token` 只读 HTTP 默认关，zh+en）；e2e `tests/dashboard_e2e_smoke.py`（真 world 进程 `--m1-port 0 --dashboard-port <空闲实端口>` 起；GET `/` 200 text/html 含 `Apollo 状态`+`setInterval`+`log_tail`、`/api/state` 200 application/json 六键 process/links/orders/fleet/script/load 且 role=world、`/nope` 404、token 面无 Authorization 401+正确 Bearer 200）。
- [x] 审核发现：3d/3e 生产 cellServer module rollout 测试未真正验证「驱动名 commit 成功才登记」的 prepare 期负边界——`RolloutM1Client` 的 promote 断言（`r["state"]==6`，tests/world_cell_independent_smoke.py:368-374）与 committed 日志行（tests/world_cell_independent_smoke.py:388）只正向证明 commit 成功与观察窗健康，却无「kCanaryPending 期驱动名未登记」的负向证据：若 `cell_last_rollout_name` 回归为 prepare 即登记（原 bug 形态），kCanaryPending 期 post_tick 会 `call_module(name,"onTick")` 落「module not a table」fault，但该 fault 记于 tick < begin_tick（begin_canaries_ 在 commit 才定 begin_tick，module_rollout.cpp:170-174），被 `module_faults_since` 的 `t>=since_tick` 排除（script_runtime.cpp:928-929）——观察窗仍零 fault 照常 promote，测试在修复回退后依旧全绿，未守住该回归（tests/world_cell_independent_smoke.py:347-420；begin_canaries_ 定 begin_tick 于 module_rollout.cpp:161-174）。
- [x] 审核发现：3d 注释声称「健康窗零 fault（回执 faults=0）直接证明」（tests/world_cell_independent_smoke.py:384），但 promote 轮询只断 `r["state"]==6`（tests/world_cell_independent_smoke.py:368-374），从未断言 `r["faults"]==0`——window_faults 仅由 kPromoted 终态隐式蕴含（ModuleRollout::evaluate_ 全窗健康才 promote，module_rollout.cpp:176-204），显式回执断言缺失，注释与代码不符。
- [x] 审核发现：覆盖范围 49e385ff0793..HEAD（4 文件）已全审并独立核对——README.md:12 与 README.zh.md:12 两条 docs 徽章回归已修（08df2b5c 改坏 zh 徽章 `/apollo/`→`/apollo/zh/` 线上 404；8b1d9d8d 改坏 en 徽章 `/apollo/en/`→`/apollo/` 语言错；已回改正确目标并提交）；`tests/world_cell_independent_smoke.py` 新增 `RolloutM1Client` 协议逐项与实现核对无问题（crc32c 反射 0x82F63B78/检查向量 0xE3069283、帧线形 16B 头+载荷+4B 尾、Hello 子型 0+kHelloProto 0x4C494E4B+u64 route_version+u64 epoch+u32 名长、952 八字段/953 七字段序与类型含 ok=Bool 1B、RolloutState 枚举 2/3/6/7、world 952→953 转发链 world_app.cpp:1130-1198、committed 日志串 cell_main.cpp:600-601、kPromoted 下 op3 拒判定 cell_main.cpp:619-627），协议面无问题
  - 证据：commit 8b1d9d8d
- [x] 审核发现：`scripts/ci/docs_i18n_check.py` 不校验 README 徽章 href（无 README/badge/href 匹配），本次两条徽章回归无 CI 守门——补 docs_i18n_check 的 README 徽章 href 校验（en→`/apollo/en/`、zh→`/apollo/`）。

- [x] 审核发现：dashboard.md:37 数据源清单把 link/TLS 状态写成 `LinkState{kIdle,kConnecting,kUp,kDown}`，但实际枚举第二值为 `kHandshaking`（modules/net/link/include/apollo/net/link/interserver_link.hpp:55-59）；`kConnecting` 只存在于另一枚举 `ConnectState`（modules/net/link/include/apollo/net/link/tcp_transport.hpp:69），LinkState 无此值——后续按此文档实现 `snapshot_links()` 会把握手态映射成不存在的状态名（docs/design/dashboard.md:37）。
- [x] 审核发现：dashboard.md:27 安全模型允许 `?token=<t>` 查询串携带 token——查询串 token 会进浏览器历史/HTTP 访问日志/Referer，与本设计自述「状态页比 Prometheus 指标更敏感、默认只绑环回」的防泄漏姿态相悖；建议仅 `Authorization: Bearer`，或显式记录查询串 token 的残留泄漏风险（docs/design/dashboard.md:27）。
- [x] 审核发现：dashboard.md:61 与 :77 不一致——§5 `/api/state` JSON 形状 `orders.pending_detail` 未给条数上限，§6 页面却写「前 20 条 pending」；作为脚本化查询面，orders 积压大时 `/api/state` 响应无界，建议在 API 形状同样界定 pending_detail 条数（docs/design/dashboard.md:61,77）。
- [x] 审核发现：覆盖范围 686417c015d5080b8c139ce251eb48c68ca97fdc..HEAD（4 文件，仅文档）已全审——README.md:12/README.zh.md:12 docs 徽章已修对（en→`/apollo/en/`、zh→`/apollo/`，与 docs/.vitepress/config.mts:53-56,122-124 的 root=zh 挂 `/`、en 挂 `/en/` 一致，线上无错配）；docs/design/dashboard.md 其余 file:line 引用逐项核对属实（CMakeLists.txt:22、metrics_http.hpp:42-49、dev_fleet.sh:91、link_manager.hpp:17-18,129,138、tls_client.hpp:72-83、order_store.cpp:82,102,113、module_version_store.hpp:26-28、module_rollout.hpp:42、metric_registry.h:143,146,168）；todo.md 本轮记录的两条 3d/3e 测试缺口结论与实现核对属实（promote 轮询仅断 state==6 未见 faults==0——tests/world_cell_independent_smoke.py:368-374；begin_tick 在 commit 后才由 begin_canaries_ 的 c.begin() 定——module_canary.cpp:15、module_rollout.cpp:170-174；module_faults_since 按 t>=since_tick 过滤——script_runtime.cpp:928-929）。
- [x] 审核发现 CR-04 [P2]：login 硬编码明文测试用户无条件装载（apps/login/src/login_server.cpp:25-43,65——构造 :22 调 initTestUsers，player1/pass1、player2/pass2，authenticate 明文比较；字段名 passwordHash 实持明文）。
  - 证据：commit 4e494ae9
- [x] 审核发现 CR-05 [P2]：--tick-rate 用 std::atoi 无校验（apps/world/src/main.cpp:174-175），"0"/"abc"→0 时 :283 `1000/config.tickRateMs` 整除零 SIGFPE（本地启动崩溃）。
  - 证据：commit fde662ad
- [x] 审核发现 CR-06 [P2]：event_loop 持锁回调（modules/net/http/src/event_loop.cpp:325-333,541,637——processTimers/onCheckTimer/onReconnectTimer 持 mutex 触发 timer->fire()/heartbeatCallback/callback()），回调若回调本组件（addTimer/registerHeartbeat/…）即自锁死锁。
  - 证据：commit 545ba298
- [x] 审核发现 CR-07 [P3]：net_rpc session_manager::getSessionByPlayerId 持 mutex_ 调 getSession（二次上锁）自锁死锁（modules/net/rpc/src/session_manager.cpp:191-200）；模块为遗留脚手架，live apps 零引用（rpc_legacy.cpp 同名面）——死代码地雷。
  - 证据：commit 05aa72aa
- [x] 审核发现 CR-09 [P3]（低置信）：broadcast/SetAttribute 持锁调监听器回调，回调重入风险（modules/net/rpc/src/session_manager.cpp:338-352；modules/game/attributes/src/attribute.cpp:78）。
  - 证据：commit 05aa72aa
- [x] 审核发现 CR-10 [P3]（低置信）：module_rollout 回滚遍历 prepared_ 全集（modules/script/src/module_rollout.cpp:317）与 script_runtime 版本清单读改写非原子（script_runtime.cpp:646），需随 rollout/版本面深审。
  - 证据：测试 12/12
- [x] 审核发现：新增 PasswordHash 单测 test_password_hash 落在 test_crypto.cpp，但 crypto_tests 目标被注释禁用（tests/CMakeLists.txt:413-419）且 test_crypto.cpp 未被任何活跃构建目标引用。
  - 证据：commit f38e867c
- [x] 审核发现：--dashboard-port 在 world/cellServer/gateway 三处均用 std::atoi 无校验。
  - 证据：commit d87fff99
- [x] 审核发现：world --tick-rate 校验上限为 INT_MAX，与 cellServer 的 1..10000 不一致。
  - 证据：commit fde662ad
- [x] 审核发现：DashboardResponder::read_and_maybe_answer 对 conn.recv 无大小上限。
  - 证据：commit eb1615c3
- [x] 审核发现：DashboardResponder::route 的 "GET /api/state" 前缀匹配无路径边界。
  - 证据：commit 1fadfa97
- [x] 审核发现：GatewayServer::dashboardJson 的 drain_detail 直接拼接进 JSON 未转义。
  - 证据：commit 6b955675
- [x] 审核发现：CR-04/CR-05 修复已落地但 todo 未勾账。
  - 证据：commit 4e494ae9
- [x] 审核发现（低置信）：cellServer pre_tick 读 link（M1LinkServer unique_ptr，apps/world/src/cell_main.cpp:365-368）与主线程 link->start() 写 manager_（apps/world/src/cell_main.cpp:479,783）无同步——内核线程 pre_tick 与主线程 start 并发窗口存在非原子数据竞争（UB）风险（if(link) 仅护 null，不护半初始化）。
  - 证据：commit ddf14375
- [x] 审核发现：broadcast() 持锁拷贝裸 `Session*`、锁外逐个 `send`——锁外 send 期间并发 `closeSession`/`shutdown`/`update` 会经 `internalCloseSession` 擦除 `sessions_`（销毁 unique_ptr→Session 析构→connection_->close），悬垂指针 use-after-free（modules/net/rpc/src/session_manager.cpp:355；CR-09 修复 05aa72aa 以 UAF 换掉自锁死锁，模块为遗留脚手架、live apps 零引用）。
  - 证据：commit 05aa72aa；测试 3/3
- [x] 审核发现：超限请求路径 `aborted_` 双计——`read_and_maybe_answer` 内 `++aborted_`（:246）且 `pump()` 对同一 `return false` 再 `++aborted_`（:198）；读错误路径只计 1，计数语义不一致（apps/world/src/dashboard.cpp:246；回归 `test_recv_cap` 仅断 `>=1` 未覆盖双计）。
- [x] 审核发现：m1_link_server 的 manager_ 加锁不完整——`manager_ = std::make_unique<LinkManager>(mc)`（:263）与 `if (!manager_)`（:276）在 `managerMutex_` 外，且 sendEvalResult/sendModuleRolloutReply/sendIntentAck/gateway_link/endpoint_port 等约 20 处访问 manager_ 均无锁；ddf14375 声称「护 start/stop/pump/snapshot 读写」未全覆盖，pre_tick 读与主线程 start 写在 :263/:276 仍可能竞态（apps/world/src/m1_link_server.cpp:263）。
  - 证据：commit ddf14375
- [x] 审核发现：`ReconnectManager::triggerReconnect` 仍持 mutex_ 调 `callback()`（:632）——回调重入 removeReconnect/addReconnect 即自锁死锁；CR-06 只修 onReconnectTimer，同型持锁回调在 triggerReconnect 残留（modules/net/http/src/event_loop.cpp:632；与 CR-06 宣称「出锁触发」不符，pre-existing 未在 diff）。
  - 证据：测试 3/3
- [x] 审核发现：`SessionManager::onDisconnected` 持 mutex_（:277）内调 `getSession`（:278，二次上锁非递归 mutex）自锁死锁——与 CR-07 修 getSessionByPlayerId 同型残留，「已修复二次上锁」宣称未闭环（modules/net/rpc/src/session_manager.cpp:278；pre-existing 未在 diff）。
  - 证据：测试 3/3
- [x] 审核发现：test_event_loop_reentrant.cpp 用 POSIX `::pipe`/`::close`（:48,:64）无 `_WIN32` 守卫，而 CMake 目标无条件加入（tests/CMakeLists.txt:1272，WIN32 仅链 ws2_32）——Windows 构建编译失败（tests/test_event_loop_reentrant.cpp:48）。
  - 证据：测试 3/3
- [x] 审核发现：test_event_loop_reentrant.cpp make_read_pair 只归还读端，对端从不关闭——POSIX 管道写端 fd[1]（:44-45）与 Windows 连接端 socket b（:34,40）泄漏；原实现两端口都 close（`::close(pipefd[0]);::close(pipefd[1])`），重构后只关读端（:103,105），资源泄漏（tests/test_event_loop_reentrant.cpp:44-45）。
  - 证据：commit 5b8c2b98；测试 3/3
- [x] 审核发现：m1_link_server 原子 shared_ptr 只保活 manager_ 指针、未保活 `mgr->link(peer)` 返回的 InterServerLink* 的并发悬垂断言。
  - 证据：commit 5b8c2b98
- [x] 审核发现（低置信）：broadcast 锁外 send 与并发 closeSession 的 `Session::close` 对 `info_.state` 非原子读写（send 读 :54-55 / close 写 :89）——shared_ptr 修复防 UAF，但「close 后 send 见 !isConnected 安全返回」依赖非原子成员，数据竞争 UB；模块为遗留脚手架、live apps 零引用（modules/net/rpc/src/session_manager.cpp:54-55,89）。
  - 证据：commit 5b8c2b98；测试 3/3
- [x] 审核发现：`info_.state` 在构造后再无写入，`markConnected`/`setPlayerId`/`close` 只改原子 `state_`，`getInfo().state` 因此永远停留在 `Connecting`，接口快照与真实状态不一致（modules/net/rpc/src/session_manager.cpp:43）。
  - 证据：测试 5/5
- [x] 审核发现：`Session::setPlayerId` 对 `state_` 采用先 load 后 store 的非原子组合，并发 `close` 可在两步之间置为 `Disconnected`，随后仍被写回 `Authenticated` 并触发 `onAuthenticated`，已关会话状态被“复活”（include/apollo/net/session_manager.h:144）。
  - 证据：测试 5/5
- [x] 审核发现：`setPlayerId_no_revive_after_close` 仅串行先 close 后 set，旧实现同样通过，未覆盖注释声称的并发 close 竞态窗口（tests/test_rpc_session_manager.cpp:145）。
  - 证据：测试 6/6
- [x] 审核发现：依赖迁移未删除 package.json 中已被 pnpm 忽略的 `pnpm.overrides`，与 pnpm-workspace.yaml 配置重复且易漂移（docs/package.json:25）。
- [x] CI 的 pnpm 9 只读 package.json 的 overrides，删除该配置后冻结安装因 overrides 与锁文件不匹配而失败。**已修复**：overrides 回到 docs/package.json 单一来源并删除 docs/pnpm-workspace.yaml 双源，锁文件 overrides（vite 6.4.4/katex 0.18.2）核对一致（docs/package.json:20-25）。
- [x] 并发测试把 close 返回后的认证回调一律判为失败，但 CAS 可在 close 前成功而回调被调度延迟至 close 后，正确实现也会随机误报。**已修复**：判据改为最终状态（close 恒为最后写入者，会话必须停在 Disconnected），删除回调时序判据与 AuthAfterCloseRecorder（tests/test_rpc_session_manager.cpp:190-236）。
- [x] 并发测试只检查 close 返回后的标志而不检查最终状态，旧实现在标志设置前的复活窗口会漏报。**已修复**：并入上一条——最终状态断言下，旧实现两步写回复活必然停在 Authenticated 而失败（tests/test_rpc_session_manager.cpp:228-233）。
- [ ] Windows 下 EventLoopReentrantTests SEGFAULT（CI 41907458774 首次 Windows Test 步实证）：socketpair 模拟 fd 进 select 事件循环崩溃，根因待 WinSock 侧定位；已按 Windows 空转口径跳过该用例，POSIX 全覆盖不变（tests/test_event_loop_reentrant.cpp:142）。
