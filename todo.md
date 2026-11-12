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
- [ ] World 管理拓扑、邻接与分配版本，单实体只有一个有效模拟代次；Instance 共享目标/时钟/结算明确唯一逻辑负责人。（BLOCKED复核 2026-11-06：D03 已代决=一个 Space 一个完整 Cell、World 独占分配、稳定分配无自动迁移、每 Instance 一个逻辑负责人；首阶段已落——spaceAssignments_ 每 Space 单调 assignment_version + journal A/R/B 四例版本规则（创建 v1/释放版本不变/重认领 +1/重启 B 行铸代次）与 Instance 单逻辑负责人（SquadInstanceService 按 world_id 绑定），测试 CellRegisterTests+SquadInstanceTests 绿、build-off 89/89；但 D03「明确不做/延后」=同 Space 多 Cell、邻接算法、自动动态重平衡、单实体独立权威代次（本轮未消费，仅 Space 级版本）——条件仍缺，不重试。）
- [x] 保留有界意图队列、模拟线程写入、过载拒收及停机尾扫；不得由 IO、管理端或后台线程直接改模拟状态。
  - 证据：测试 15/15
- [x] 受理单写者契约已钉（P2 分离前置回归底线）：有界队列顶格拒收不丢旧、两级水位滞回、过载拒收、tick 边界 drain、G-3 停机尾扫、维护/预启动闸六例既有验收；新增第七例——模拟线程外 submitIntent 只入队不写 world（提交即刻探测会话表无绑定），会话绑定仅在 gameThread tick 边界 drain 后显形（resolveIntentPlayer 提为公开只读观测面，互斥会话表跨线程安全）；code-verified：意图消费仅 gameLoop tick 边界与停机尾扫两处，M1 泵/回执/管理面（rollout/drain/debugEval/fleet 上报/metrics）全在 gameLoop 驱动。
- [x] 更新真实 CMake 入口、CLI、配置、脚本和开发编队，删除冲突宿主接线与别名；文档只标记真实存在的程序，不虚构目录。
  - 证据：测试 15/15

## P3：服务目录、Gateway 与登录

- [x] Base/World 向 loginServer 注册逻辑身份、管理地址、代次、兼容与就绪；World 发布已提交执行位置投影，目录不另行分配 Cell。（2026-11-13 L115 位置投影首切片：契约 968 space_position_projection 六字段（sdks/contract/messages.xml + 生成五件套同步，test_contract 计数 92→93）；目录受理 = PositionProjectionRegistry 水位裁决（position_projection_registry.hpp:53——重复幂等/乱序拒/墓碑命中水位抹除/非法拒），968 裸帧受理随注册面装配（m1_service_register_server.cpp handle_projection_frame）；World 发布 = mirror-first 客户端（m1_projection_client.cpp——publish 先记镜像，kUp 跃迁全量重放补投；Windows stub 面关），发布点挂 journal A/R 行后（world_space_assign.cpp:122/:186）、journal 装载 seed 随首拍（world_cell_registry.cpp driveCellRegistry）；宿主装配 world/main.cpp（serviceRegisterPort!=0 起，失败降级告警不阻服务）；目录不分配 Cell 仅投影登记（复合键 world+space，无分配语义）。测试 tests/test_world_position_projection.cpp 8 用例（registry 裁决 6 + 真链路生命周期/kUp 重放 2）；主树 ctest **127/127**（原 126+1）；san 树目标套件 6/6（投影/注册族/Link/ContractHandlers ASan+UBSan）；off-tree2（IPC=OFF）/examples 全量重建 exit 0。commit d07da72d。）
  - 挂账（本批新发现）：san 树全量 4 个进程 smoke（database/login/world 起步即崩）同签名 `crash_capture.cpp:114 runtime error: member call ... invalid vptr`——fresh san 树首次跑 app 级 smoke 揭出的前置既有 UB（database 二进制不含本批任何代码即触发），待专批修。
  - BLOCKED复核 2026-11-06：login 服务注册受理面（ServiceRegistry + M1 964）与 Base/World 拨号（base/main.cpp:107、world/main.cpp:366）已落，但 World 只登记自报管理地址、未发布已提交执行位置投影（world_app.cpp 无投影发布面），目录不另行分配 Cell 部分仅靠注释口径——仍缺位置投影发布，不重试。（本项 blockers 已随 L115 切片解除）
- [ ] 玩家记录分开原区服、Base/归属版本、World/Space/Cell/执行进程/可选 Instance/模拟代次，以及 gateway/session/会话版本。（BLOCKED复核 2026-11-06：PlayerData 已拆长期字段 + PlayerRuntimeState（x/y/z 位置），WorldAssignment 仍仅 world_id/map_id/instance_id/space_id/route_version 五字段，缺 cell/执行进程/模拟代次/session 版本与真实 wire；home_zone_id 未按业务区服语义接线——字段与 wire 仍缺，不重试。）
- [ ] 分开进程代次、资料写入代次、实体权威代次、拓扑版本和路由/会话版本；扩展 WorldAssignment/远程引用与真实 wire，不把现有字段当完整契约。（BLOCKED复核 2026-11-06：WorldAssignment（world_assignment.hpp）仅五字段值类型，无进程/资料写入/实体权威/拓扑/路由/会话版本域；P5 协议更新批未到，真实 wire 未演进——版本域与端到端往返仍缺，不重试。）
- [ ] gateway 基础业务接所属 Base，模拟输入接当前 cell，下行绑定真实会话；缓存失效、迟到位置投影、重定向和旧代次拒绝可恢复，正常消息不逐条经目录或 World 管理端代理。（BLOCKED复核 2026-11-06：gateway MessageRouter 有 M1 egress 接 Base 前递（gateway_server.cpp:249 forwardToBaseApp）与 fetchRouteSnapshot，但 M1 单 cell 端点注释明言「路由快照仅存档，选择面随多 cell 批回归」，模拟输入接当前 cell/下行绑定真实会话/位置投影重定向仍依赖 cell 真实接线与投影发布——仍缺，不重试。）
- [x] 登录串联认证 → gateway 会话 → Base 经 database 激活 → World 准入/分配 → cell 创建实体；未进玩法也可保有 Base 对象。
  - 证据：测试 84/84
- [ ] 多 gateway 重连、重复登录、旧会话撤销与路由持久性接真实网络，恢复原 Base 与当前模拟位置，不创建第二个资料写入者。（BLOCKED复核 2026-11-06：缺真实网络/多 gateway/多客户端演练——D10 用例矩阵已落 docs/review/2026-10-30-actions/04 §3，真实网络验收待 P4；TCP 会话保持与唯一资料写者裁决未接线——仍缺真实链路，不重试。）
- [x] 验证 World/cell 无同机 machined 独立注册与运行；machined 不知道各服资料位置，也不裁决跨服去向。
  - 证据：测试 13/13

## P4：副本结算、模拟迁移与跨服

- [ ] Base 校验小队、资格和版本，向 World 申请容量；cell 准入可信绑定各成员原 Base、快照、会话和有期限凭证。（BLOCKED复核 2026-11-06：缺真实多 Base/多 World 链路与认证凭据——D11 依赖 L118 gateway 接 Base/cell 与认证凭据面（均未完成）；跨服设计稿暂不启用，单区服真实入场链未接线——仍缺，不重试。）
- [ ] Instance 结果接 cell → 各原 Base → database，稳定玩法/结算 ID 校验模拟代次；多 Base 分别确认，定义部分成功、补偿、待重试和最终结束。（BLOCKED复核 2026-11-06：缺 cell→各原 Base→database 真实结算链路——D12 逐 Base 幂等结算依赖 L118/L127 真实链路；squad_instance settlement_credential_store 仅存储面，无多 Base 分别确认/补偿/待重试编排——仍缺，不重试。）
- [ ] 未持久确认保留唯一结算与恢复记录，覆盖 Base 暂不可达、断线、解散、超时和异常；失败取消不发成功奖励。（BLOCKED复核 2026-11-06：缺结算恢复记录与异常结束持久化——D13 依赖 L128 结算链路；未持久确认的唯一结算/恢复记录与失败取消不发奖逻辑未接线——仍缺，不重试。）
- [ ] 真实链路覆盖多 Space/Instance 隔离、多个执行进程选路及远端回收，不沿用 World 间完整档案搬迁作为场景迁移。（BLOCKED复核 2026-11-06：缺多 Space/Instance 隔离、多执行进程选路及远端回收真实链路——D14 依赖 L113 拓扑批与 L118 网关路由；当前无跨进程选路/远端回收，不沿用 World 间档案搬迁作为场景迁移——仍缺，不重试。）
- [ ] 明确显式换 Space 与同 Space 跨 Cell 两条路径；现有 SceneTransfer 离场重建 Avatar 可用于换场原语，不能实现连续 Cell 交接。（BLOCKED复核 2026-11-06：显式换 Space 路径可复用 scene_transfer 离场重建原语（scene_transfer.hpp），但同 Space 跨 Cell 连续交接为 D15「明确不做/延后」（触发条件=明确跨边界玩法需求），L139 拓扑/邻接未实现——连续交接路径仍缺，不重试。）
- [ ] 迁移完整必要运行状态，保持实体 ID、血量、冷却、时钟和必要脚本状态；源冻结、目标未激活准备、条件提交、路由投影更新、目标激活与源清理逐步确认。（BLOCKED复核 2026-11-06：缺迁移状态机接线——D16 依赖 L131 两路径与 L113 拓扑批（均未完成）；实体 ID/血量/冷却/时钟/脚本状态的源冻结→准备→提交→激活→清理未实现——仍缺，不重试。）
- [ ] 迁移决定持久可查询，跨 World 有唯一协调提交依据；结果不明先对账，隔离旧源，覆盖每步超时、源/目标及管理端重启，禁止双端解冻。（BLOCKED复核 2026-11-06：缺跨 World 唯一迁移决定协调——D17 依赖跨服暂不启用 + L132 迁移链路；持久提交记录/对账/隔离旧源/防双端解冻未实现——仍缺，不重试。）
- [x] 跨服设计预留：原区服/Base/存储归属稳定，目的地持各成员原 Base 关联，不获取各服数据库凭证；持续 Space 与临时 Instance 均有边界。
- [ ] 跨服需求启用时，接两区服两 Base 同场、分组、容量、准入、逐 Base 幂等结算与安全返回；home_zone_id 不直接当业务区服 ID。（BLOCKED复核 2026-11-06：跨服为需求触发——D18 需目标玩法/成员原区服/匹配/容量/奖励归属/安全返回六项用户答案（问卷已落 docs/review/2026-10-30-actions/09 §1），未启用；home_zone_id 未按业务区服语义接线——条件仍缺，不重试。）

## P5：两种更新方案

- [ ] 协议更新验证真实新旧业务解码、状态转换、未升级服务/客户端兼容；握手 schema_hash 白名单不能代替兼容实现，存储结构演进另行验收。（BLOCKED复核 2026-11-06：缺 P5 协议更新批——D19 需真实新旧业务解码/状态转换/未升级服务客户端兼容样本；当前仅 compat 投影裁决与 route_version 握手，schema_hash 白名单不能代替兼容实现，存储结构演进另验收——仍缺，不重试。）
- [x] 未知/未受理消息型受理面契约先钉（切片）：新增代次消息型（真实增量先例 9fd9690b squad 族 0x0072）与枚举外裸值到达未升级受理面 = 结构化 ERROR 回包（非崩溃、非断链），sessionId 保留、错误原因非空，且受理面不被污染——后续已知类型照常受理；可证边界：scripts/drill_proto_update.sh 仍为 schema_hash 指纹（仅换代次不做真实新旧解码兼容），真实字段演进兼容验收仍开放。
  - 证据：commit 9fd9690b
- [x] 真实新旧解码兼容面先钉（切片）：合法帧头下畸形 body（非 JSON）此前向泵线程抛 parse_error 炸进程，现结构化 ERROR 回包（会话号保留，dispatchRequest 补受理兜底）；旧形最小载荷（缺字段全默认）容忍解码为结构化响应；档案载荷未知未来字段忽略不污染、缺字段默认不炸。
- [x] cell 受理面畸形意图兜底先钉：残帧头/合法帧头+残 body 的意图此前在 gameLoop drain 抛异常炸进程，现 drain 逐意图兜底——坏意图丢弃（已出队不堵队列）、同拍与后续好意图照常执行、受理面不污染；坏意图无延时回执（请求方按超时收敛），回执纪律余量随 P2 分离面定。
- [x] 档案序列化转义与垃圾档兜底已修（L69 存储结构演进前置）：PlayerData toJson/fromJson 由裸字符串手写解析改为 nlohmann（真转义、类型化容忍、未知字段忽略、缺字段结构体默认）；此前含 "exp": 片段的用户名可污染字段解析甚至经 std::stof 崩溃读档方、垃圾档案静默载回全默认档——现特殊字符对称无损、垃圾档 loadPlayer 如实失败、journal 脏条目保留人工排查；nlohmann 依赖经 apollo_app_services PUBLIC + 直编测试目标自取守卫接线（三树重建过）。
- [ ] cell 更新：新旧并存、停止源新分配、tick 边界冻结、目标准备、World 提交权威/路由、按序恢复输入、源清理后退出，原 Base 与存储归属不变。（BLOCKED复核 2026-11-06：缺多进程 cell 更新演练——D20 依赖 L113 拓扑批与 L104 管理/执行分离；自然排空演练清单已落 docs/review/2026-10-30-actions/06 §2，但新旧并存/停止新分配/tick 冻结/World 提交权威/按序恢复输入未接线——仍缺，不重试。）
- [ ] 完整活动连续包含成员、实体、怪物、目标、计时、随机与必要脚本状态；只搬玩家、自然结束或强停不能作为连续迁移证据。（BLOCKED复核 2026-11-06：缺完整活动连续迁移——D21 依赖 L132 迁移状态机；成员/怪物/目标/计时/随机/脚本状态未验证，只搬玩家/自然结束/强停不能作连续证据——仍缺，不重试。）
- [ ] Base 更新：排平存储、交接长期对象/会话/待办、提交写权代次、刷新 Base 路由、隔离旧写及清理；数据库归属不变。（BLOCKED复核 2026-11-06：缺 Base 更新演练——D22 依赖 L145 与更新链路；排平存储/交接长期对象/提交写权代次/隔离旧写未接线——仍缺，不重试。）
- [ ] World 管理端更新：交接拓扑、注册代次、容量/排空、在途创建/迁移决定与提交记录；冻结冲突分配、隔离旧管理端并恢复执行端关联，不让双管理端分配同一 Space/Cell。（BLOCKED复核 2026-11-06：缺 World 管理端更新演练——D23 依赖拓扑/管理端更新链路；交接拓扑/注册代次/冻结冲突分配/防双管理端未实现——仍缺，不重试。）
- [ ] 定义管理失联期间执行权限/租约与继续 tick 的期限，区分故障停止、重新接管与已有活动恢复，不能用单纯进程重启证明管理升级。（BLOCKED复核 2026-11-06：缺管理失联执行租约设计——D24 需执行权限/租约与继续 tick 期限定义，区分故障停止/重新接管/已有活动恢复；当前无租约机制——仍缺，不重试。）
- [ ] 各更新阶段覆盖超时、提交不明、双方重启、不可逆转换及回滚边界，已生效请求不能跨代次盲目重放。（BLOCKED复核 2026-11-06：缺更新超时/回滚边界——D25 需覆盖超时/提交不明/双方重启/不可逆转换/回滚；已生效请求跨代次重放防护未实现——仍缺，不重试。）
- [ ] 仅代码更新保持编号、字段与 wire；Lua 在执行边界处理模块私有状态、旧协程和回滚副作用，C++ 复用对应角色进程替换。（BLOCKED复核 2026-11-06：缺仅代码更新与 Lua 状态——D26 需保持编号/字段/wire，Lua 执行边界处理模块私有状态/旧协程/回滚副作用；当前未验证——仍缺，不重试。）
- [ ] gateway 持续持有 TCP；其自身替换分别验收自然排空、期限强停、免重登恢复与新连接持续接入；TCP 无断开需独立连接交接实现。（BLOCKED复核 2026-11-06：缺 gateway TCP 连续更新——D27 需自然排空/期限强停/免重登恢复/新连接持续接入；TCP 无断开需独立连接交接实现，当前无连接交接——仍缺，不重试。）
- [ ] 分别记录服务接入、免重登、TCP 连续及活动连续，使用真实客户端、多个 Base/World/cell 和失败回滚验证。（BLOCKED复核 2026-11-06：缺四类连续性分别验收——D28 需服务接入/免重登/TCP 连续/活动连续，用真实客户端+多 Base/World/cell+失败回滚验证；当前未执行——仍缺，不重试。）

## P6：预留接口与最终验收

- [ ] 在当前单 Cell 契约中表达同 Space 多 Cell、全空间坐标、稳定实体 ID、分区范围/邻接/拓扑版本和实体权威代次，避免假定 Space 永远等于一个进程。（BLOCKED复核 2026-11-06：D29 需在单 Cell 契约中表达同 Space 多 Cell/全空间坐标/稳定实体 ID/分区邻接/拓扑版本/实体权威代次；当前契约未扩展，仍假定 Space=一个进程——仍缺，不重试。）
- [ ] 预留 Real/Ghost 与观察者视图、邻接复制的版本/失效、可见集连续/去重、跨 Cell 请求裁决/顺序/幂等与故障对账边界；Ghost 无模拟和持久写权，迁移快照不当作 Ghost。（BLOCKED复核 2026-11-06：D30 Real/Ghost 与观察者边界未实现——modules/net/remoteentity entity_ref 仅为引用面，邻接复制/可见集连续/跨 Cell 请求裁决/故障对账未实现；Ghost 无模拟和持久写权，迁移快照不当作 Ghost——仍缺，不重试。）
- [ ] 将契约、真实网络、持久化、故障重启、排空、两种更新演练重接最终拓扑；资料、管理与模拟恢复分别验证。（BLOCKED复核 2026-11-06：D31 最终拓扑真实验收未执行——契约/真实网络/持久化/故障重启/排空/两种更新演练未重接最终拓扑——仍缺，不重试。）
- [ ] 清理冲突配置、脚本、死命令和注释，完成适当构建与回归后按证据更新文档，不以旧档案搬迁日志验收新模型。（BLOCKED复核 2026-11-06：D32 冲突配置/脚本/死命令/注释清理未执行——docs/review/2026-10-30-actions/08 §4 候选清单已核对为现行目标输出，无残留需清；但按证据更新文档与最终回归未做——仍缺，不重试。）

## 后续能力：需求触发后实施

- [ ] 实际无缝地图：空间切分/动态负载、邻接 Ghost、连续可见性、跨边界战斗和故障接管；基于上述同一体系扩展，当前不实现算法。（BLOCKED复核 2026-11-06：D33 无缝地图为需求触发——空间切分/动态负载/邻接 Ghost/连续可见性/跨边界战斗/故障接管算法未实现，当前不实现——仍缺，不重试。）
- [ ] 外部 MySQL/Redis/ClickHouse 后端；database 职责先独立，不以产品选择为前提。（BLOCKED复核 2026-11-06：D34 外部 MySQL/Redis/ClickHouse 为需求触发——database 职责先独立，产品选型未定——仍缺，不重试。）
- [ ] protobuf/zstd 真实消息路径；生成产物不代表运行接线完成。（BLOCKED复核 2026-11-06：D35 protobuf/zstd 真实消息路径未接线——modules/net 仅有 message_codec/协议头，生成产物不代表运行接线完成——仍缺，不重试。）
- [ ] 专用 Profiler，验证采样开销与启停；当前未集成。（BLOCKED复核 2026-11-06：D36 专用 Profiler 未集成——当前无 Profiler，采样开销/启停验证待需求触发——仍缺，不重试。）
- [ ] OpenTelemetry SDK/OTLP exporter，验证上下文、采样、背压和降级；当前未集成。（BLOCKED复核 2026-11-06：D37 OpenTelemetry SDK/OTLP 未集成——当前无 OTel，上下文/采样/背压/降级验证待触发——仍缺，不重试。）
- [ ] Kafka/Prometheus/Grafana 部署及具体玩法战斗验证，按实际需求实施。（BLOCKED复核 2026-11-06：D38 Kafka/Prometheus/Grafana 为需求触发——部署与玩法战斗验证待外部需求，未启用——仍缺，不重试。）

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
  - 证据：commit 70e20cbf；WorldRoleTests 1/1
- [x] 审核发现：`--role` 缺值错误仍提示 `(manager|cell)`，而 `cell` 已被拒绝，错误信息会误导用户传入必失败的取值（apps/world/include/world_app/role_options.hpp:26）。
  - 证据：commit 70e20cbf；apps/world/include/world_app/role_options.hpp:26；WorldRoleTests 1/1
- [x] 审核发现：`world --role=cell` 委托已删除（role_options.hpp:33-35 显式拒绝、main.cpp:67-70 exit 2），但 docs/zh/design/bigworld-apollo-mapping.md:13、:85 与 docs/zh/en/design/bigworld-apollo-mapping.md:13、:85 仍称「world --role=cell 委托同一执行端运行体 / delegates to the same executor body」，与实现不符——本批只同步了 world-cell-management.md，漏改此档（docs/zh/design/bigworld-apollo-mapping.md:13）。
  - 证据：commit 328f17e9；docs/zh/design/bigworld-apollo-mapping.md:13
- [x] 审核发现：同上——docs/zh/Directory_Structure.md:34 与 docs/zh/en/Directory_Structure.md:34 仍写「world --role=cell 兼容委托同一执行端运行体 / delegates to the same executor body」，且把 cell 专用 flag `--link-grace-ms`（及 register 三件套）列在 `world` 行——world 已删解析拒收（main.cpp 移除四 flag），与入口契约不符（docs/zh/Directory_Structure.md:34）。
  - 证据：commits 70e20cbf,328f17e9；docs/zh/Directory_Structure.md:34
- [x] 审核发现：同上——docs/zh/architecture/adr.md:59 与 docs/zh/en/architecture/adr.md:59 仍写「增加已讨论的 cell 执行职责（world 二进制 `--role=cell`）」，而 world 已不再提供 cell 角色（cellServer 独立入口承担），ADR 与实现不符（docs/zh/architecture/adr.md:59）。
  - 证据：commit 328f17e9；docs/zh/architecture/adr.md:59
- [x] 审核发现：同上——docs/public/architecture/system-zh.svg:13、:58、system-zh-mobile.svg:10、:82、system-en.svg:13、:58、system-en-mobile.svg:10、:82 仍以「--role=cell」标注 cell 执行入口已落地，实际执行端为 cellServer 独立入口（docs/public/architecture/system-zh.svg:13）。
  - 证据：commit 328f17e9；现图迁移至 docs/public/images/architecture-{zh,en}[,-mobile].svg 且标注 cellServer
- [x] 审核发现：cell_app.hpp L108 新块（:149-152）定义「执行进程身份 = 本进程注册到所属 World 的逻辑 cell ID（config.cellId）」，相邻 L99 块（:163-164）仍定义「执行进程身份 = 本进程（worldId；注册后由 worldServer 分配）」——同文件两处对「执行进程身份」给出不同定义；且两处均称 World「注册后分配」身份，实际为执行端 `--cell-id` 自报、World 仅回显（world_app.cpp:560 sendCellRegisterReply 传 request.cell_id；cell_register_client.cpp:335 自比 config_.cell_id），World 不分配（apps/world/include/world_app/cell_app.hpp:163）。
  - 证据：测试 15/15
- [x] 审核发现：partition_id_of_space（cell_app.hpp:154-156）经 resolve_scene_id 对未注册 space 静默回落默认 scene 的 partition（cell_app.cpp:1219-1224 回落 defaultSceneId_），新注释块未注明该回落——与 hosts_space（:189-191）「回落默认语义不适合作持有判据」的既有告诫相反，观测面把「未注册空间」与「默认分区」混同；测试仅覆盖已注册 space 7，未覆盖未知空间边界（apps/world/include/world_app/cell_app.hpp:154）。
  - 证据：测试 15/15
- [x] 审核发现：tests/test_cell_app.cpp:1157-1158 新增断言 `partition_id_of_space(999)==default_scene_id()` 在未调用 `cell.start()` 时求值，`ensureDefaultScene()` 未执行、`defaultSceneId_` 保持 0，断言退化为 0==0 恒真，未实际验证「未知空间回落默认分区」语义（apps/world/include/world_app/cell_app.hpp:146）。
  - 证据：测试 15/15
- [x] 审核发现：tests/world_cell_independent_smoke.py:147-148 machined 断言仅检查已捕获的三进程输出（cell_out 在 start() 返回后不再增量读取），无法证明 machined 未启动/未参与——若 machined 以独立进程运行其输出不在任何缓冲中，弱于文档声称的「machined 不启动、不参与」（tests/world_cell_independent_smoke.py:147）。
  - 证据：commit e26c9869；tests/world_cell_independent_smoke.py:142-151,195,462-475；WorldCellIndependentSmoke 1/1
- [x] 审核发现：docs/zh/Directory_Structure.md:34（及 docs/zh/en/Directory_Structure.md:34）把 `--add-space` 仅归 cellServer 执行端入口，但 apps/world/src/main.cpp:150 显示 world manager 入口同样解析并生效 `--add-space`（经内嵌 CellApp 的 config.spaces，cell_app.cpp:154 启动窗创建），实现与文档不符（docs/zh/Directory_Structure.md:34）。
  - 证据：commit 9de9058a；docs/zh/Directory_Structure.md:34；apps/world/src/main.cpp:191
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
  - 证据：commit 318efb64；scripts/dev_fleet.sh:87-125,176

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
  - 证据：commit f5eb0b32；docs/.vitepress/theme/components/ArchitectureDiagram.vue:12-19,39-47
- [x] 审核发现：`architecture-{lang}.svg:4` 箭头 marker 用 `fill="context-stroke"`，MDN BCD 显示 Safari 不支持该值（version_added=false），Safari 下箭头回退为默认黑色而非沿用边线颜色，与 Chrome 124+/Firefox 111+ 渲染不一致（docs/public/images/architecture-en.svg:4）。
  - 证据：commit f5eb0b32；docs/public/images/architecture-en.svg:4 使用显式 fill 颜色
- [x] 审核发现：`ArchitectureDiagram.vue:15` 将 system 图源切至 `/images/architecture-{lang}.svg` 后，旧的 `docs/public/architecture/system-{en,zh}.svg` 及 `system-{en,zh}-mobile.svg` 仍被 git 跟踪但组件不再引用（仅 todo.md:301 历史文字提及），成为死资产，建议清理或确认废弃（docs/.vitepress/theme/components/ArchitectureDiagram.vue:15）。
  - 证据：commit f5eb0b32；docs/public/architecture/ 无 system-*.svg 残留
- [x] 审核发现：本轮 diff 未见问题（4bd4185e..HEAD）。
  - 证据：commit 4bd4185e；测试 501/502

- [x] 审核发现：`scripts/docs/render_architecture.py:283` 从生成循环移除 `system` 后，`system()` 函数（行 57）成为无调用死代码；且本 diff 新增/编辑的 `docs/public/images/architecture-{en,zh}[,-mobile].svg`（含窄屏移动版）不在 `--check` 校验的 12 份 `architecture/{space,cross-server,callbacks}*.svg` 之列、无再生成源，架构图今后可静默漂移（scripts/docs/render_architecture.py:283）。
  - 证据：commit acffdda4；测试 16/16
- [x] 审核发现：测试目标改名（a21826ea，`baseappmgr_tests`→`online_directory_tests`、`baseappmgr_m1_control_tests`→`online_directory_m1_control_tests`）后遗留两处旧目标名注释——`CMakeLists.txt:438`「session_world_tests / base_anchor_tests / baseappmgr_tests 全部静默失联」与 `tests/test_baseappmgr_m1_control.cpp:82`「baseappmgr_tests 同例」均引用已不存在的目标名（现为 `online_directory_tests`），与「仅目标名无行为变化」同步口径不符（CMakeLists.txt:438）。
  - 证据：commit acffdda4
- [x] 审核发现：新增源模板 scripts/docs/templates/architecture-zh-mobile.svg 的正文（body/label）文本仍为英文（如 L11 "Login · Input · State"、L38 "Player profiles & base gameplay"、L44 "Register · Topology · Capacity · Migrate"、L50 "Scene · Avatar · AOI · Battle · Lua" 等），仅节点名与图例已中文化，与已全量中文化的宽屏 architecture-zh.svg 不一致，中文窄屏图呈现英文正文（scripts/docs/templates/architecture-zh-mobile.svg:11）。
  - 证据：commit 410591bc；scripts/docs/templates/architecture-zh-mobile.svg:8,11-64
- [x] 审核发现：scripts/docs/render_architecture.py:237 非 `--check` 生成分支只对 OUT（docs/public/architecture）做 `mkdir`，未对新目标 IMG（docs/public/images）做 `mkdir`；若 docs/public/images 目录缺失，写入 system 图将抛 FileNotFoundError 而非自动重建，与 OUT 处理不对称（scripts/docs/render_architecture.py:237）。
  - 证据：commit 536b7374；scripts/docs/render_architecture.py:237-238
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
  - 证据：commit af599e31；apps/world/src/cell_main.cpp:680-693；WorldAppM1LinkTests+UpdateContinuityTests 2/2
- [x] 审核发现：cell 执行端 cell_last_rollout_name 在 prepare 成功即登记（apps/world/src/cell_main.cpp:579），而 post_tick 无条件按它 call_module(name,"onTick")（apps/world/src/cell_main.cpp:383-385）——commit 前（kCanaryPending）模块尚未换入目标 runtime，call_module 落「module not a table」fault 入 module_fault_log_，污染 canary 观察窗/熔断器，可致误判坏提前回滚；rollback/abort 后该名也未清空，onTick 持续打在已回滚模块上。
  - 证据：commit af599e31；apps/world/src/cell_main.cpp:655-670；WorldCellIndependentSmoke 1/1
- [x] 审核发现：README.zh.md 新增 docs 徽章 href 为 `https://cuihairu.github.io/apollo/zh//`，但 VitePress `rewrites` 把 `zh/:path*` 重写到根 `:path*`、构建产物无 `zh/` 目录——中文文档实际部署于 `https://cuihairu.github.io/apollo/`，`/apollo/zh/` 为 404（另含尾双斜杠），与中文文档位置错配（README.zh.md:4）。
  - 证据：commits af599e31,9b8a125d；README.zh.md:12；docs_i18n_check 通过
- [x] 审核发现：README.md 新增 docs 徽章 href 为 `https://cuihairu.github.io/apollo//`（尾双斜杠，浏览器规整为 `/apollo/`），指向中文根文档而非英文文档——英文 README 的 docs 徽章应链接英文文档 `https://cuihairu.github.io/apollo/en/`（VitePress 英文 locale 挂载 `/en/`），语言错配且含双斜杠（README.md:4）。
  - 证据：commits af599e31,9b8a125d；README.md:12；docs_i18n_check 通过
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
  - 证据：commit 7c48a83d；tests/world_cell_independent_smoke.py:362-380；WorldCellIndependentSmoke 1/1
- [x] 审核发现：3d 注释声称「健康窗零 fault（回执 faults=0）直接证明」（tests/world_cell_independent_smoke.py:384），但 promote 轮询只断 `r["state"]==6`（tests/world_cell_independent_smoke.py:368-374），从未断言 `r["faults"]==0`——window_faults 仅由 kPromoted 终态隐式蕴含（ModuleRollout::evaluate_ 全窗健康才 promote，module_rollout.cpp:176-204），显式回执断言缺失，注释与代码不符。
  - 证据：commit 7c48a83d；tests/world_cell_independent_smoke.py:381-390；WorldCellIndependentSmoke 1/1
- [x] 审核发现：覆盖范围 49e385ff0793..HEAD（4 文件）已全审并独立核对——README.md:12 与 README.zh.md:12 两条 docs 徽章回归已修（08df2b5c 改坏 zh 徽章 `/apollo/`→`/apollo/zh/` 线上 404；8b1d9d8d 改坏 en 徽章 `/apollo/en/`→`/apollo/` 语言错；已回改正确目标并提交）；`tests/world_cell_independent_smoke.py` 新增 `RolloutM1Client` 协议逐项与实现核对无问题（crc32c 反射 0x82F63B78/检查向量 0xE3069283、帧线形 16B 头+载荷+4B 尾、Hello 子型 0+kHelloProto 0x4C494E4B+u64 route_version+u64 epoch+u32 名长、952 八字段/953 七字段序与类型含 ok=Bool 1B、RolloutState 枚举 2/3/6/7、world 952→953 转发链 world_app.cpp:1130-1198、committed 日志串 cell_main.cpp:600-601、kPromoted 下 op3 拒判定 cell_main.cpp:619-627），协议面无问题
  - 证据：commit 8b1d9d8d
- [x] 审核发现：`scripts/ci/docs_i18n_check.py` 不校验 README 徽章 href（无 README/badge/href 匹配），本次两条徽章回归无 CI 守门——补 docs_i18n_check 的 README 徽章 href 校验（en→`/apollo/en/`、zh→`/apollo/`）。
  - 证据：commit 7c48a83d；scripts/ci/docs_i18n_check.py:63-85

- [x] 审核发现：dashboard.md:37 数据源清单把 link/TLS 状态写成 `LinkState{kIdle,kConnecting,kUp,kDown}`，但实际枚举第二值为 `kHandshaking`（modules/net/link/include/apollo/net/link/interserver_link.hpp:55-59）；`kConnecting` 只存在于另一枚举 `ConnectState`（modules/net/link/include/apollo/net/link/tcp_transport.hpp:69），LinkState 无此值——后续按此文档实现 `snapshot_links()` 会把握手态映射成不存在的状态名（docs/design/dashboard.md:37）。
  - 证据：commit 7c48a83d；docs/design/dashboard.md:37
- [x] 审核发现：dashboard.md:27 安全模型允许 `?token=<t>` 查询串携带 token——查询串 token 会进浏览器历史/HTTP 访问日志/Referer，与本设计自述「状态页比 Prometheus 指标更敏感、默认只绑环回」的防泄漏姿态相悖；建议仅 `Authorization: Bearer`，或显式记录查询串 token 的残留泄漏风险（docs/design/dashboard.md:27）。
  - 证据：commit 7c48a83d；docs/design/dashboard.md:27 明确「不用查询串」
- [x] 审核发现：dashboard.md:61 与 :77 不一致——§5 `/api/state` JSON 形状 `orders.pending_detail` 未给条数上限，§6 页面却写「前 20 条 pending」；作为脚本化查询面，orders 积压大时 `/api/state` 响应无界，建议在 API 形状同样界定 pending_detail 条数（docs/design/dashboard.md:61,77）。
  - 证据：commit 7c48a83d；docs/design/dashboard.md:61,67,77-79 统一上限 20
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
  - 证据：commit a59fdc8c；apps/world/src/dashboard.cpp:197-199 为唯一计数点；DashboardTests 1/1
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
  - 证据：commit 13e1b8ee；docs/package.json:20-25 为 overrides 单一来源，docs/pnpm-workspace.yaml 已删除，pnpm-lock overrides 一致
- [x] CI 的 pnpm 9 只读 package.json 的 overrides，删除该配置后冻结安装因 overrides 与锁文件不匹配而失败。**已修复**：overrides 回到 docs/package.json 单一来源并删除 docs/pnpm-workspace.yaml 双源，锁文件 overrides（vite 6.4.4/katex 0.18.2）核对一致（docs/package.json:20-25）。
- [x] 并发测试把 close 返回后的认证回调一律判为失败，但 CAS 可在 close 前成功而回调被调度延迟至 close 后，正确实现也会随机误报。**已修复**：判据改为最终状态（close 恒为最后写入者，会话必须停在 Disconnected），删除回调时序判据与 AuthAfterCloseRecorder（tests/test_rpc_session_manager.cpp:190-236）。
- [x] 并发测试只检查 close 返回后的标志而不检查最终状态，旧实现在标志设置前的复活窗口会漏报。**已修复**：并入上一条——最终状态断言下，旧实现两步写回复活必然停在 Authenticated 而失败（tests/test_rpc_session_manager.cpp:228-233）。
- [x] Windows 下 EventLoopReentrantTests SEGFAULT（CI 41907458774 首次 Windows Test 步实证）：`EventLoop::Impl` 的唤醒通道在 Windows 用 `CreatePipe` 的 `HANDLE`，随后经 `reinterpret_cast<socket_t>` 塞进 `WSAPoll`——`WSAPoll` 只接受真实 Winsock socket，把 kernel HANDLE 当 SOCKET 用是非法句柄→UB/SEGFAULT。**已修复**：Windows 唤醒改用回环 TCP socket pair（read=accept / write=connect），事件循环与 wakeup 全部走真实 `SOCKET`（`recv`/`send`/`closesocket`），并在本翻译单元显式 `WSAStartup`/`WSACleanup`（`http.o` 静态初始化不保证被链接）；测试移除 Windows 跳过，恢复真实用例（tests/test_event_loop_reentrant.cpp:142-146；modules/net/http/src/event_loop.cpp:74-98,113-117,213-217,260-263）。测试：Linux `EventLoopReentrantTests` 3/3 绿；Windows 交叉编译 + wine 3/3 绿。
- [x] 审核发现：960 新增第 11 字段（credential）破坏线格式向后兼容——旧版 10 字段 cell 与新版 World（及新 cell 与旧 World）互连时在 decode 层被静默丢弃（`++rejected_`、无 961 回执、无 code 5 诊断），compatWhitelist 声称的「混编旧 cell 与新版 World 并存」对真实旧二进制无法生效，只能接纳新格式但显式声明旧投影的 cell（apps/world/src/m1_link_server.cpp:820）。
  - 证据：`decode_payload_fields`/`encode_payload_fields` 新增（modules/net/contract_wire/src/contract_wire.cpp:264-289,305-322）；`handle_cell_register_frame` 先按 11 字段解码、失败回退 10 字段旧帧并视为 credential 空（apps/world/src/m1_link_server.cpp:820-845）；测试 `test_cell_register_credential_compat_legacy` 覆盖旧 10 字段帧受理与认证开拒绝（tests/test_cell_register.cpp:1401-1495）。测试：CellRegisterTests 89/89 绿。
- [x] 审核发现：cell 侧 code 6（凭据失配）拒绝不触发自冻结（仅 code 3/5 触发），被拒 cell 每 heartbeat 无限重发 960 且不隔离——World 重启轮换 registerToken 后，曾短断链的合法 cell 以旧凭据重注册遭 code 6，却继续模拟其已分配的 Space，与「World 已隔离本端→自冻结停止权威副作用」的既有语义不一致，存在双主风险（apps/world/src/cell_register_client.cpp:324）。
  - 证据：`handle_reply` 冻结条件加入 `kCellRegisterRejectedCredential`（apps/world/src/cell_register_client.cpp:320-332）；测试断言 code 6 触发 `kAdjudicationRejected` 冻结（tests/test_cell_register.cpp:1476-1490）。测试：CellRegisterTests 89/89 绿。
- [x] 审核发现：凭据裁决（code 6）置于代次裁决（code 3）之前——旧代次执行端凭据过期/轮换时收到 code 6 而非 code 3，失去自隔离信号；恰是代次裁决处代码注释明确警告的「若被 code 5 抢先答则旧执行端失去自隔离」同类场景（apps/world/src/world_app.cpp:275）。
  - 证据：凭据裁决块移入代次裁决之后（apps/world/src/world_app.cpp:275-323）；测试「旧代次 + 错凭据 → code 3 而非 code 6」断言（tests/test_cell_register.cpp:1447-1459）。测试：CellRegisterTests 89/89 绿。
- [x] 审核发现：启动凭据经命令行 `--register-token` 传入，`/proc/<pid>/cmdline` 与 `ps` 对同宿主任意用户可读，machined 子进程 exec 失败也会把完整命令行打到 stderr（apps/machined/src/supervisor.cpp:201）——与 config.hpp「独立运行目录/环境注入、凭据不进可公开日志」的声明通道不符，凭据可被同机进程窃取（apps/world/src/cell_main.cpp:145、apps/world/src/main.cpp:112）。
  - 证据：两入口删除 `--register-token` CLI 分支、改读 `APOLLO_REGISTER_TOKEN` 环境（apps/world/src/main.cpp:278-290、apps/world/src/cell_main.cpp:276-284）；help 文案改 `(env APOLLO_REGISTER_TOKEN)`（apps/world/src/main.cpp:247、apps/world/src/cell_main.cpp:252）；实测 `world --register-token foo` exit=2 未知参数。测试：world/cellServer 构建绿。
- [x] 审核发现：凭据校验用非定长常量时间的 `std::string operator!=`（首字节不同即短路，可经计时侧信道逐字节枚举），且 M1 链路为明文 TCP、凭据为全体 cell 共享的单一静态值（无逐 cell 绑定、无挑战应答、无轮换）——嗅探/泄漏一次即成为永久凭证，可重放注册任意 cell_id，与「拒伪造注册」的强度目标不匹配（apps/world/src/world_app.cpp:275）。
  - 证据：凭据比较改 `constant_time_string_equal`（长度先比 + 全量异或累积不早退，apps/world/src/world_app.cpp:36-48,318-320）；测试：CellRegisterTests 89/89 绿。
- [x] 审核发现：961 的 `code` 字段契约词汇未收录新 code 6（kCellRegisterRejectedCredential），仍只列 1-5，生成的 h/json/proto/lua 同步携带旧文——契约消费方（Lua 路由、客户端）无法发现 code 6 语义（sdks/contract/messages.xml:634）。
  - 证据：messages.xml 961 code 词汇加 `6=rejected_credential`（sdks/contract/messages.xml:634），`apollo_gen` 重新生成 h/json/proto/lua（sdks/cpp/generated/apollo_contract.h:724 等）；`apollo_gen_golden_check` 通过。测试：NetWireTests 绿。
- [x] 审核发现：`--compat-whitelist` 用 `strtol` 基 0 且不检查 errno/ERANGE——越界条目静默钳位到 LONG_MAX/LONG_MIN 入白名单，前导 0 条目按八进制解析（"0755"→493），配置拼写错误被静默采纳为错误的兼容投影（apps/world/src/main.cpp:121）。
  - 证据：改 `strtoll` 基 10 + `errno`/全消费检查，0 条目忽略（apps/world/src/main.cpp:113-132）；实测 `0755`→755（十进制）、溢出/垃圾 exit=2 拒绝。测试：world 构建绿。
- [x] 审核发现：凭据裁决移到代次裁决之后（apps/world/src/world_app.cpp:318-319）使错误凭据的探测者先在代次裁决处收到 code 3 + detail「stale epoch: registered=<epoch>」（:308-309）或「watermark=<epoch>」（:301-302）——未经凭据验证即泄露该 cell_id 是否已登记及其启动代次（可逐 cell_id 枚举），与「拒伪造注册」目标相悖；改前凭据先判，错凭据一律 code 6 不泄露任何登记状态（回归自本提交）。
  - 证据：凭据裁决块移回代次裁决之前（apps/world/src/world_app.cpp:290-301,310-315）；旧代次 + 错凭据重注册改断言 code 6，新增负向断言「credential reject detail 无 epoch 字样」（tests/test_cell_register.cpp:1452-1462）；code 6 与 code 3/5 同冻结条件，旧代次凭据过期仍自隔离（apps/world/src/cell_register_client.cpp:320-332）。测试：CellRegisterTests 绿。
- [x] 审核发现：`--register-token` 从 world 与 cellServer 入口删除、改读环境 `APOLLO_REGISTER_TOKEN`（apps/world/src/main.cpp:286、cell_main.cpp:280），但仓库内无任何子进程环境注入代码——config.hpp「World spawn 子进程时注入该环境变量」与 main.cpp:284「改由 APOLLO_REGISTER_TOKEN 环境传递，仅 World 侧配置」仅为注释，实际仅靠 machined `execvp` 继承 environ（supervisor.cpp:198）被动传播；若 cell 进程树未继承 World 的环境（独立启动、machined 早于 World 起），认证开时 cell 恒收 code 6 永不登记，与注释声称的注入机制不符。
  - 证据：修正 4 处「World spawn 子进程时注入」为「部署方须设进 World 与 cell 共享进程环境（machined fork+execvp 或同源启动脚本透传）」，删除虚构注入声明（apps/world/include/world_app/config.hpp:66-76、apps/world/src/main.cpp:278-291、apps/world/src/cell_main.cpp:272-285、apps/world/include/world_app/cell_register_client.hpp:66-75）。测试：CellRegisterTests 绿。
- [x] 审核发现：apps/world/src/m1_link_server.cpp:826-828 注释称旧版 10 字段 cell「只有显式声明白名单投影才能通过兼容裁决」，但凭据裁决（world_app.cpp:318-319）在兼容裁决之前——registerToken 非空（认证开，即本批加固目标形态）时旧帧 credential 为空恒被 code 6 拒，根本到不了白名单兼容路径；旧帧受理仅在认证关时成立，注释对混编兼容性的描述过宽。
  - 证据：注释收窄为「旧帧受理仅在认证关（registerToken 空）时成立；认证开时旧帧在凭据裁决处被 code 6 拒，到不了兼容白名单路径」（apps/world/src/m1_link_server.cpp:820-831）；凭据先判已由 world_app.cpp 与测试 `test_cell_register_credential_compat_legacy` 认证开旧帧断言 code 6 固定（tests/test_cell_register.cpp:1414-1423）。测试：CellRegisterTests 绿。
- [x] 审核发现：本轮 diff（c85c3dc5..HEAD）未见问题。
  - 证据：复核 c85c3dc5..HEAD 仅含 todo.md 拓扑复核注释（:106），注释所述 D03 代决（一个 Space 一个完整 Cell、World 独占分配、稳定分配无自动迁移、每 Instance 一个逻辑负责人）与 docs/review/2026-10-30-actions/02-topology-assignment.md §1.1 一致；首阶段已落（spaceAssignments_ 每 Space 单调 assignment_version + journal A/R/B 四例版本规则（创建 v1/释放版本不变/重认领 +1/重启 B 行铸代次，apps/world/src/world_app.cpp:548,557,635,1010-1027）+ SquadInstanceService 按 world_id 绑定，apps/common/src/squad_instance.cpp:55-56,83-84）；测试 CellRegisterTests+SquadInstanceTests 绿、build-off 89/89。
- [x] 审核发现：本轮 diff 未见问题（a0ae72f9..HEAD）。
  - 证据：`git diff --stat a0ae72f9..HEAD` 仅 `todo.md` 4 行（新增 c85c3dc5..HEAD 与 a0ae72f9..HEAD 两条复核记录，各含条目行与证据行，无代码/契约变更）；`ctest --test-dir build-off` 89/89 通过。
- [x] 审核发现：a0ae72f9..HEAD 的复核证据称两行包含两条复核记录，但提交该记录前实际只有 c85c3dc5 复核记录及其证据两行，提交后则共有四行，所记范围和内容不符（todo.md:413）。
  - 证据：`git diff --stat a0ae72f9 7d2cab39` = `todo.md | 4 ++++`（两条复核记录各 2 行，共 4 行，无代码/契约变更），与修正后证据行一致；修正前误记 2 行已更正为 4 行。测试：`ctest --test-dir build-off` 89/89 通过。

## 源码分层分析 2026-11-07

### a) 抽象层清单

- L0 进程壳与生命周期：统一入口参数、崩溃捕获、应用门控和 PreInit→Ready 组件生命周期。（apps/CMakeLists.txt:34；include/apollo/server/game_server.h:146）
- L1 应用装配与状态面：把配置、连接、路由、后台泵和常驻状态表装配成单个可启停服务。（apps/gateway/include/gateway/gateway_server.hpp:200）
- L2 传输与会话边界：承载客户端 TCP/WS/WSS/TLS、packet、认证、心跳、准入、限流和会话缓存。（include/apollo/net/session.h:86）
- L3 服务间协议与路由：以 XML 契约生成的路由 JSON 装载 M1 intent/rpc 端点并维护代次与重连。（apps/world/include/world_app/m1_link_server.hpp:30）
- L4 IPC 与服务发现：提供共享内存/Socket/QoS 通道和 SQLite、Redis、内存三种端点发现后端。（include/apollo/ipc/channel.h:20）
- L5 契约生成与客户端 SDK：把 XML/XSD 属性、消息、实体生成到服务端与 Unity/Cocos/Laya 客户端调用面。（sdks/gen/src/main.cpp:105）
- L6 游戏内核与 cell 执行：运行属性、AOI、Scene、tick、意图队列、冻结闸和 Lua 契约处理器。（apps/world/include/world_app/cell_app.hpp:124）
- L7 数据归属与持久化：管理玩家缓存、write-ahead journal、原子文件、迁移线和数据库 RPC。（apps/common/include/player_store/storage.hpp:11）
- L8 观测监督与运维：汇聚日志、导出指标、告警、看板、fork/exec 监督和 PID/死亡事件。（apps/machined/src/supervisor.cpp:190）

### b) 超大文件候选

- [x] LAY-001 [P1] 拆分 apps/world/src/cell_app.cpp（1519 行）——进程 tick 主循环、意图受理、Space 生命周期、场景模拟和观测输出挤在同一个实现单元。（apps/world/include/world_app/cell_app.hpp:124）**已落地** `54415ed4`：按职责分 6 单元——`cell_app.cpp` 537 行（生命周期 start/stop/frozen + 受理 submitIntent/onIntent/sendAck + 内核循环 gameLoop/executeIntent/drainAcceptanceQueue + tickInterval/record_metrics/deliverToClient/find_scene）；`cell_handlers.cpp` 292 行（实体/战斗 5 handler + broadcastToViewers/session_of_entity/find_scene_by_entity/entity_type_name）；`cell_squad.cpp` 313 行（队伍/小队副本 handler + driveSquadGameplay + deliver* + resolveIntentPlayer）；`cell_script.cpp` 207 行（脚本宿主 attach/register/lua_*/dispatch_script_intent/contract_id_for）；`cell_spaces.cpp` 206 行（Space 生命周期 add/remove/revoke/drain + resolve_scene_id + space_descriptor_for/declared_space_name + ensureDefaultScene）；`cell_sessions.cpp` 136 行（attach/detachPlayerWorldSession）；共用日志出口 `cell_app_detail.hpp`（cell_detail::world_log 单一定义于 cell_app.cpp）。行为保持：纯移动 + 日志出口转发，无逻辑改。装配：apps/world `WORLD_CELL_APP_SOURCES`（world+cellServer）+ tests `APOLLO_CELL_APP_SOURCES`（14 处测试目标同源）。测试：全树构建绿；build-off ctest 89 套 88 绿（WorldCellIndependentSmoke 负载下 49.83s 超时，隔离重跑 22.75s 绿——环境噪声；cell_app_tests/cell_register_tests/world_drain_tests/squad_wire_e2e 等 cell 面全绿）。
- [x] LAY-002 [P1] 拆分 apps/gateway/src/gateway_server.cpp（1478 行）——ingress 装配、会话生命周期、三向路由、控制面和 dashboard 序列化共存。（apps/gateway/src/gateway_server.cpp:190）**已落地** `c054a369`：按职责分 6 单元——`gateway_server.cpp` 512 行（构造/多监听装配 assemble_client_ingress/makeAdmissionService/start/stop/Base 前递 baseClientForEndpoint/setBaseResolver/监听端口观测）；`gateway_router.cpp` 136 行（MessageRouter 全部方法：M1 出口装配/世界与 Base 前递/939 下行出口/buildDefaultRoute）；`gateway_sessions.cpp` 218 行（入口观察者回调 onConnectionOpened/PacketReceived/Closed/Idle + tryResumeSession + heartbeatCheckLoop + onClientDisconnect/destroySessionAndNotify + encodeDisconnectMessage）；`gateway_client.cpp` 402 行（handleClientMessage/handleClientAuth/createPendingSession/authenticateSession/activateAndBindAtBase/sendToClient/ensure-fetchRouteSnapshot + routeLabel/kM1RouteTimeoutMs）；`gateway_pump.cpp` 140 行（acceptNewConnections 宿主泵/recordMetrics/handleDrainRequest/drain_pump/enter_drained/steady_now_ms）；`gateway_dashboard.cpp` 184 行（dashboardJson + /proc 读取 + json_escape/drain_state_label）；共用日志出口 `gateway_server_detail.hpp`（server_detail::glog 单一定义于 gateway_server.cpp，各 TU 匿名转发调用点不变）。行为保持：纯移动，无逻辑改。装配：apps/gateway `GATEWAY_SERVER_SOURCES` + tests `APOLLO_GATEWAY_SERVER_SOURCES`（12 处测试目标同源）。测试：全树构建绿；build-off ctest 89 套全绿（GatewayWsIngress/SessionResume/BaseActivation/M1Egress/PushDownlink 5 套 gateway 面全绿，201.71s 总时）
- [x] LAY-003 [P1] 拆分 src/apollo/utils/crypto.cpp（1472 行）——通用工具路径承载了过多算法实现，难以为算法族独立测试和替换。（include/apollo/utils/crypto.h:1）**已落地** `d14f7c1d`：按算法族分 6 单元——`crypto.cpp` 361 行（平台头块 + SecureRandom 全部 + Hash 门面 md5/sha256/sha1/murmur3/xxhash32/cityHash64/toHexString）；`crypto_digest.cpp` 397 行（detail 族增量摘要 SHA256/SHA1/MD5：update/finish/transform）；`crypto_encode.cpp` 272 行（Base64 编解码表+方法 + Hex + URLCodec）；`crypto_cipher.cpp` 212 行（SimpleCipher xor/rotate/substitution + HMAC-SHA256 RFC 2104）；`crypto_checksum.cpp` 173 行（adler32/crc32/crc16/sum8/16/32）；`crypto_password.cpp` 119 行（PasswordHash PBKDF2-HMAC-SHA256 pbkdf2/generateSalt/hash/verify/format/parse）。行为保持：纯移动，跨族引用全经公开头 crypto.h（detail 类已在其内声明）。装配：APOLLO_CRYPTO_SOURCES 变量进四处——根 CMakeLists（legacy apollo lib）+ tests（6 处测试目标）+ apps/login + apps/interfaces。测试：全树构建绿；build-off ctest 89 套全绿（crypto_tests 自包含覆盖 PasswordHash 面，148.25s 总时）。
- [x] LAY-004 [P1] 拆分 apps/world/src/world_app.cpp（1263 行）——cell 注册裁决、容量、Space 分配、journal 回放和排空控制在一个类内交织。（apps/world/include/world_app/world_app.hpp:233）**已落地** `9067faf1`：按职责分 5 单元——`world_app.cpp` 231 行（ctor/dtor/start/stop：M1 端点装配 + 管理泵 + PersistentVolumeLock）；`world_cell_registry.cpp` 368 行（961 受理裁决含常量时间凭据校验/onPeerLinkDown 存疑标记/suspect_cell_count/firstReady-RegisteredCellPeer/begin-cancelCellRetire/onCellRetire/retired_cell_count/find_cell_registration/driveCellRegistry 心跳摘除）；`world_space_assign.cpp` 271 行（pushSpaceAssignments 962 全池视图/lower_load_peer 让位/releaseCellSpaces/onCellSpaceReport 967 对账/find_space_assignment + capability_fit）；`world_journal.cpp` 272 行（loadWorldJournal v2 装载回放/appendJournalRow fsync 闸/recordEpochWatermark 墓碑 + pool_has）；`world_m1_ops.cpp` 204 行（944/952/956 受理转发 + 945/953/957/929 回程收账 + drop-sweepExpiredCellReplyAccounts 在途账面 + steady_now_ms/kM1EvalBacklogMax）。行为保持：纯移动；共用日志出口 world_app_detail.hpp（detail::world_log 单定义于 world_app.cpp）。装配：apps/world `WORLD_APP_SOURCES`（world 目标；cellServer 不编管理壳，原样）+ tests `APOLLO_WORLD_APP_SOURCES`（12 处测试目标同源）。测试：全树构建绿；build-off ctest 89 套全绿（WorldCellIndependentSmoke 26.56s 绿，176.63s 总时）。
- [x] LAY-005 [P1] 拆分 apps/fleetctl/src/main.cpp（1171 行）——CLI 解析、编队协议、多个 RPC 等待循环和控制流程都进入入口文件。（apps/fleetctl/src/main.cpp:586）**已落地** `8e838c69`：按命令面分 5 单元 + 共享内面——`main.cpp` 262 行（文件头注释 + Windows 桩 main + POSIX main 分流 + printUsage/parseArgs）；`fleetctl_internal.hpp` 230 行（namespace fleetctl 内别名/id_of/泵拍常量/Options/ControlDial 全类/now_ms/7 个 run* 入口声明）；`fleetctl_rpc_cmds.cpp` 180 行（runAssign/runResolve/runMigrate/runEval + EvalCapture 945 回执捕获）；`fleetctl_module.cpp` 284 行（RolloutCapture/RolloutPeer/rollout_call/wait_terminal/print_target + runModule 跨 World 灰度编排）；`fleetctl_drain.cpp` 156 行（DrainCapture/DrainWire/drain_wire_for/drain_call + runDrain）；`fleetctl_retire.cpp` 122 行（RetireReply/Capture + runRetire）。行为保持：纯移动；命名空间 fleetctl 收拢原先全局匿名域（Windows 桩与 POSIX 分支原样），main TU 经 using 声明引 fleetctl 名。装配：apps/fleetctl CMakeLists 五 TU 直列（无测试目标编 fleetctl 源；WorldCellIndependentSmoke 经二进制消费）。测试：全树构建绿；build-off ctest 89 套全绿（WorldCellIndependentSmoke 走 fleetctl drain 面，176.29s 总时）；fleetctl 用法路径冒烟输出正常。
- [x] LAY-006 [P1] 拆分 apps/common/src/player_store.cpp（1152 行）——存储缓存、journal、M1 端点、迁移线与线程化保存队列集中在一个文件。（apps/common/include/player_store/storage.hpp:11）**已落地** `bfce3ab4`：按职责面分 4 单元 + 共享日志头——`player_store.cpp` 441 行（ctor/dtor/start/stop：M1 base 受理面装配 + 停机五阶段 + journal write-ahead/drain 水位 + nextWriteGen 写端代次账 + autoSaveLoop/flushDirtyAvatars + getCurrentTimeMs/activateAvatar/finalizeSave 保存引擎收口 + store_detail::blog 单定义）；`player_store_session.cpp` 179 行（activatePlayer 播种串行化/set_directory_event_sink/bind-unbindSession 冻结锚点拒绑 + 目录事件源①/findAvatar/findPlayerBySession/collect_mirror_entries 目录镜像）；`player_store_rpc.cpp` 300 行（dispatchRequest 维护闸 + 7 handler：DB load/save/query、activate、bind、ping + toAvatarStateString）；`player_store_migration.cpp` 338 行（freeze-snapshot-storeMigrationSnapshot-adoptPlayer 五步接权 + 933/935/937 迁移 handler + 958 结算入账 + adoptedEpochs_ 高水位拒重放）。行为保持：纯移动；共用日志出口 player_store_detail.hpp（store_detail::blog 单定义于 player_store.cpp，各 TU 匿名转发保持 blog() 调用点不变）。装配：apps/common `apollo_app_services` 四 TU 直列 + tests `APOLLO_PLAYER_STORE_SOURCES`（base_avatar_tests/online_directory_tests 双分支 4 处同源）。测试：全树构建绿；build-off ctest 89 套全绿（migration_chain_tests/fleet_rehome_chain_tests 迁移面均绿，162.49s 总时）。
- [x] LAY-007 [P1] 拆分 include/apollo/net/adapters/sdnet_adapter.h（1138 行）——适配器、帧解析、会话映射和平台分支在单一头文件实现。（include/apollo/net/adapters/native_adapter.h:1）**已落地** `66afd921`：按职责面分 6 头 + 聚合伞——`sdnet_sscp.h` 164 行（SSCP 门面：INT8-UINT64 别名/SSSVersion/ISSBase/ISSConnection/ISSSession/ISSPacketParser/ISSSessionFactory/ISSListener/ISSConnector/ISSNet/ESDNetErrCode/NETIO 常量）；`sdnet_util.h` 58 行（SDNET_BACKEND/getCurrentTimeMs/IpAddressConverter）；`sdnet_connection.h` 188 行（SDNetConnection 连接适配 + SDNetPacketParser 帧解析）；`sdnet_session.h` 285 行（SDNetSessionBase/SDNetSession 会话映射 + SDNetSessionFactory/TemplateSDNetSessionFactory）；`sdnet_endpoint.h` 280 行（SDNetListener/SDNetConnector）；`sdnet_manager.h` 227 行（SDNetNetworkManager）；`sdnet_adapter.h` 21 行（伞：保留 APOLLO_USE_SDNET 宏与 include 面不变，聚合 6 头）。**附带修复（拆分语法验证揭示的原有潜伏编译错——该头此前无任何消费者、从未被编译，拆前原文件 -fsyntax-only 同样 16 处 error）**：SDNetPacketParser/SDNetSessionBase 内未限定 SSCP::INT32/UINT32 补限定；SDNetSession 触 protected sessionId_ 增 SDNetSessionBase::setSessionId 公共口；SDNetNetworkManager::resetStats 基类声明非 const 去掉 const override（stats_ 本就 mutable）。验证：7 头逐一 `g++ -std=c++20 -fsyntax-only -I include` 全绿；无 CMake 装配点（头文件无目标消费，保持原样）。测试：全树构建绿；build-off ctest 89 套全绿（142.20s 总时；期间 WorldDrainTests 曾在机器负载 35-47 时两度 pump-window 超时假红——二进制为 LAY-006 构建未重链、与本次头改动无关联，负载回落 ~25 后全绿复跑证实环境噪声）。
- [x] LAY-008 [P1] 拆分 apps/world/src/m1_link_server.cpp（1121 行）——路由表装载、链接生命周期、协议编解码和管理回执耦合在链路实现中。（apps/world/include/world_app/m1_link_server.hpp:30）**已落地** `1a981426`：单类拆面——M1LinkServerImpl 全类声明上收 src 本地内面头 `m1_link_server_impl.hpp` 176 行（含 m1_link_detail 共享面：id_of inline 定义 + build_legacy_intent 声明 + MsgId using；world_app 域共享别名 link/trace/wire——勿上提全局，::link 是 POSIX unistd 函数名）+ 按职责分 4 TU——`m1_link_server.cpp` 349 行（ctor/start/stop/pump/on_peer_down/snapshot_links/listenPort/gatewayConnected/received-rejectedFrames/gateway_link/load_route_table/install_dispatching 白名单装配 + makeM1LinkServer POSIX 工厂与 Windows 桩分支）；`m1_link_server_send.cpp` 309 行（12 个 send*：945/953/957/929 回执回传 + 961/966 + 962 指派 + 944/952/956 下行命令 + 939 下行 + 932 裸转发，Reliable 纪律原样）；`m1_link_server_recv.cpp` 262 行（12 个 handle_*_frame 裸帧受理解码 + 宿主回调投递 + 932 信封帧受理）；`m1_link_server_wire.cpp` 209 行（build_legacy_intent 契约→legacy 帧镜像入 m1_link_detail + decodeEnvelopeIntent 对外转发）。行为保持：37 个成员函数纯移动为类外定义（体原样、去 override、去一级类内缩进）；匿名域别名/MsgId 上收内面头命名空间。装配：apps/world `M1_LINK_SERVER_SOURCES`（world 与 cellServer 两目标同源）+ tests `APOLLO_M1_LINK_SERVER_SOURCES`（12 处同源，含 gateway_route_tests 12 空格缩进处）。测试：全树构建绿；build-off ctest 89 套全绿（140.04s 总时）。
- [x] LAY-009 [P1] 拆分 src/apollo/ipc/async_io.cpp（1118 行）——平台异步队列、事件派发、背压和定时逻辑集中在 IPC 传输实现。（include/apollo/ipc/async_io.h:1）**已落地** `d152eb10`：按后端分 5 TU——`async_io.cpp` 55 行（IoMultiplexer::getBackendName 平台探测 + create 工厂）；`async_io_iocp.cpp` 368 行（APOLLO_IO_IOCP 节：IocpMultiplexer 17 方法）；`async_io_io_uring.cpp` 216 行（APOLLO_IO_URING 节：IoUringMultiplexer 18 方法含 RingDeleter）；`async_io_epoll.cpp` 309 行（APOLLO_IO_EPOLL 节：EpollMultiplexer 21 方法含 processEvents/handle* 派发）；`async_io_kqueue.cpp` 243 行（APOLLO_IO_KQUEUE 节：KqueueMultiplexer 17 方法含 EVFILT_TIMER 定时派发）。行为保持：各节 #ifdef..#endif 连同 banner 纯移动；75 个成员定义 exactly-once（17/18/21/17+2）。装配：无 CMake 点——async_io.cpp 本就不在任何目标源表（含 APOLLO_ENABLE_IPC=ON 分支，build-off 该开关=OFF），拆分不改变任何构建面。**既有潜伏腐（拆分诊断对照揭示，原样保留不扩边修）**：该文件此前从未被编译——拆前原文件 -fsyntax-only 与拆后 TU 同样报头链错（service_endpoint_ex.h 嵌套 Config 缺省成员初始化序 / channel.h:399 std::future 缺 <future> 直含 / async_io.h:102 AsyncOp::IoCallback 先用后声明；本机带 liburing 时 IO_URING 节另有 18 处 liburing 声明缺位）：诊断集对照 拆前 38 = 头链 20 + IO_URING 节 18，与拆后 io_uring TU 38、其余 TU 各 20 完全一致——拆分零新增。测试：全树构建绿（拆后增量构建零重编——无目标消费）；build-off ctest 89 套全绿（161.79s 总时）。
- [x] LAY-010 [P1] 拆分 examples/benchmark.cpp（1108 行）——基准场景、协议生成、统计和 CLI 输入混在单个示例入口。（examples/benchmark.cpp:1）**已落地** `89d773e6`：按段分面 6 文件——`benchmark.cpp` 66 行（CLI 入口：--models/--acceptance 选段 + banner + main）；`benchmark_sections.h` 90 行（Benchmark 计时类整体上收——static run(auto&&) 是简写模板必须在头内可见；Result/printHeader/run 原样 + 9 段入口声明 + 类体沿用原 using namespace std/std::chrono，调用点不限定名保持）；`benchmark_micro.cpp` 311 行（纯 std 微基准：基础运算/容器/线程同步/内存池 + SpinLock + 锁）；`benchmark_scene.cpp` 196 行（AOI 模拟 + 消息处理编码）；`benchmark_model.cpp` 388 行（三模型场景段：banner + 匿名域 ModelConfig/ModelStats/Drifter/scatter/encode_aoi_event/run_model/printModelStats + 匿名域外 benchmarkThreeModels）；`benchmark_acceptance.cpp` 198 行（受理面段：banner + 匿名域 makeMoveFrame/elapsedUs/measureAcceptNs/measureExecuteNs + 匿名域外 benchmarkAcceptanceFace）。行为保持：12 区块连续铺贴 1..1108 纯移动（锚点正则定位+区块连续性断言）；9 个入口函数定义/声明 exactly-once、class Benchmark/SpinLock/main/两匿名域开闭 exactly-once；各 TU 内保持原文件相对顺序；4 个新 TU 全量携带原 include 块（保真优先）。装配：examples/CMakeLists.txt 单点 `add_executable(benchmark ...)` 加 4 TU（无 set 块）；build-off/build-san APOLLO_BUILD_EXAMPLES=OFF 不受影响，ctest 不覆盖此目标。测试：build-examples 全量构建绿 + 链接成功；`./benchmark --acceptance` 冒烟跑通（受理面数字量级正常：accept 590.7 ns/intent / execute 753.6 / total 1344.3，全容量档 244% cap 顶格口径不变）；build-off ctest 89 套全绿（136.85s 总时）。
- [x] LAY-011 [P1] 拆分 src/apollo/ipc/sqlite_service_discovery.cpp（1063 行）——SQLite schema、注册、订阅、健康检查和负载均衡都在一个后端实现。（include/apollo/ipc/sqlite_service_discovery.h:71）**已落地** `404862ca`：按服务面分 6 单元 + 共享内面——`sqlite_service_discovery.cpp` 201 行（辅助函数 banner + metadataToJson/jsonToMetadata 定义上收命名空间级 + ServiceEndpoint::toJson/fromJson + SQLiteServiceDiscovery banner + ctor/dtor/start/stop + initDatabase/createTables）；`sqlite_service_discovery_registry.cpp` 159 行（服务注册面：registerService/insertService/deregister/heartbeat/updateHeartbeat/updateService）；`sqlite_service_discovery_query.cpp` 159 行（发现面：discover/discoverAsync/selectOne + Watch 面：watch/unwatch/notifyWatchers）；`sqlite_service_discovery_health.cpp` 120 行（健康检查面：markUnhealthy/markHealthy/getServiceNames + 后台线程面：heartbeatThreadFunc/cleanupThreadFunc/cleanupExpiredServices）；`sqlite_service_discovery_balancer.cpp` 268 行（LoadBalancer::select/create + RoundRobin/Random/Weighted/ConsistentHash 四策略）；`memory_service_discovery.cpp` 239 行（测试用 MemoryServiceDiscovery 全类 + 原命名空间闭合）；`sqlite_service_discovery_detail.hpp` 16 行（共享序列化小工具声明，单定义在 core TU）。行为保持：12 区块连续铺贴 1..1063 纯移动（banner 定位 = 标题行-1，区块连续性断言）；46 项成员定义 exactly-once（含命名空间开闭计数）；两 helper 匿名域壳撤除、上收命名空间级声明——各调用点不限定名不变；其余全部原样。装配：顶层 CMakeLists `if(APOLLO_ENABLE_IPC)` 内 `target_sources(apollo ...)` 单源换 6 源——即原文件唯一消费点。**休眠面说明**：src/apollo/ipc/* 在 MODULAR=ON 全绿树无任何目标编译（仅弃用路径 `APOLLO_ENABLE_MODULAR_LAYOUT=OFF` 的巨构 apollo 引用）；该弃用路径配置本身阻塞于预存腐（apps/machined 引 `apollo_package_crash_handler` 无定义——helper 仅 modular 模式定义；`add_library(apollo)` 报 No SOURCES），与本拆分无关、原样保留不扩边。测试：编译纪律对照（项目 = `-std=c++20` + include 传播，无 -Werror）——拆前原文件 `-fsyntax-only` 2 错（RandomBalancer ctor/next 用未声明成员 `rng_`：service_discovery.h RandomBalancer 无此成员，预存潜伏腐，文件从未被任何绿树编译）；拆后 6 TU 仅 balancer TU 同 2 错、其余 5 TU 0 错——**拆分零新增**；build-off ctest 89 套全绿（148.28s 总时；WorldDrainTests 曾于 vcpkg 依赖安装负载期假红，隔离重跑 3.03s 绿）。
- [x] LAY-012 [P1] 拆分 apps/login/src/login_server.cpp（1047 行）——认证、目录投影、M1 注册、准入、Base 代理和运行循环同文件承载。（apps/login/include/login/login_server.hpp:60）**已落地** `9d6d6e0e`：按服务角色分 6 单元，无共享 helper（anon ns1 steady_now_ms/collect_mirror_entries 仅 LoginServer core 用、anon ns2 kM1BaseTimeoutMs/callBaseRpc 仅 preparePlayerOnline 用，无跨 TU 符号、无需 detail 头）——`login_server.cpp` 432 行（LoginServer banner + 匿名域别名/工具 + ctor/dtor/start/stop + fleetLoop 编队运行循环）；`login_authenticator.cpp` 137 行（Authenticator 全类：initTestUsers/authenticate/isLockedOut/recordAttempt）；`login_gateway_allocator.cpp` 70 行（GatewayAllocator 全类：selectGateway/updateGatewayLoad）；`login_session_manager.cpp` 182 行（SessionManager 全类：createSession/validateSession/validateSessionByTicket/removeSession/cleanupExpiredSessions/generateLoginTicket）；`login_server_handlers.cpp` 140 行（dispatchRequest/handleLoginRequest/handleGatewayAssignRequest/handlePing 请求分发面）；`login_server_agents.cpp` 176 行（匿名域 RPC 代理 + preparePlayerOnline/parseGatewayEndpoint Base 准入代理面）。行为保持：12 区块连续铺贴 1..1047 纯移动；38 项 exactly-once（含 `namespace login` 开闭 6/6、匿名域 2/2 计数）；各 TU 原样携带完整 include 块 R(1,16)。装配：apps/login/CMakeLists.txt `add_executable(login)` 单源换 6 源；tests/CMakeLists.txt 新增 `set(APOLLO_LOGIN_SERVER_SOURCES ...)`（沿用 APOLLO_M1_LINK_SERVER_SOURCES 模式）并替换 4 处直编消费点（login_chain_e2e_tests/hosted_services_tests/fleet_rehome_chain_tests/migration_chain_tests）。测试：build-off `login`+4 测试目标重建绿（6 TU 全新编+链接）；4 受影响套件绿（LoginChainE2e 0.57s / HostedServices+FleetRehome 6.06s / MigrationChain 11.08s）；build-off ctest 89 套全绿（140.53s）。

### c) 目录/分类不合理处

- `apps/common` 同时承载数据库 RPC、PlayerStore、在线目录和小队结算四组边界；`target_link_libraries` 分裂为 `apollo_app_services`、`apollo_database_rpc` 和 `apollo_squad_instance`，说明并非一个稳定公共层。（apps/common/CMakeLists.txt:10）
- `include/apollo/net` 与 `include/apollo/network` 并存，均包含 session/rpc/transport/messaging 相关 API；新旧调用面没有在文件名上表达适用边界，容易误接旧层。（include/apollo/net/session.h:86；include/apollo/network/rpc/rpc.hpp:1）
- 客户端 SDK 目录名混用 `sdks/` 与 `skds/`，前者放契约与生成物，后者放客户端 SDK；拼写差一位但职责完全不同，增加路径检索和文档链接成本。（sdks/README.md:1；skds/README.md:1）
- [x] 审核发现：LAY-001 记录 cell_squad.cpp 为 318 行，但提交 54415ed4 与 HEAD 中该文件均为 313 行，行数记录不准确（todo.md:433）。**属实已修**：`git show 54415ed4 --stat`（cell_squad.cpp | 313）+ HEAD `wc -l`（313）双确认；LAY-001 证据行 318→313 已更正（记录笔误，拆分本身无影响）。
- [x] 审核发现：LAY-005 记录 fleetctl main.cpp 与 fleetctl_internal.hpp 分别为 261 行和 231 行，但提交 8e838c69 与 HEAD 中实际分别为 262 行和 230 行，行数记录不准确（todo.md:437）。**属实已修**：`git show 8e838c69 --stat`（fleetctl_internal.hpp | 230）+ HEAD `wc -l`（main.cpp 262 / fleetctl_internal.hpp 230）双确认；LAY-005 证据行 261→262、231→230 已更正（记录笔误，拆分本身无影响）。
- [x] 审核发现：LAY-010 记录 benchmark_sections.h、benchmark_micro.cpp、benchmark_scene.cpp、benchmark_model.cpp 与 benchmark_acceptance.cpp 分别为 86、309、194、386、196 行，但提交 89d773e6 与 HEAD 中实际分别为 90、311、196、388、198 行，行数记录不准确（todo.md:442）。**属实已修**：`git show 89d773e6 --stat` + HEAD `wc -l`（90/311/196/388/198）双确认；成因＝证据取数在收尾补丁前（4 TU 各补 `#include "benchmark_sections.h"` +2、头补 using +4，行数未回填）；LAY-010 证据行五数已更正（benchmark.cpp 66 行无误）。
- [x] 审核发现：本轮 diff（2b673f5f..HEAD）为纯移动拆分、逻辑逐行一致，未见本轮引入的问题。LAY-011（404862ca）sqlite_service_discovery 拆 6 单元、LAY-012（9d6d6e0e）login_server 拆 6 单元，均逐行对照原文件纯移动无逻辑改；CMake 装配正确（6 源替换单源、4 处测试消费点同源）；todo.md 行数记录与 HEAD `wc -l` 一致。**属实**：独立复核——行多重集对照（Counter 差）：LAY-012 diff==5×(头16+ns开+ns闭) 精确、零丢失；LAY-011 零丢失仅 2 行匿名域壳撤除（`namespace {`/`} // anonymous namespace`，helper 上收命名空间级——LAY-011 证据已记录，非逻辑改），无新增非脚手架行；CMake 装配核对（apps/login 6 源、tests/CMakeLists `APOLLO_LOGIN_SERVER_SOURCES` 1 定义+4 消费、顶层 CMakeLists IPC 块 6 源）；行数记录与 HEAD `wc -l` 全一致（201/159/159/120/268/239、432/137/70/182/140/176）。
- [x] 审核发现：sqlite_service_discovery_balancer.cpp:140/155 用未声明成员 `rng_`（service_discovery.h RandomBalancer 无此成员），`-fsyntax-only` 实报 2 错；经拆前旧文件（old 713/728）同报 2 错证实为预存潜伏腐、非本轮引入（todo.md LAY-011 已如实记录）。另注：全部构建目录 APOLLO_ENABLE_IPC=OFF，IPC 拆分单元从未被真实构建（仅 -fsyntax-only 验证），属环境休眠面非本轮回归（src/apollo/ipc/sqlite_service_discovery_balancer.cpp:140）。**属实已修**：`git show 2502edb6:...sqlite_service_discovery.cpp` grep `rng_`=713/728 与发现行一致（拆前即 2 处）；`include/apollo/ipc/service_discovery.h:295-306` RandomBalancer 确无 `rng_` 成员（仅 endpoints_/mutex_），balancer.cpp ctor `rng_.seed(...)`+`dist(rng_)` 确用；补 `std::mt19937 rng_;` 成员（service_discovery.h:306，`<random>` 已在 :13）；`-fsyntax-only` 全 6 拆分 TU 0 错（baseline balancer 2 错清除）；另 6 个 IPC 文件旧/新头对照错误数全同（redis 1/service_endpoint_ex 5/service_discovery 7/shared_memory 1/channel 49/qos 53）——本修零回归（其余为预存腐不扩边）；build-off 全量构建 + ctest 89/89 绿（139.55s）。
- [x] 审核发现：本轮 diff 未见问题（2e37a24e..HEAD）**属实**：diff = cd3e3aa3（todo.md 审核发现行记录，纯文档）+ 9ded0c05（`include/apollo/ipc/service_discovery.h` +1 行 `std::mt19937 rng_;` 成员补漏 + todo.md 两发现行勾选），无其他代码改动；rng_ 修复已过 `-fsyntax-only` 6 拆分 TU 0 错 + 旧/新头对照零回归 + build-off ctest 89/89（139.55s）；CI（Build and Test）9ded0c05 全平台 success。

## 覆盖率批次1：模块测试接线修复与休眠套件清剿（2026-11-12）

- [x] 根 CMakeLists 测试接线修复：`enable_testing()` 原置于 `add_subdirectory(modules)` 之后，模块本地 `if(BUILD_TESTING)` 套件（base/core/game/bigworld/data/runtime/net）全部静默失联——`enable_testing()` 只置其后目录的测试机制；且 `enable_testing()` 本身不定义 `BUILD_TESTING`（定义它是 `include(CTest)` 的职责）。两处齐修（CMakeLists.txt：enable_testing 前移 + 显式 `set(BUILD_TESTING ON)`）后 7 个模块套件 + contract/protocol 16 测试首次接入，coverage 树 ctest 89→114 套。（CMakeLists.txt:448）
- [x] 首次编译暴露的生产修复：`thread_pool.hpp` submit 内层 lambda 把 tuple 左值绑到 `Args&&` 形参，GCC 15 std::invoke 严格化下编不过（改 `std::apply(f, std::move(tuple))`，右值展开等价原 forward 语义）；`game/core/entity.hpp` `get_component<T>` 仅声明无定义、任何调用链接即断（头内实现 + `static_assert` 约束 T 派生 IEntityComponent）；`service_discovery.h` RandomBalancer 补 `rng_` 成员（commit 9ded0c05，见上审核发现行）。
- [x] 休眠测试套件对现行 API 的修复（从未编译过的脚手架）：core comprehensive 删除 ConfigValue 已删 API 段 + 修正 ApplicationPhase::Running→Stopping、LogLevel::Warn→Warning、ConfigRegistry::clear 不存在、di add_singleton 旧签名（bean 依赖重写）；data comprehensive 修类型名/双命名空间笔误、ASSERT_TRUE 双参改 ASSERT_EQ、SqlTemplate 构造改 SimpleDataSource 工厂、未连接拒答与精确 SQL seed 对齐 impl 口径；`net/tests/net_comprehensive_tests.cpp` 743 行为初始导入脚手架、include 的旧树头已随 modular 迁移删除整体删除（modules/net/CMakeLists.txt 注明处置理由）；`data/orm/tests/data_orm_tests.cpp` 补 `connect()`（MemoryConnection 默认断连，未连接查询/更新一律拒答）。
- [x] runtime `host_shutdown_hooks` SIGILL 根因修复：ASSERT 宏失败分支 `return false;` 使 `std::function<void(StopReason)>` 回调 lambda 推导出 bool 返回类型，断言通过路径直落函数末尾无 return——值返回函数 fall-off-the-end 是 UB，GCC 15 对此生成 `ud2`（反汇编实证：+38 `je +224` → +224 `ud2`，挂在 void std::function 上即 SIGILL）。改为 hook 内记录 reason、hook 外断言（同文件 host_shutdown_hooks_order 与 tests/test_runtime.cpp 既有 hook 均为此写法，全仓其余 3 处 hook 调用点核对无同类雷）。（runtime_comprehensive_tests.cpp:389）
- [x] 覆盖率基线（coverage 树全量 114/114 绿后 gcovr `--filter modules/`，批次前 TOTAL 68.6%）：**TOTAL 21582 行 79.9%**——base 95.9 / protocol 93.2 / runtime 92.8 / contract 91.6 / game 90.8 / script 90.4 / data 87.4 / core 84.9 / net 54.6。批次前 0% 的纯逻辑 TU（id_pool/config_registry/application_context/memory_connection/application_host 等）已全部消除。
- [x] 覆盖率缺口与休眠面如实记录（批次2目标）：modules/net 54.6%——旧树 tcp/rpc/websocket/http/protocol-endpoint 约 2900 行 0%（socket 166/reactor 79/rpc 121/net_common 18/websocket 617/native_adapter 592/rpc_legacy 201/message_codec 122/session_manager 133 未覆盖/endpoint 104 行），需真实 socket/WS 集成测试批；module_manifest/runtime_manifest 各 0/2 待补。休眠面（不进绿树编译，与覆盖率口径无关）：`src/`+`include/` 遗留树 50.0%（IPC face 仅弃用 modular=OFF 路径引用）、apps/machined 34.9%（进程装配面）、apps/fleetctl 26.4%（CLI 编排，经二进制消费无源级测试）。

## 覆盖率批次2：cache 套件新建 + FileAppender 行为面 + macOS 时窗修复（2026-11-12）

- [x] `data/cache` 首个测试套件（此前库级零消费零覆盖）：`modules/data/cache/tests/cache_tests.cpp` 8 用例自包含 main 风格（orm 子目录同款）——set/get 往返与同键覆盖、miss、remove/exists、TTL 过期拒答（1.1s 实睡）、TTL 被 0-ttl 覆盖复位、cleanup_expired 清扫、CacheManager 无 provider 恒 false 且写无操作、有 provider 委托全链 + 单例复位保序。CMake 注册 apollo_data_cache_tests（cache/CMakeLists.txt:27）。注：`CacheManager::get_or_compute<T>` 头内声明无定义、任何调用链接即断——休眠 API 原样记录不扩边（同 entity.hpp get_component 先例，待需求触发再实装）。
- [x] FileAppender 行为面 5 用例进 `modules/core/tests/core_comprehensive_tests.cpp`（此前 47.3% 122/258）：同日重启续接序号（ByBoth `_1.` 后缀 + 旧文件不覆写）、按大小轮转 + maxFiles=2 清理上限、级别过滤（isEnabled 位掩码口径——掩码含 Error 才放行 Error，非阈值语义，用例注释写明）、structuredOutput logfmt 行（ts= 首键/level=INFO/msg=hello 且无人读格式）、setConfig 换目录重开且旧文件保留。core 84.9%→**88.9%**（2230 行 1982 覆盖）。注：原拟加进根 tests/test_core_log.cpp 后撤回——该套件仅 legacy 分支（链旧 apollo 库）装配，现行绿树不编译，休眠面保持原样不扩边。
- [x] macOS CI 红（f8b165a3，base_comprehensive test_time_timer `elapsed<=100` 断言 50ms sleep 超窗）：上界改 5000ms 只防单位错误、不量调度速度（注释写明成因——加载下 macOS 调度 50ms sleep 常越 100ms，紧上界是对调度器的断言非对 Timer 的断言）。build-cov 115/115 绿 + WorldDrainTests 隔离重跑 2.80s 绿（负载假红既证）。
- [x] 覆盖率批次2（同口径 gcovr `--filter modules/`，全量重链+清计数器后采集）：**TOTAL 21812 行 80.7%**（批次1 79.9%）——base 95.9 / data **93.9**（945→1035 行，cache 并入且覆盖）/ core **88.9**（84.9→）/ protocol 93.2 / runtime 92.8 / contract 91.6 / game 90.8 / script 90.4 / net 54.6。采集中 gcovr `no_working_dir_found` 崩 = 6 对 gcda/gcno stamp 错配（并发 ninja 产物），逐对 `gcov -n` 定位、删 .o 重编全量重链后复采（错配文件恰为聚合中消失的 ~600 行）。

## 覆盖率批次3：net/tcp 传输层回环集成测试 + manifest 字段面（2026-11-12）

- [x] `modules/net/tcp/tests/tcp_socket_tests.cpp` 首个真回环集成套件（gateway 客户端接入的生产传输栈，此前 socket.cpp 0/166）：NetAddress 构造/ToString、Socket 创建+选项组（非阻塞/TCP_NODELAY/REUSEADDR/收发缓冲/UDP）、Buffer 读写语义（Peek 不推进/读尽复位/Discard+Compact 保留未读段/EnsureWritable 扩容）、阻塞 echo 回环（端口 0 内核分配 + Accept/SendAll/ReceiveAll + GetRemoteAddress 端口核对）、对端关闭语义（非阻塞轮询 5s 上限：先收 1 字节再 EOF→SOCKET_CLOSED，不依赖阻塞死等）。CMake 注册 apollo_net_tcp_tests（modules/net/CMakeLists.txt 末段，Windows 分支补 ws2_32）。
- [x] module_manifest/runtime_manifest 字段面（各 0/2 → 全盖）：core/runtime comprehensive 各加 1 用例断言 name/layer/description 字段值 + 静态单例同一性（`&core_manifest() == &manifest`）。
- [x] 覆盖率批次3（同口径）：**TOTAL 21968 行 81.2%**（批次2 80.7%）——net 54.6→**56.9**（socket.cpp 0→66.3%、net_common.cpp 94.4%）、runtime 93.1、core 89.0；本地全量 116/116 绿。tcp 内剩余 0%：reactor.cpp 0/79（异步事件循环，需 Reactor 驱动的定时器/socket 事件测试，单独批）、tcp/rpc.cpp 0/121（legacy 帧面）；net 旧树其余 0% 大头不变（websocket 617/native_adapter 592/rpc_legacy 201/message_codec 122/endpoint 104/http 4）。

## 覆盖率批次4：Reactor poll 事件循环（2026-11-12）

- [x] `modules/net/tcp/tests/reactor_tests.cpp` 3 用例：WRITE 即时可写事件（已建连 socket 发送缓冲即刻可写，回调触发后 Modify 切 READ、对端写后 READ 回调内 Receive 读得数据）、Remove/重复 Add/未知 fd 语义（Remove 二次失败、Modify 未知 fd 失败）、空 fd 集循环 Stop 收敛（10ms idle 分支，Stop 后 EventLoop 退出）。断言全部带 5s 上限轮询，不依赖阻塞死等。CMake 注册 apollo_net_reactor_tests（modules/net/CMakeLists.txt）。
- [x] 覆盖率：reactor.cpp 0/79 → **93.7%**（74/79，未盖行为 Stop 后残留 fds 一轮）；tcp 内 0% 仅剩 tcp/rpc.cpp 0/121（RpcClient/RpcServer 依赖 protobuf::Service stub，需生成桩或手写最小 Service，后续批）。WorldDrainTests 全量期间负载 ~4.2 时 pump-window 假红两次、直接跑二进制全绿（exit 0）——既证环境噪声非回归。





## 审核修复批 P0（audit-batch2-6repos-2026-11-11，2026-11-12 起）

### P0-1 PlayerDirectory 跨线程无锁并发（online_directory.cpp:122 + login_server.cpp:378）

- [x] 修复：`PlayerDirectory` 内部 `mutable std::recursive_mutex mutex_`（player_directory.hpp:196），全部公开入口持锁串行化——M1 泵线程（bindSession/assignWorld → session_up/moved）与 login fleetLoop 线程（sweep_suspended → sweep）并发改 entries_ 无锁 → 迭代器失效/UAF，已消除。recursive 因方法间嵌套调用（session_up→evict_for_relogin、moved→advance_epoch_to、intake_full_report→session_up、sweep→session_down、reconcile→online_ids）会自死锁，普通 mutex 不可。
- [x] `find()` 返回裸指针改为 thread_local 槽副本（player_directory.cpp:314）——map 指针解锁后因他线程 erase 悬空；槽在本线程下次 find() 前有效，全部调用方均立即拷贝字段。
- [x] 回归测试 `modules/game/session/tests/player_directory_concurrency_tests.cpp`：写线程 session_up/evict/session_down/moved × 读线程 online_ids/find/size/sweep 并发 2s 不崩 + 语义保持（顶号/窗口/迁移单线程面全保）；CMake 注册 apollo_game_session_tests（modules/game/session/CMakeLists.txt）。
- [x] 证据：commit 7a531985；build-cov 全量 **118/118 绿**（含新用例，213s）。

### P0-2 TimerManager executeTimer 解锁未持锁 + 回调窗口引用失效（timer_manager.cpp:344-358）

- [x] 修复：`executeTimer` 的 `mutex_.unlock()`（解锁未持有的锁 = UB）删除——回调参数 TimerDesc 拷出、**锁外触发**（参照 modules/base TimerWheel fire_slot 出锁范式）；回调后按 id 重查推进 repeat（回调内 killTimer(selfId) 已删条目则不再重插，尊重回调自停）。`update()`→`processSlot`/`cascadeTimer` 全程不加锁的问题一并封口：wheel 摘桶/timers_ 访问独立加锁（原实现 update() 无锁，并发 setTimer/killTimer 竞态）。
- [x] 附带修复（测试暴露的既有缺陷）：首次 `update()` 从 currentTime_=0 全量回放 epoch 积压（量级 O(uptime_ms)，uptime 196731s → ~1.97e8 tick 自旋，实测挂起 111s+）——`started_` 标志首次 update() 跳到当前拍，后续只处理增量。
- [x] 回归测试 `tests/test_timer_manager.cpp`（直编 timer_manager.cpp，自包含不依赖 legacy apollo lib，绿色模块化树真实执行）：单次触发后移除 / 周期回调内 killTimer 自停（修复前悬空引用 UAF）/ 并发 setTimer-killTimer×update() 1s 不崩；CMake 注册 apollo_timer_manager_tests（tests/CMakeLists.txt，TimerManagerTests）。
- [x] 证据：commit 3ed1eb71；build-cov 全量 **119/119 绿**（202.7s）。

### P0-3 event_loop onCheckTimer 边遍历边 erase + Manager 无析构（event_loop.cpp:578-594）

- [x] 修复①onCheckTimer：超时 fd 先收集 `timedOut`，遍历结束后循环外 `heartbeats_.erase(fd)`——原实现在 `for (auto& [sockfd, info] : heartbeats_)` 体内 erase(sockfd) 致迭代器失效 UB。
- [x] 修复②析构：`~HeartbeatManager()` 移除周期检查定时器 `checkTimerId_`、`~ReconnectManager()` 移除全部活跃重连定时器——两定时器回调捕获 `this`，manager 先死则回调打悬空 this → UAF。
- [x] 回归测试 `tests/test_event_loop_heartbeat_timeout.cpp`：3 心跳同轮超时全部触发恰一次（修复前遍历中 erase 跳过/崩溃）/ 构造-析构管理器后泵过 1000ms 周期 loop 仍存活可用；CMake 注册 event_loop_heartbeat_timeout_tests（EventLoopHeartbeatTimeoutTests）。
- [x] 证据：commit 458560a6；全量 119/120（WorldDrainTests 负载假红 exit134×2、直跑第 3 次全绿——load avg 26-45 环境噪声，本会话未动 world/drain 代码）。

### P0-4 SharedMemoryRingBuffer 环形缓冲区读写竞态 + open() 无容量校验 + 回绕边界差一（shared_memory_channel.cpp）

- [x] 修复①（audit 原文）：`allocate+memcpy+commit` 非原子——并发 `FlatBufferChannel::send` 取到同一 writePos 重叠写、撕裂发布。新增 `write()`（shared_memory_channel.cpp:318）全程持 `writeMutex_`（shared_memory_channel.h:104）串行化 allocate→memcpy→commit，writePos 在 memcpy 完成后才 release 发布；`FlatBufferChannel::send(void*)` 与 `send(BuilderFunc)`（shared_memory_channel.h:135）改走 write()。原 send(BuilderFunc) 只分配 sizeof(uoffset_t)=4 却 memcpy 整个 buffer 且 commit size+4——块头 size 与 writePos 步进记账失配，一并修复。跨进程并发生产者不在保证范围（通道单生产者约定，头文件契约注释写明）。
- [x] 修复②：`open()` 容量校验（shared_memory_channel.cpp:152/186 双平台）——原只查 magic 不查 capacity，读写两端各按自己 totalSize_ 寻址，容量不一致即越界写/SIGBUS。
- [x] 修复③：回绕边界差一——`readPos == totalSize` 恰好接界原 `<=` 误拒（shared_memory_channel.cpp:283）。
- [x] 回归测试暴露并一并修复的同类缺陷（5 个，全带成因注释与测试覆盖）：
  - `readPos==writePos` 判空（read()/isEmpty()）：回绕后 writePos 折回与 readPos 数值重合，已发布消息被永久误判为空 → 改以 count_ 为准判空（:337/:410）；commit 的 count.fetch_add 相应 relaxed→release（:315）与块数据写入同步。
  - 恰好写满 capacity 时 commit 置 writePos=totalSize、consume 却置 0——两端回绕约定分裂，读端重读上一圈陈旧消息并冲过写端前沿 → commit 统一回 0（:309）。
  - 回绕爬升与旧圈未读并存（单 writePos 无法表示两个前沿）：写端爬到 writePos==readPos 时 available 误判整环空闲直接覆盖旧圈 → 回绕前提 count==0 排空门（:263）+ available() 三分支按 count 消歧（:241-249）。
  - 跳过标记写出 data 区界外（残尾 <16B 仍写 16 字节 marker）+ readPos==capacity 幻影块 → 写端残尾 <16B 静默回绕（:268）、读端 read()/consume() 同规则跳回 0（:345/:369）。
- [x] 回归测试 `tests/test_shared_memory_channel.cpp`（APOLLO_ENABLE_IPC 默认 OFF、源码休眠，直编 .cpp 同 TimerManagerTests 范式）：roundtrip+type 传播+超容量拒绝 / open 容量不匹配拒绝 / 恰好接界回绕 / exact-fill 双场景约定一致 / 4 写者×400 条 size-填充模式一一对应并发无撕裂（读者校验 size↔模式配对）。CMake 注册 apollo_shared_memory_channel_tests（tests/CMakeLists.txt，SharedMemoryChannelTests）。
- [x] 证据：commit d05a773b；build-cov 全量 **121/121 绿**（271s，含新用例）；新用例优化构建 100 连绿（40+60，其中 60 连在 4 线程自旋负载下）+ ASan/UBSan 树 5 组全绿 + TSan 10 连绿。
- [x] 范围外备注：`template<typename T> bool send(const T&, uint32_t)`（shared_memory_channel.h:146）为无定义声明，休眠无调用方，本批不动（P0 批只修实缺陷，不扩 API 面）。

### P1-9 QosManager/TokenBucket consume(0) 活锁 + TTL 时间单位错（qos.cpp:246-274, 190-191）

- [x] 修复①活锁：`TokenBucket::consume` 的 timeoutMs==0 语义改为立即失败（原 else 分支 refill+yield 无限自旋——`RateLimiter::acquire` 默认路径 timeoutMs=0，令牌耗尽尤其 rate==0 永不补充时活锁占满核）。签名 void→bool 返回消费结果；`acquire` 原返回 `available()>=permits` 在"恰好消费最后一批令牌"时误报失败，改为直接报告 consume 结果（qos.cpp:255）。
- [x] 修复②时间单位：`send`/`retry` 的 timestamp 统一存毫秒（原存 steady_clock 原始 count——Linux 为纳秒，`cleanupExpired` 按毫秒构造 time_point → sendTime 落在三万年后 → TTL 永不过期，pendingAck_ 只能靠显式 ack 清，超时统计形同虚设）（qos.cpp:31/87）。
- [x] 附带修复（audit P2 + 直编暴露的休眠缺陷，均带测试覆盖）：
  - retry 重新入队不补 `pendingMessages` → getNext 二次出队计数下溢为负（qos.cpp:92）。
  - `send` 记账/追踪错位：`push(std::move(msg))` 后 `sentBytes += msg.data.size()` 恒加 0；`pendingAck_[seq] = queue.top()` 取的是全队列优先级最高者而非本条消息——队列里有更早高优消息时追踪错条目（实测：ttl=50 消息过期清理连带清掉 ttl=0 的后入队消息）。改为拷贝入队、msg 本体留作追踪（qos.cpp:60-68）。
  - 等待时间计算 `needed*1000/rate_` 在 rate==0 时除零 SIGFPE → rate==0 直接睡到 deadline 由超时路径返回（qos.cpp:268）。
  - 休眠树从未编译过（APOLLO_ENABLE_IPC=OFF），直编即暴露并修复：qos.h 缺 channel.h 包含；引用不存在的枚举值（TransportType::Auto→Tcp、ChannelMode::Bidirectional→Client，随 channel.h 实际枚举）；`hasPending` 等 const 方法锁非 mutable mutex_（→mutable）；`TokenBucket::setRate` 锁不存在的成员 mutex_（本类并发由 tokens_ 原子量承担）；channel.h 缺 `<future>`；service_endpoint_ex.h `LocalFirstSelector(const Config& = Config{})` NSDMI 默认实参 GCC 拒编 → 双构造等价改写。
- [x] 回归测试 `tests/test_qos.cpp`（直编 qos.cpp 同 P0-4 范式）：consume(0) 空桶立即失败不自旋 / acquire 默认路径真伪（成功消费最后一批必报 true）+ 默认路径不自旋 / timeout>0 等待补齐与超时耗尽（rate=0 睡到超时、超量请求失败）/ TTL 50ms 实际过期+超时统计+ttl=0 永不过期 / retry 重新入队 pendingMessages 不下溢。CMake 注册 apollo_qos_tests（tests/CMakeLists.txt，QosTests）。
- [x] 证据：commit 552f119c；build-cov 全量 **121/122**（唯一红 WorldDrainTests 负载假红：load avg 12-38 下直跑二进制亦 1/5 abort，该二进制 05:52 构建于本批任何改动之前、低载时 121/121 全绿含本用例——环境噪声既证）；QosTests 30 连绿 + ASan/UBSan 树 5/5。
- [x] 范围外备注：channel.cpp 自身另有 43 处休眠编译错（ChannelBuilder 未声明等，先于本批存在），不属 P1#9 修域。

### P1-10 SessionManager 锁内回调死锁 + sendPacket 部分发送流错位（session_manager.cpp:50-80, 372-392）

- [x] 修复①锁内回调：`internalCloseSession` 改为仅簿记（摘映射/归还 ID/计数）并返回 shared_ptr，close 由调用方出锁后触发——closeSession/closeSessionByPlayerId/kickPlayer/shutdown/update 全路径不再持 mutex_ 调 listener 回调（监听器典型实现重入 manager 查会话/踢人，std::mutex 非递归即自死锁）。update 超时路径同构：锁内只做超时判定快照，onTimeout 与 close 出锁触发，已断会话簿记同轮完成（与原实现同 pass 摘除断言语义一致）（session_manager.cpp:208-283）。
- [x] 修复②流错位：`SessionHelper::sendPacket` 长度头与体合并为单次 send——原两次 send 中第二次失败时长度头已上流、体缺席，对端解析器把后续包字节当体吞掉，整条流从此错位且会话悬活；发送失败即 close(2) 断连（流不可续）（session_manager.cpp:384-410）。
- [x] 回归测试 `modules/net/rpc/tests/session_manager_tests.cpp`（直编 + 链 apollo_net_rpc，补直编 legacy utils/id_pool.cpp 同 tests 惯例；单例以用例体入 lambda + 统一收尾 shutdown 防断言早退漏悬空监听器）：重入监听器（回调内调回 getSession/getSessionCount）四死锁面（closeSession/kickPlayer/update 超时/shutdown）+ 单帧单次发送与长度核对 + 失败断连 + 空载荷 4 字节头帧。旧代码直跑即挂死（timeout 124 实证）。CMake 注册 apollo_net_rpc_session_tests（modules/net/CMakeLists.txt）。
- [x] 证据：commit f51198b7；新用例 10 连绿 + ASan/UBSan 树 5/5（首版测试自身 UAF 被 ASan 抓出并修）；build-cov 全量 **122/123**（唯一红 WorldAppM1LinkTests 负载假红：load avg 33-87 下直跑 4/4 全绿、该目标不链 session_manager/apollo_net_rpc——环境噪声既证）。
- [x] 附带（P1#9 Windows CI 红）：qos.h 缺 `<array>`/`<mutex>`/`<cstdint>`/`<unordered_map>` 等（GCC 传递可得、MSVC 不可），补齐（commit 1365ccde）。

### P1-8 RedisServiceDiscovery stoi 裸抛 + 心跳持锁 I/O（redis_service_discovery.cpp:152-164）——阻塞

- [ ] **阻塞**：该 TU 依赖 `apollo/redis/redis_template.h`（RedisTemplate）——全仓无此文件，源码从未编译过、无测试入口；按「构建/测试绿才算完成」纪律无法验证修复。三处 stoi/stoll 裸抛与心跳线程持 registeredMutex_ 做 Redis I/O 的修复方案已定位（hashToEndpoint L152/158/164 包 try 返默认、heartbeatThreadFunc L550 快照出锁、另发现 stop() L227 持锁调 deregister（L313 再取同锁）自死锁），待 RedisTemplate 依赖落地后一并修。

### P1-5 NativeAdapter 长度头字节序/对齐 UB + 无锁 map 迭代竞态（native_adapter.cpp:179, 816, 1036/1044）

- [x] 修复①长度头：`handleRecv` 原 `*reinterpret_cast<uint32_t*>(peekBuf)` 原生序裸读——非对齐/别名 UB，且小端机上与发送侧（`SessionHelper::sendPacket` 大端编码）字节序互斥，正常帧被判超长直接断连；改逐字节大端组装（native_adapter.cpp:179-189）。
- [x] 修复②map 竞态：`processEvents` 原无锁迭代 `listeners_`/`connectors_`，默认按 CPU 核数开多个 IO 线程并发进入，与 `createListener`/`removeListener` 等持锁变更方构成数据竞争——改持锁快照、出锁处理（避免 `handleAccept`/`handleConnect` 用户回调重入 manager 持锁接口自死锁，同 P1-10 纪律）；`stop()` 收线循环一并持锁（native_adapter.cpp:1049-1078, 826-840）。
- [x] 附带休眠代码：`NativeListener::isRunning()` 头内仅声明无定义，vtable 悬空引用在本测试首次实例化该类时链接期显形——补定义（native_adapter.cpp:422-424）。
- [x] 回归测试 `modules/net/rpc/tests/native_adapter_tests.cpp`（链 apollo_net_rpc，CMake 注册 apollo_net_rpc_native_adapter_tests）：socketpair 对拍大端帧原样送达两帧（长度/内容核验，连接不关闭）+ 超长守卫断连不派发 + 快照路径 update/stop 走通。旧代码直跑 FAIL「two packets delivered」（大端头 00 00 00 05 被小端读成 0x05000000 判超长）——实证。
- [x] 证据：commit 906dd8f4；新用例 3/3 + 30 连绿 soak + ASan/UBSan 树 3/3 + 旧代码实证 FAIL；build-cov 全量重建 exit 0、ctest **123/124**（唯一红 CellRegisterTests 高负载假红：load avg 59-75（外部 Go 构建并发）下直跑 4/4 全绿 + ctest 复跑 1/1 绿，该二进制不链 apollo_net_rpc——环境噪声既证）；build-off（APOLLO_ENABLE_IPC=OFF）/build-examples 全量重建均 exit 0；CI 906dd8f4 六作业全绿（含 Windows Release 与 coverage）。

### P1-6 WebSocket 接收缓冲无上限/O(n²) 擦除 + 单次 recv 握手 + 裸指针 getConnection（websocket.cpp:662, 823, 864）

- [x] 修复①缓冲：`handleReceive` 原 `receiveBuffer_` 无上限——帧声明长度（8 字节扩展长）攻击者可控，未完成帧可无限累积（任意客户端打 OOM）；且每解析一帧 `erase(begin,...)` 首部搬移 O(n)，N 帧即 O(n²)。改偏移消费（parse 自 `data()+consumed` 起），仅当前缀占比过半或已大块时压缩一次（分摊 O(1)）；未消费余量超 `config_.maxMessageSize` 即断连（websocket.cpp:664-694 服务端、1161-1185 客户端）。
- [x] 修复②握手：服务器 `handleClient` 原单次 `recv`——请求分片到达时拿到不完整头即判定无效握手断开；改循环收至 `\r\n\r\n`（头满或缓冲满视为无效），缓冲上限 4096（websocket.cpp:885-907）。
- [x] 修复③getConnection：原返回裸指针，连接表并发 erase 后调用方悬空；改返回 `shared_ptr`（接口 websocket.h:331 + 实现同步；仓内无其他实现/调用方）。
- [x] 附带死锁：`WebSocketServer::stop()` 原置位后 `join()` 永等——acceptLoop 阻塞在 `accept()` 不因 running_=false 或关闭套接字解除；监听套接字改非阻塞，acceptLoop 以 10ms 周期检查 running_（websocket.cpp:794-806）。
- [x] 回归测试 `modules/net/websocket/tests/websocket_tests.cpp`（链 apollo_net_websocket，CMake 注册 apollo_net_websocket_tests）：回环 TCP 直连服务端——正常文本帧往返（偏移消费重构后单帧完整送达）+ 分片握手（50ms 间隔两段，原单次 recv 即败）+ 声明 256MB 帧流灌超 maxMessageSize 断连（OOM 面）+ getConnection 未启动返回 nullptr。旧逻辑（仅补 stop 死锁与非阻塞 shim、隔离审计面）3/3 直跑确定性 FAIL「oversized frame closes connection」（连接悬活不断）——实证。
- [x] 证据：commit bf6fa876；新用例 4/4 + 30 连绿 soak + ASan/UBSan 树 4/4 + 旧逻辑实证 FAIL；build-cov 全量重建 exit 0、ctest **125/125** 全绿（含新增两用例）；build-off（APOLLO_ENABLE_IPC=OFF）/build-examples 全量重建均 exit 0。

### P1-7 内置 HTTP 连接线程悬空/泄漏 + 客户端无超时无上限 + 解析抛异常 + url 忽略（http.cpp:537, 629, 756）

- [x] 修复①连接生命周期：连接线程原 detach 且闭包捕获裸 `this`，server 析构后 handler 访问 `router_` 悬空（UAF）；`connections_` 只增不减（`removeConnection` 无人调用）每连接泄漏 shared_ptr。改连接线程持 router/registry 的 shared_ptr（经共享状态访问，闭包不捕获 this），线程退出统一 `registry->remove`（http.cpp:728）；`stop()` 收线：置 shutdown 标志 → 连接线程 select 周期退出（100ms）→ join 全部连接线程（原阻塞 recv 无法收线，http.cpp:679）。监听套接字非阻塞，acceptLoop 周期检查 running_。
- [x] 修复②客户端响应超时/大小上限：`receiveResponse` 原对端不发即永久阻塞 recv（可挂死）、响应无上限（可 OOM）。改 `connector_->waitReadable` select 超时（http.cpp:879）+ 达 `maxResponseSize_` 即停（http.cpp:873，默认 16MB）。
- [x] 修复③解析抛异常：`Headers::contentLength` 与 `RequestParser` 对垃圾 `Content-Length` 原 `std::stoll` 抛异常（服务端连接线程未捕获 → terminate；客户端未捕获冒泡）——包 try 按无体处理（http.h:245, 511）。
- [x] 修复④`httpGet`/`httpPost` 原忽略 url 恒连 localhost:80——`parseUrl` 按 url 解析 host/port/path（https 拒绝返回 Invalid URL，http.h:662-666）。
- [x] 附带 RequestParser 字节多计：头部行原先 `bytesConsumed += line.size()+2` 再弹 `\r`，`line.size()` 含 `\r` 又补 `\r\n` 每行多计 1 → `bytesConsumed` 越界 → `receiveBuffer_.substr` 抛 out_of_range → 连接线程 terminate。改先弹 `\r` 再计（http.h:483-486）。
- [x] 回归测试 `modules/net/http/tests/http_tests.cpp`（链 apollo_net_http，CMake 注册 apollo_net_http_tests，CMakeLists.txt:300-315）：GET/POST 往返 + 连接关闭后表项移除 + stop() 快速收线（<1s）+ 静默对端响应超时 + 垃圾 Content-Length 客户端不抛 + 超大响应封顶（4KB 上限收 64KB 体）+ 服务端垃圾 Content-Length 不崩 + httpGet url 解析生效 + https 拒绝 + 接口面可构造。
- [x] 旧逻辑实证 FAIL（/tmp/http-old，git HEAD 源码 + 新用例）：①旧 HttpServer 往返 abort exit 134（substr out_of_range → terminate）；②旧 HttpClient 对端静默阻塞 recv，超时杀进程 exit 124——确定性 FAIL，回归测试为真。
- [x] 证据：commit c1ec9a7d；新用例 10/10 + 30 连绿 soak + ASan/UBSan 树 10/10（无泄漏）+ 旧逻辑实证 FAIL；build-cov 全量重建 exit 0、ctest **125/126**（唯一红 WorldAppM1LinkTests 高载 40-63 时序 flake，独立重跑 20.55s 绿，非本批回归）；build-off（APOLLO_ENABLE_IPC=OFF）/build-examples `apollo_net_http` 重建均 exit 0。

### P1-11 协议解码网络面裸抛（codec.cpp 坏 JSON/残帧未捕获，dispatch 路径炸进程）

- [x] 修复①链路分发兜底：`Dispatcher::dispatch` 原裸调 handler（dispatcher.cpp:25）——handler 内 `decodeBody` 的 `nlohmann::json::parse`（parse_error）与 `throw std::runtime_error("Invalid data size"/"body truncated")` 无捕获，来自对端的残帧/坏 JSON 冒泡出传输层即进程崩溃（gateway ingress / 服务器间 link / m1_egress / 在线目录 / 玩家库等全在 link Dispatcher 收口）。改 handler 与 fallback 调用包 try/catch，异常按帧拒收：新增 `DispatchResult::kHandlerError` + `Stats::handler_error` 计数（dispatcher.cpp:27-45、dispatcher.hpp:25,57；"错误是数据不是控制流"内核纪律：不抛不静默）。
- [x] 修复②网关客户端面：`GatewayServer::handleClientMessage`（gateway_client.cpp:36）原无 try——`decodeBody<Ping>`（:57）对坏 Ping 体裸抛冒泡出宿主泵线程炸进程；其余解码（:161/:269/:303/:380）已在既有 try 内。改 switch 整体包 try/catch，异常记日志 + `removeSession` 断连兜底（gateway_client.cpp:119）。
- [x] 覆盖面复核：login `dispatchRequest`、gateway ingress `session_admission`、world `CellApp::executeIntent`（drainAcceptanceQueue try/catch）、`database_client`、`login_server_agents`（调用方 try）均已自带 try/catch——本批补上仅剩的 link Dispatcher 与 gateway_client 客户端面两处，全网络解码入口齐备。
- [x] 回归测试：`tests/test_net_link.cpp` `test_dispatcher_handler_error`（:301，注册 :1662）——handler 抛 runtime_error → `dispatch` 返 `kHandlerError` + `handler_error` 计数、不崩；fallback 抛同收拒；正常 handler 不误伤（不计数、仍 `kDispatched`）。
- [x] 旧逻辑实证 FAIL（/tmp/dispatcher-old，git HEAD dispatcher.cpp + 抛异常 handler）：旧 dispatch 裸调 handler → 异常未捕获 → `std::terminate` → Abort exit 134（确定性 FAIL）；新 dispatcher.cpp 同 proof exit 0（按异常拒收不崩）。
- [x] 证据：commit ae2873e6；新用例 link_tests **41/41**（原 40+1）+ 旧逻辑实证 FAIL + ASan/UBSan 树 link_tests 41/41（无泄漏）；build-cov 全量重建 exit 0、ctest **126/126** 全绿（168s，WorldAppM1LinkTests 本次过）；build-off（APOLLO_ENABLE_IPC=OFF）/build-examples `apollo_net_link` 重建均 exit 0。

## Codex 深审发现（2026-11-12，luna profile 近5 commit 审核，3C/4M/4m，待修）

- [ ] [C] m1_egress 翻译路径加异常兜底与坏帧测试（apps/gateway/src/m1_egress.cpp:329）——泵线程可被客户端残帧终止（forwardIntent 投递后 translate_and_send 裸调 MessageCodec）
- [ ] [C] HTTP Content-Length 回绕防护与服务端大小上限（include/apollo/net/http.h:525、modules/net/http/src/http.cpp:454）——data.size() >= headerEnd+contentLength 无符号回绕为 0，协议混淆+OOM 面
- [ ] [C] 统一 POSIX socket send 防 SIGPIPE 策略（modules/net/tcp/src/socket.cpp:122）+ native_adapter 三处（:49/:235）——对端关闭后写=SIGPIPE 杀宿主进程，同型残留
- [ ] [M] 重构 Base forward 回调所有权与 stop 排空顺序（apps/gateway/src/gateway_server.cpp:242）——回调捕获 this 访问 messageRouter_，停机 UAF
- [ ] [M] HTTP 路由异常捕获返回 500（modules/net/http/src/http.cpp:723）——用户路由抛异常穿出连接线程终止进程
- [ ] [M] OnlineDirectory::dispatchRequest 异常转错误响应（apps/common/src/online_directory.cpp:238）——关闭 login 泵线程崩溃入口
- [ ] [M] 修正 LengthPrefixPacketParser 字节序与长度加法检查（include/apollo/net/adapters/native_adapter.h:580）——跨端帧长误读
- [ ] [M] 收紧 HttpClient 响应长度解析与状态码映射（modules/net/http/src/http.cpp:904）——客户端封顶回绕与无效码截断
- [ ] [m] 明确 HTTP URL 能力边界或实现域名/IPv6（modules/net/http/src/http.cpp:1000）——消除"可解析但不可连"接口不实
- [ ] [m] 补 GatewayServer::handleClientMessage 坏帧断连测试（apps/gateway/src/gateway_client.cpp:53）——本批修复入口回归保护
- [ ] [m] 更正 todo 覆盖面声明并登记解码入口清单（todo.md:583）——避免安全审计记录虚报
