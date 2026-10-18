# Apollo 实施清单

目标职责与程序短名：gateway、login、base、world、database、machined；cell 是 world 同一二进制的执行角色。world 管理执行进程与 Space/Cell，cell 执行模拟；Cell 是空间分区，不是进程。Space 对应现有 Scene 的空间语义，Instance 保留临时玩法生命周期，不增加同义容器或擅自改代码名。

依据：[BigWorld 与 Apollo 对照](docs/design/bigworld-apollo-mapping.md)、[架构](docs/guide/architecture.md)、[当前决策](docs/architecture/adr.md)。按下面依赖顺序实施，每项以真实验收勾选；库原语或旧拓扑测试不能代替最终接线。跨服模拟不改变原区服和存储归属；Base 承载可随负载或更新独立迁移，按玩家身份解析当前归属。当前不启用无缝地图，接口预留未来扩展。

## P0：文档与命名

- [x] 建立 BigWorld 原词 → Apollo 角色 → 现有代码 → 实现缺口的中英文对照；保留真实第三方名称，清除冲突目标，不保留历史备选。
- [x] 明确 worldServer/CellAppMgr、cell/CellApp、Space/Scene、Cell 分区及 Instance 玩法边界；默认一个执行进程，每 Space 一个 Cell。
- [x] 明确 Base 长期对象、Real/Cell 实体、Witness 部分原语及 Ghost 预留，不声称完整 BigWorld 已实现。
- [x] 明确注册、拓扑权威与位置投影，machined 只监督本机，World/cell 独立部署不要求同机 machined。
- [x] 区分支持协议更新的进程替换与仅代码更新；Base、cell、World 管理端各交接不同状态，TCP/会话/活动连续分别验收。
- [x] 保持 VitePress 原生 locales：中文根目录、英文 en，各自正文与导航，新增对照入口并纳入文档 CI。
- [x] 专用 Profiler、OpenTelemetry SDK/OTLP exporter 标为未集成，自研日志、MetricRegistry、span/trace 不作为集成证据。

## P0.1：角色实体与应用命名迁移（待实施）

依据：[玩家对象模型与行业引用](docs/design/player-object-model.md)、[World/cell 设计草案](docs/design/world-cell-management.md)。本节按本轮讨论登记，尚未修改代码或验收；下面章节中的旧程序/类名描述当前实现，实施后同步清理。行业通用语义为角色实体的 Base/Cell 部分；具体 C++ 命名空间是 Apollo 的实现约定，不宣称是上游原名。

- [x] 固定代码迁移表（todo L21 + docs/design/player-object-model.md §命名 L30 双源钉死）：`apollo::game::session::PlayerAnchor` → `apollo::game::base::Avatar`；`apollo::game::world::Avatar` → `apollo::game::cell::Avatar`。同一角色两职责部分，不新增第三个 Avatar 容器、不合并客户端 Session、通用 Entity/NPC 不机械改；实施后删过渡映射不留两套名（L22-L30 落地）。
- [x] Base 部分头源与命名空间已迁到独立 Base 模块（modules/game/base，`apollo::game::base` / target `apollo::game_base`，链接 `apollo::game_core`；`apollo::game_session` PUBLIC 链接 base，消费方经传递获得）。迁出四组：`player_anchor.{hpp,cpp}`→`avatar.{hpp,cpp}`（PlayerAnchor→Avatar，AnchorState→AvatarState，SessionBinding 随宿主类同迁）、`anchor_manager.{hpp,cpp}`→`avatar_manager.{hpp,cpp}`（AnchorManager→AvatarManager，AnchorPtr→AvatarPtr，anchor_count→avatar_count）、`anchor_reward_sink.hpp`→`avatar_reward_sink.hpp`（AnchorRewardSink→AvatarRewardSink，kExpKey 语义不变）、`recovery.{hpp,cpp}`→base 同名（RecoveryCoordinator/Tier/Phase/recovery_manifest 随迁，restore_anchors→restore_avatars）。另随迁 `world_assignment.hpp`：无依赖值类型被 Avatar 按值持有，留 session 会造成 base→session 反向依赖（已代决）。session 模块保留 locator/directory/directory_mirror/fleet_recovery 四件套。附属符号同清：set_anchor_resolver→set_avatar_resolver、AnchorResolveFn→AvatarResolveFn、ApplyError::AnchorUnavailable→AvatarUnavailable、PlayerStore::avatar_manager()/flushDirtyAvatars/activatePlayerAvatar/findAvatar、测试目标 test_base_anchor→test_base_avatar（BaseAnchorTests→BaseAvatarTests）与 test_*_anchor_* 用例名/注册串同步。CMake：base 模块新增、session 减三源、tests 与 examples 直编源列表改指 base 并补 base include 目录。同态设计确认：`apollo::game::base::Avatar` 与 `apollo::game::world::Avatar`（cell 侧，L23 再迁 `apollo::game::cell`）各自完整命名空间并存，冲突文件全限定 + 局部量 `base_avatar` 区分。术语保留（已代决，跨契约/无关概念不并入）：`anchor_epoch`/`old_anchor_epoch`（契约字段 messages.xml + 生成物）、`EntityDomain::Anchor`（net 传输面枚举）、runtime `--demo-anchors`、Lua registry anchor 注释。门禁：build-off ninja 全量构建 + ctest 86/86、/tmp/apollo-san gmake 全量 + ctest 85/85、build-examples 86/86、examples/full_loop_demo 全链路断言全过。
- [x] Cell 部分头源已迁到独立 Cell 模块（modules/game/cell，`apollo::game::cell` / target `apollo::game_cell`）：`modules/game/world/src/avatar.cpp` + `include/apollo/game/world/avatar.hpp` → `modules/game/cell/src/avatar.cpp` + `include/apollo/game/cell/avatar.hpp`，`apollo::game::cell::Avatar/AvatarState/AvatarPtr` 三符号随迁（仅依赖 game_core + game_base，cell→base 单向）；world 模块删 avatar.cpp 并 PUBLIC 链接 `apollo::game_cell`（+`apollo::game_base`），`modules/game/CMakeLists.txt` 注册 cell 于 world 之前。引用同步：world/scene.hpp+scene.cpp 的 AvatarPtr/AvatarState、scene_transfer.cpp 重建 Avatar 两处、instance.hpp/world.hpp include、world-app 装配处、5 个测试目标 with using-directive、2 个 example —— 全部改为完整命名空间 `apollo::game::cell::`，world 模块文件不用同名全局 using 抹边界。CMake 直编源列表（tests 与 examples）改指 cell 并补 cell include 目录。无 `class Avatar` 前置声明、无 install(EXPORT) 规则需同步。门禁：build-off ninja 全量 + ctest 86/86、/tmp/apollo-san gmake 全量 + ctest 85/85、build-examples 86/86、full_loop_demo 全链路通过。
- [x] 类型引用全量更新已随 L22/L23 完成（PlayerStore/apps/common、WorldApp、Scene/Instance、迁移 migration_wire+scene_transfer、恢复 recovery、奖励接收端 squad_instance set_avatar_resolver/AvatarResolveFn、全部测试与 examples）。命名空间口径核验（3802d827+a32dec42 后复查）：`using apollo::game::base::Avatar;` 全仓 0 处——base 侧一律完整限定 `apollo::game::base::`；`using apollo::game::cell::Avatar;` 仅 6 文件（test_scene/test_reconnect/test_avatar/test_scene_attrsync_wiring + benchmark/full_loop_demo），单别名单 Avatar 可见、base 侧不进 using，无同名双 using 抹边界文件。Lua 桥（modules/script + world-app contract_handlers.lua）无 Avatar 类型引用残留；modules/contract 测试中 entity id "Avatar" 为契约实体树夹具名（无关同名，保留）。结构性数据/进程拆分未随改名虚报——仍按 P1/P2 条目另行实施。门禁：build-off 86/86、san 85/85、build-examples 86/86。
- [x] 迁移对稳定标识零影响（3802d827+a32dec42 diff 复查）：PlayerID/PlayerId/EntityId 强分型未动；PlayerData 及存档 JSON 键未动（database_service.hpp 0 diff）；sdks/contract/messages.xml 0 diff（方法编号 933-938/958-959 等全保持）；modules/protocol/messages.hpp 唯一改动是注释（AnchorState 字符串 → AvatarState 字符串）；wire 状态串值逐字节保持（toAvatarStateString：loading/online/transferring/disconnected/saving/offline，与迁移前 toAnchorStateString 输出一致）；anchor_epoch 契约字段保留原值语义。无兼容别名/转发头（`using PlayerAnchor|using AnchorManager|using AnchorState` 全仓 0 处）——旧类名不残留。无契约演进，无需版本/读取规则条目。门禁：build-off 86/86、san 85/85、build-examples 86/86（MigrationChainTests/WorldAppAcceptanceTests 等迁移链 e2e 全过 = wire 语义回归证据）。
- [x] 应用目录与目标已统一：`apps/gateway-app`→`apps/gateway`（target gateway-app→gateway）、`apps/login-app`→`apps/login`（login-app→login）、`apps/base-server`→`apps/base`（baseServer→base）、`apps/world-app`→`apps/world`（world-app→world）；database/machined/logger/admin/fleetctl 名实一致未动；`apollo_package_crash_handler` 随目标改名重挂（build 输出确认 crashpad handler 打包跟新名）。`apps/game-server`（169 行 runtime service-host 演示 main，职责被 examples/game_server_demo 与真宿主覆盖）连同 dev_fleet roster 行与 apps/CMakeLists 条目移除；空壳目录 apps/base-app、apps/baseappmgr（无 CMake 无源码，git 未跟踪残留）清理。引用同步 33 文件：tests/CMakeLists（含直编源路径与 base_database_process_smoke.py）、tests/store_host.hpp、5 个 drill 脚本、dev_fleet.sh、examples/CMakeLists；CI workflow 无二进制名引用（无需改）。无 install/CPack 规则（无安装面可改）；无别名/包装脚本残留——旧 `-app`/`baseServer` 名全仓 0 命中（apps/scripts/tests/examples 扫描）。门禁：build-off ninja 全量 + ctest 86/86（一次 -j8 磁盘配额抖动重跑绿）、/tmp/apollo-san gmake 全量 + ctest 85/85、build-examples 86/86、full_loop_demo 通过。
- [x] world 共用二进制分角色：无参数或未指定角色默认管理端，`world --role=cell` 启动执行端；错误角色/缺值明确拒绝，cell 不递归派生。具体自动拉起、扩缩容与监督接续按 P2 管理/执行设计实现并验收，不随改名虚报完成。落地：`apps/world/include/world_app/role_options.hpp` 纯解析头（`--role <v>`/`--role=<v>` 两形态，只认 manager/cell，未知值与缺值返回 false+err，重复指定后到覆盖）+ `tests/test_world_role.cpp` 四用例（WorldRoleTests）；`main.cpp` 入口拒绝即 `world: <err>` + exit 2，help 首行列 `--role`；`config.hpp` 加 `role` 字段、Configuration 日志块输出 `Role: <role>`。真机验证：`--role=worker`/末位 `--role` 均 exit 2 且报错带出值；默认启动日志 `Role: manager`、`--role=cell` 日志 `Role: cell`。cell 不递归派生（入口零 spawn 代码）；职责切分未虚报——两角色现阶段同栈启动（WorldApp 全量即执行面），role 仅入账（config+日志），自动拉起/扩缩容/监督随 P2。门禁：build-off ninja 全量 + ctest 87/87（新增 WorldRoleTests）、/tmp/apollo-san gmake 全量 + ctest 86/86、build-examples 87/87、full_loop_demo 全链路通过。
- [x] 将 Base 离线激活、并发登录复用已有对象、动态迁移与当前归属解析列为后续行为验收：离线不创建第二写入者；支付/结算按稳定玩家身份投递，订单不永久绑定 Base 进程 ID；资料归属不等于进程不变。存储写权隔离与结算幂等依赖 P1/P3/P4，不由命名本身保证。登记核账（本条目为登记项，四行为均已在后续条目显式列为行为验收，映射如下，无新增镜像测试——L30 明令不为纯改名新增镜像实现的测试）：Base 离线激活/离线不创建第二写入者 → P0.3「Base 唯一激活/写权……覆盖通知重复/乱序、回执丢失、离线激活、并发登录、Base 迁移及接口/存储重启」+ P3 登录串联（Base 经 database 激活、未进玩法保有 Base 对象）与重复登录（恢复原 Base、不创建第二个资料写入者）；支付/结算按稳定玩家身份、订单不绑定进程 ID → P0.3「稳定玩家/角色身份关联订单、查询当前 Base 与归属代次、不永久绑定 Base ID/IP」+ P4 逐 Base 幂等结算（稳定玩法/结算 ID、未持久确认唯一结算记录、失败取消不发成功奖励）；动态迁移/资料归属不等于进程不变 → P5 Base 更新（排平/交接/写权代次/路由刷新/隔离旧写）+ P4 场景迁移逐步确认（禁止双端解冻）+ P3 代次分开（进程代次/资料写入代次/实体权威代次）+ 导语「Base 承载可随负载或更新独立迁移，按玩家身份解析当前归属」；当前归属解析 → P3 注册逻辑身份与 gateway 缓存失效/迟到投影/重定向可恢复。依赖如实挂账：存储写权隔离 = P1（写入代次线已落地，PlayerData 拆分仍未完），结算幂等 = P3/P4 未实施；docs/design/player-object-model.md 明示「本轮文档更新不代表代码已完成该迁移」，L22-L27 勾账均带未虚报声明——无以命名冒充行为完成处。
- [x] 同步中英文词汇表、架构图、类型映射与开发说明；迁移完成后删掉旧目标名称、旧类型名、过渡对照和死链接，不保留生产代码别名/转发头或两套命令。第三方引擎源码原名和必要的协议兼容规则保留真实含义，不误清理外部引用。zh/en 双语 51 文件同步：类型旧名（PlayerAnchor/AnchorManager/AnchorState/AnchorRewardSink/AnchorPtr）在现行文档清零——player-object-model 改写为定稿命名（base::Avatar=Base 部分、cell::Avatar=Cell 部分、依赖 cell→base→core）并删「尚未迁移/本次不改类名/过渡映射」过渡表述；concept-glossary 三行与角色实体段、api/game 类型清单与 PlayerId 注释、modules/game 九头清单改为 base 五件+session 四件并换新类型名、world-cell-management/apps-index/bigworld-apollo-mapping/guide-architecture/architecture-adr/base-cell-proxy/bigworld/starter/attribute-sync 行内同步。目标名与路径：apps/gateway-app→apps/gateway、apps/login-app→apps/login、apps/base-server→apps/base、apps/world-app→apps/world；命令示例改真实路径（./build-examples/apps/base/base、./build/apps/world/world --help、apps/world/scripts/contract_handlers.lua），Directory_Structure 双语应用表按实仓 apps/ 重写（含 admin/fleetctl/logger），ServiceHost 使用方改指测试侧（runtime_comprehensive_tests/test_runtime）。过渡对照已删：world-cell-management 双列迁移表收敛为单列现状表并改为已完成表述。状态句按已验收事实校正（结算受理已接、资料归属已切 base、有界保存队列/存储路由/写入代次已落地，目录归属/完整跨进程结算链/存储分区仍待迁移）。架构图 ArchitectureDiagram.vue 0 旧名命中无需改；全库相对链接与路由链接复查 0 死链（TimerId/int 为代码误报，./quick-start 为 vitepress 路由写法）。35 号执行清单双语补历史件标注（沿用 34 号格式），apps/game-server 相关未勾条目按原样留档不再执行。保留项（第三方/必要兼容，已代决）：BigWorld/KBE/Agones/SSengine/Unreal/Unity 原名不动；baseServer/loginServer/worldServer 作为角色词汇保留（本清单自身沿用该词汇）；apps/game-server 与 PlayerAnchor 仅在历史标注与命名沿革句中以「已移除/已弃用」事实出现；quick-start/runtime 的 game-server 为用户自建示例名；baseappmgr 域术语保留（活跃测试目标 baseappmgr_tests 在册、目录归属随 P3 条目清理，与遗留测试目标处置待用户拍板）。生产代码无别名/转发头、无第二套命令（仅注释措辞改动）。门禁：docs build 两次绿（102.83s / 96.35s）；注释改动后三树 build-off 87/87、/tmp/apollo-san 86/86、build-examples 87/87 + full_loop_demo 全链路绿；一次 WorldAppAcceptanceTests 失败根因为本会话早先冒烟残留的 world 进程占 9101，清除后隔离与全量重跑均绿；BaseDatabaseTests 一次 -j4 负载抖动，隔离与全量重跑均绿。
- [ ] 验收真实默认配置与 game 模块配置的构建/测试，覆盖 Base 激活/恢复/持久化、Cell 入场/离场/模拟、结算和迁移；Windows/Linux/macOS CI、双角色真进程启动及文档国际化/构建/内链均通过。按 git 文件清单复查旧名残留，只有对应实现和清理完成才勾选，不为纯改名新增镜像实现的测试。

## P0.2：项目初始化 CLI 与开发脚手架（待实施）

现状：仓库有 CMake、scripts/dev_fleet.sh、编队运维和 CI，但现有服务 CLI 没有 init 项，不能称为已交付项目初始化。参考 [Mistlane 核对](docs/analysis/mistlane-review.md)的实际入口，按 Apollo 目标角色生成，不搬旧项目的角色名或游戏业务。

- [ ] 明确 init 子命令的唯一 CLI 入口、默认模板和帮助说明；区分运行参数、项目生成与测试/压测工具，不向每个服务复制一套初始化器，不把 CI 描述成在线服务。最终入口统一后同步可执行目标、发布包和教程。
- [ ] 生成最小项目的配置、Lua 入口、契约入口、目录说明、忽略规则和启停脚本：gateway/login/base/world/database，world 自动维护 cell；machined/独立 interfaces 按部署需求启用。不生成生产凭据或伪造的支付验签实现。
- [ ] 统一项目根路径和逐实例运行目录；模板不依赖启动时 cwd，角色、端口、构建版本、日志/资料/管理记录路径明确，多个 cell 不共写目录。
- [ ] 提供有真实 readiness 的最小编队启动/停止/状态命令；处理依赖失败、超时及只回滚本次新启动成员，保留已有实例，避免固定 sleep 冒充就绪。
- [ ] 生成可审查的 CMake/CI 配置与开发验证入口，覆盖构建、契约生成、测试和文档；CI 由仓库 workflow 执行，不装入业务处理循环。公共引擎 CI 与生成游戏项目 CI 分开验收。
- [ ] 验收非空目录拒绝覆盖、非法名称/路径、从其他 cwd 启动、文件权限、无秘密配置、最小客户端登录/进入玩法、停止清理及失败回滚；模板完成前文档标为待实现。

## P0.3：两类外部入口与图文接线（待实施）

- [ ] 游戏 gateway 与 HTTP interfaces 均支持按监听独立启用 WS/WSS，允许多个监听及明文/TLS 共存；游戏 WebSocket 进入游戏会话，HTTP 服务的 WebSocket 使用专用 upgrade 路由和连接生命周期，普通 HTTP 请求仍走 HTTP handler。补证书、端口冲突、路由隔离及并行监听验收。

- [ ] 游戏 TCP/WS/WSS 接入接到 gateway 的同一游戏帧/会话/意图路径，分别路由 Base 与 cell；WebSocket 升级不进入 interfaces 的 HTTP 业务 handler，内部协议与传输封装分离。
- [ ] 为游戏 WS/WSS 补客户端消息到 Lua/场景逻辑、稳定 session 寻址下行、服务端主动推送/广播、AOI 同步、有界积压、慢读处理及重复/乱序规则；不能以单次 echo 请求/响应或已有通用 websocket 库作为完整接线证据。
- [ ] 分别验收 TCP、WS、WSS 的登录、Base 业务、cell 输入/异步下行、断线重连和迁移路由刷新；WSS 覆盖证书链/主机名错误拒绝和密钥配置，协议更新与 TCP/WS 会话连续分别记录。
- [ ] 独立 interfaces HTTP/HTTPS 入口用于平台账号/支付等服务端回调；保留必要原始请求体以按渠道规范验签，设置请求体/并发/超时预算，Lua/业务执行进入自己的有界单写者受理链，不阻塞游戏模拟。
- [ ] 实现订单/通知可靠受理与持久待办，确认收到不等于发货完成；通过稳定玩家/角色身份关联订单，再查询当前 Base 和归属代次。Base 不可达、玩家离线、迁移和升级期间都能恢复投递，不永久绑定 Base ID/IP。
- [ ] Base 唯一激活/写权、订单校验、奖励与发货去重凭据原子持久提交，覆盖通知重复/乱序、回执丢失、离线激活、并发登录、Base 迁移及接口/存储重启；当前 DB_LOAD/DB_SAVE 原型不能代替订单事务。
- [ ] 首页及架构图分别展示游戏 WS/WSS、HTTP 回调、动态 Base 查询与持久发货；区分运行进程、CLI 工具和 CI，并标注实现/规划。相关图只随真实验收更新状态，不因画出节点而宣称能力完成。

## P1：独立 Base 与 database

- [x] 完成 baseServer 在默认编队中的资料归属（205cac3d）、基础业务与结算迁入（4fe7a014）；独立入口已具备，离场/Instance 销毁不卸载长期资料已验收（857847e9：成员中途离场 + 成功线销毁三段后，锚点仍可解析、存量分值原样、在场者结算分值照常落账；真实 Base 侧销毁断言随 squad_wire_e2e——Destroyed 后名册锚点全 Online + killer exp > 0；World 域无卸载通道：PlayerStore 无卸载 API，唯一写口 = 958 结算入账）。
- [x] 基础业务与结算迁入落地（4fe7a014）：契约 958/959 player_score_credit request/reply + business_wire 编解码 + PlayerStore M1 dispatch 受理（accepted=0 语义 = 无锚点/Transferring 挂账拒绝）；SettlementLedger 增 BusinessSink 出口（Ok=applied 收口，其余挂账保留分值 retry 同路径重提——幂等归调用方账本，Base 只受理不记重放）；WorldApp 装配 StoreDial（--base-business-host/port，dev_fleet 拨 9201，面关 = 结算挂账降级态）；squad_wire_e2e 锚点落账断言回归 Base 侧（killer exp > 0）+ 专项 BaseBusinessWireTests 五步（常规落账/无锚拒绝/冻结拒绝/账本幂等双投/面停挂账）；contract_handlers.lua 补 958/959 占位行过同步闸。门禁：build-off 全量 86/86、san 85/85、build-examples 86/86。
- [x] baseServer/database 进默认编队先落地：dev_fleet.sh 拉起 database 9200（data-dir/journal 落编队运行目录）+ baseServer 9201（存储后端 database 9200，存储不可达即拒起、由 machined 退避重试；编队位置在 database 之后）。进程级验收：双成员 ready 且无退避重启、--probe 经编队内 database 真存真读（player_900001.json 落盘读回）、machined 监督 7 成员在列（重启抖动仅遗留 game 占位项，非本次引入）。双进程尚无 machined 成员注册（号=0）。剩余：基础业务与结算迁入、离场/Instance 销毁不卸载长期资料验收。
- [x] 资料归属切换落地（205cac3d）：WorldApp 弃档案宿主——store/报告面/结算锚点解析器/credential 目录全撤（config/main/gameLoop 同步清场），档案宿主 = baseServer 9201（dev_fleet 档案客户端拨 9201）；baseServer 装备 store 报告面（--report-host/port/component-id/zone-id/interval-ms）+ collect_mirror_entries 收集面（zone = 承载域、Transferring 过滤）；六测试套重构（hosted_services/fleet_rehome/migration_chain 以 StoreHost 替身同口径装配、squad_wire_e2e/world_drain/update_continuity 弃档案种子——结算挂账为设计降级态，收尾不阻塞，锚点落账断言随基础业务迁入回归 Base 侧）。门禁：build-off 全量重建 85/85、san 十套受影响 ASan+UBSan 绿、build-examples 全量重建绿。
- [x] 完善 database 的有界异步保存队列（磁盘 I/O 移出 RPC 泵线程）、存储路由与写入代次（三款全落地）：有界队列 + worker 面 = 446626fd（满载背压拒收不丢数据）；存储路由 + 串行化 = 499111be（PlayerStorage 注入路由本地/远程，单进程 storeMutex_ 串行 + SaveQueue 按玩家分槽 FIFO）；写入代次 = database 半（writeGen 单调校验/同代幂等/低拒）+ Base 半（6eada1b7 写端自持代次账）。不以 MySQL 为前提（文件档案 + journal）。
- [x] 有界保存队列先落地（446626fd）：SaveQueue 排队+在途容量上限（saveQueueCapacity，默认 4096，0=不设界），满载背压拒收不丢数据——载荷 journal 先段，脏位/下轮 flush 重取；磁盘 I/O 已在独立 worker 面（P1-4）、存储路由与写入代次已随 PlayerStorage 接口与 journalSeq 水位落地（499111be）。满载拒收 1 例新增；build-off 85/85、build-examples 85/85、san 84/84 无发现、drill 全链断言全过。
- [ ] 从当前 WorldApp/模拟宿主移出完整档案、长期写权和直接存储依赖；定义只读玩法快照与返回业务请求，禁止跨界使用本地 Anchor 指针修改资料。
- [x] Base 串行修改资料，database 对玩家保存排序并验证写入代次/版本，拒绝旧进程与旧快照覆盖。database 半已随 L23 落地（单调校验：高受理/同代幂等/低拒、无版本兼容受理不清地板、重启地板从档案恢复——test_base_database write_generation_ordering 七步）；本次补 Base 半（6eada1b7）：PlayerStore 写端代次账 writeGens_ 激活首载播种（重启续代）+ 四个落库点盖章（flush / handleDbSave / freeze 排平 / adopt 接权），接管后旧进程迟到写（代次 ≤ 地板）被 database 半拒、长期资料不动（base_write_generation_self_managed：落库 gen 1→2→旧 gen 1 拒）；Base 串行 = storeMutex_ 内态判定+入段+入队同一临界区，database 单 operations 锁 + per-key journal 序。门禁：build-off 86/86、san 85/85、build-examples 86/86。
- [x] 单进程存档串行化先落地（499111be）：PlayerStore 以 storeMutex_ 为单一串行点，自动保存/RPC 受理/保存回调/迁移冻结四线原子交错，落盘 I/O 全在锁外；SaveQueue 按玩家分槽（playerId % 槽数）同玩家 FIFO 落盘、在途合并拒绝；PersistJournal 类内互斥、sink 锁外执行、append 返回段全局序；SaveTask 携带 journalSeq，landedSeqs_/adoptWatermarks_ 双水位令 drain 回放拒绝旧快照倒灌；freeze/adopt 先等排空再同步写回。新增并发 append/drain、同玩家串行合并、RPC+自动保存竞加重启三例；build-off 85/85、build-examples 85/85、san 84/84 无发现、drill 全链断言全过。
- [x] 跨进程写入代次 database 半先落地：DbSaveRequest.writeGen 上 wire（0=无版本写端兼容受理）、PlayerData.writeGen 随档案持久化（重启即恢复地板）。database 以已落盘档案 writeGen 为地板只验单调不分配（已代决：写端自持代次，方向同 world-cell-management 草案 §3/§8/§10）——旧代次迟到写/旧快照回放如实拒绝档案不动、同代次重试幂等回执不重复落盘（first-landed truth wins）、无版本写端受理且不清地板。Base 侧代次自发分配（锚点播种+每写递增）仍待后续切片。新增 write_generation_ordering 七段全链用例；build-off 受影响 10/10（含 BaseDatabaseProcessSmoke 真双进程）、san 树 6 例无发现。
- [x] 真实保存载荷接入 database，资料变更与去重凭据持久提交；区分传输、受理、执行和持久结果，覆盖重试、乱序、失败与重启。资料面：exp 经锚点→flush→DB_SAVE 全载荷 JSON 持久提交（journal 先段 + drain 落盘 + 写入代次单调，见 L64 行）；凭据面（95f8d46a）：生产装配补齐——WorldAppConfig.settlementCredentialDir + --settlement-credential-dir + dev_fleet 编队目录，start() 接 set_credential_dir + ingest_restored_credentials，e2e 第二副本真链验收（预置 party 2 凭据 → 启动装载 → launch 消费回填 → battle 结算对队长分值幂等拒绝（Base exp 停留首轮值）→ release 擦除凭据目录清空）；分层语义：受理 = dispatch 身份/版本校验，执行+持久 = journal+apply 后才回 success，传输歧义不自动重放（database_client exchange 语义），重试/乱序 = journal 重放 + journalSeq 水位 + 写入代次，失败/重启 = crash restart + replay 测试族。门禁：build-off 86/86、san 85/85、build-examples 86/86。
- [x] database 崩溃恢复契约已钉（E2E）：断链期保存如实失败（传输失败不假受理成功）、锚点真值保留；database 同存储同端口重启 + base 重启后载回最后一次已落盘真值，断链期失败写入不复活，复活后新写照常落盘。可证边界：远程模式无跨崩溃重试缓冲（无本地 journal，未落档增量随崩溃丢失）；无编队监管时 fleet 闸挡住重拨（notify_fleet_down 语义），重连需进程重启或监管在位；乱序拒绝仍待写入代次 wire 设计。
- [x] 当前单进程口径先落地（d2574505）：exp 以 Anchor 为持久真源，activate/启动恢复/adopt 三路播种、flush/freeze 收集覆盖写回、adopt 以快照差值收敛；结算凭据 settlement_<party>.cred 持久提交，重启 ingest 回填、重开副本重放拒绝、未落账补齐、终局回收；build-off/build-examples 各 83/83 绿，含凭据 roundtrip/跨重启去重/挂账 6 例新增与迁移链播种不变量更新。

- [x] 恢复 baseServer 独立可执行入口，通过 PlayerStorage 注入远程 database，按需激活 PlayerAnchor；远程模式不访问本地档案与存储 journal，无存储不可达时的本地回退。
- [x] 新增 database 独立入口，复用现有 DB_LOAD/DB_SAVE 与 M1 信封，提供文件加载/保存、journal 先写/恢复、身份校验及写入完成回执；尚非完整存储分区与异步执行队列。
- [x] 保存队列按玩家 ID 去重，允许同线程槽的不同玩家排队；停机排完已受理任务，不因关闭 running 标志提前丢弃。
- [x] BaseDatabaseTests、BaseDatabaseProcessSmoke、BaseAnchorTests、PersistJournalTests、PersistenceChainTests、RecoveryTests、HostedServicesTests 共 7 个定向门禁通过；独立双进程分别重启、Base 无本地档案及不可达拒起已验收。
- [ ] 拆分旧 PlayerData 的长期字段与模拟位置/运行字段，移除默认 WorldApp 的旧档案宿主及直接存储路径；独立原型不作为最终 Base/Cell 对象契约完成证据。

## P2：worldServer 管理与 cell 执行分离

- [ ] 从当前 WorldApp 分离 worldServer 管理职责与 cell 模拟入口；模拟循环、WorldHost/Scene/Avatar、AOI、战斗与 Lua 迁入执行端，World 保留注册、拓扑、容量和迁移管理。
- [ ] 明确 Space 对应现有 Scene 的实现映射，分开空间 ID、Cell 分区 ID 与执行进程 ID；保留静态 SceneDescriptor 与 Instance 八态，不新增 Scene→Space 同义层。
- [ ] 默认一个 cell 进程，每 Space 一个完整 Cell；同进程承载多个 Space，复用既有副本流程，验证隔离与临时资源回收。
- [ ] cell 向所属 World 注册地址、代次、版本、就绪、负载与排空状态；World 分配完整 Space/Cell，定义心跳失效、重注册、旧执行端隔离和控制面失联策略。
- [ ] World 管理拓扑、邻接与分配版本，单实体只有一个有效模拟代次；Instance 共享目标/时钟/结算明确唯一逻辑负责人。
- [ ] 保留有界意图队列、模拟线程写入、过载拒收及停机尾扫；不得由 IO、管理端或后台线程直接改模拟状态。
- [x] 受理单写者契约已钉（P2 分离前置回归底线）：有界队列顶格拒收不丢旧、两级水位滞回、过载拒收、tick 边界 drain、G-3 停机尾扫、维护/预启动闸六例既有验收；新增第七例——模拟线程外 submitIntent 只入队不写 world（提交即刻探测会话表无绑定），会话绑定仅在 gameThread tick 边界 drain 后显形（resolveIntentPlayer 提为公开只读观测面，互斥会话表跨线程安全）；code-verified：意图消费仅 gameLoop tick 边界与停机尾扫两处，M1 泵/回执/管理面（rollout/drain/debugEval/fleet 上报/metrics）全在 gameLoop 驱动。
- [ ] 更新真实 CMake 入口、CLI、配置、脚本和开发编队，删除冲突宿主接线与别名；文档只标记真实存在的程序，不虚构目录。

## P3：服务目录、Gateway 与登录

- [ ] Base/World 向 loginServer 注册逻辑身份、管理地址、代次、兼容与就绪；World 发布已提交执行位置投影，目录不另行分配 Cell。
- [ ] 玩家记录分开原区服、Base/归属版本、World/Space/Cell/执行进程/可选 Instance/模拟代次，以及 gateway/session/会话版本。
- [ ] 分开进程代次、资料写入代次、实体权威代次、拓扑版本和路由/会话版本；扩展 WorldAssignment/远程引用与真实 wire，不把现有字段当完整契约。
- [ ] gateway 基础业务接所属 Base，模拟输入接当前 cell，下行绑定真实会话；缓存失效、迟到位置投影、重定向和旧代次拒绝可恢复，正常消息不逐条经目录或 World 管理端代理。
- [ ] 登录串联认证 → gateway 会话 → Base 经 database 激活 → World 准入/分配 → cell 创建实体；未进玩法也可保有 Base 对象。
- [ ] 多 gateway 重连、重复登录、旧会话撤销与路由持久性接真实网络，恢复原 Base 与当前模拟位置，不创建第二个资料写入者。
- [ ] 验证 World/cell 无同机 machined 独立注册与运行；machined 不知道各服资料位置，也不裁决跨服去向。

## P4：副本结算、模拟迁移与跨服

- [ ] Base 校验小队、资格和版本，向 World 申请容量；cell 准入可信绑定各成员原 Base、快照、会话和有期限凭证。
- [ ] Instance 结果接 cell → 各原 Base → database，稳定玩法/结算 ID 校验模拟代次；多 Base 分别确认，定义部分成功、补偿、待重试和最终结束。
- [ ] 未持久确认保留唯一结算与恢复记录，覆盖 Base 暂不可达、断线、解散、超时和异常；失败取消不发成功奖励。
- [ ] 真实链路覆盖多 Space/Instance 隔离、多个执行进程选路及远端回收，不沿用 World 间完整档案搬迁作为场景迁移。
- [ ] 明确显式换 Space 与同 Space 跨 Cell 两条路径；现有 SceneTransfer 离场重建 Avatar 可用于换场原语，不能实现连续 Cell 交接。
- [ ] 迁移完整必要运行状态，保持实体 ID、血量、冷却、时钟和必要脚本状态；源冻结、目标未激活准备、条件提交、路由投影更新、目标激活与源清理逐步确认。
- [ ] 迁移决定持久可查询，跨 World 有唯一协调提交依据；结果不明先对账，隔离旧源，覆盖每步超时、源/目标及管理端重启，禁止双端解冻。
- [x] 跨服设计预留：原区服/Base/存储归属稳定，目的地持各成员原 Base 关联，不获取各服数据库凭证；持续 Space 与临时 Instance 均有边界。
- [ ] 跨服需求启用时，接两区服两 Base 同场、分组、容量、准入、逐 Base 幂等结算与安全返回；home_zone_id 不直接当业务区服 ID。

## P5：两种更新方案

- [ ] 协议更新验证真实新旧业务解码、状态转换、未升级服务/客户端兼容；握手 schema_hash 白名单不能代替兼容实现，存储结构演进另行验收。
- [x] 未知/未受理消息型受理面契约先钉（切片）：新增代次消息型（真实增量先例 9fd9690b squad 族 0x0072）与枚举外裸值到达未升级受理面 = 结构化 ERROR 回包（非崩溃、非断链），sessionId 保留、错误原因非空，且受理面不被污染——后续已知类型照常受理；可证边界：scripts/drill_proto_update.sh 仍为 schema_hash 指纹（仅换代次不做真实新旧解码兼容），真实字段演进兼容验收仍开放。
- [x] 真实新旧解码兼容面先钉（切片）：合法帧头下畸形 body（非 JSON）此前向泵线程抛 parse_error 炸进程，现结构化 ERROR 回包（会话号保留，dispatchRequest 补受理兜底）；旧形最小载荷（缺字段全默认）容忍解码为结构化响应；档案载荷未知未来字段忽略不污染、缺字段默认不炸。database::Server::dispatch 已有同等兜底（版本+长度校验）无需改。可证边界：PlayerData::toJson 未转义特殊字符（存量缺陷，随后续切片修复）。
- [x] cell 受理面畸形意图兜底先钉：残帧头/合法帧头+残 body 的意图此前在 gameLoop drain 抛异常炸进程，现 drain 逐意图兜底——坏意图丢弃（已出队不堵队列）、同拍与后续好意图照常执行、受理面不污染；坏意图无延时回执（请求方按超时收敛），回执纪律余量随 P2 分离面定。
- [x] 档案序列化转义与垃圾档兜底已修（L69 存储结构演进前置）：PlayerData toJson/fromJson 由裸字符串手写解析改为 nlohmann（真转义、类型化容忍、未知字段忽略、缺字段结构体默认）；此前含 "exp": 片段的用户名可污染字段解析甚至经 std::stof 崩溃读档方、垃圾档案静默载回全默认档——现特殊字符对称无损、垃圾档 loadPlayer 如实失败、journal 脏条目保留人工排查；nlohmann 依赖经 apollo_app_services PUBLIC + 直编测试目标自取守卫接线（三树重建过）。
- [ ] cell 更新：新旧并存、停止源新分配、tick 边界冻结、目标准备、World 提交权威/路由、按序恢复输入、源清理后退出，原 Base 与存储归属不变。
- [ ] 完整活动连续包含成员、实体、怪物、目标、计时、随机与必要脚本状态；只搬玩家、自然结束或强停不能作为连续迁移证据。
- [ ] Base 更新：排平存储、交接长期对象/会话/待办、提交写权代次、刷新 Base 路由、隔离旧写及清理；数据库归属不变。
- [ ] World 管理端更新：交接拓扑、注册代次、容量/排空、在途创建/迁移决定与提交记录；冻结冲突分配、隔离旧管理端并恢复执行端关联，不让双管理端分配同一 Space/Cell。
- [ ] 定义管理失联期间执行权限/租约与继续 tick 的期限，区分故障停止、重新接管与已有活动恢复，不能用单纯进程重启证明管理升级。
- [ ] 各更新阶段覆盖超时、提交不明、双方重启、不可逆转换及回滚边界，已生效请求不能跨代次盲目重放。
- [ ] 仅代码更新保持编号、字段与 wire；Lua 在执行边界处理模块私有状态、旧协程和回滚副作用，C++ 复用对应角色进程替换。
- [ ] gateway 持续持有 TCP；其自身替换分别验收自然排空、期限强停、免重登恢复与新连接持续接入；TCP 无断开需独立连接交接实现。
- [ ] 分别记录服务接入、免重登、TCP 连续及活动连续，使用真实客户端、多个 Base/World/cell 和失败回滚验证。

## P6：预留接口与最终验收

- [ ] 在当前单 Cell 契约中表达同 Space 多 Cell、全空间坐标、稳定实体 ID、分区范围/邻接/拓扑版本和实体权威代次，避免假定 Space 永远等于一个进程。
- [ ] 预留 Real/Ghost 与观察者视图、邻接复制的版本/失效、可见集连续/去重、跨 Cell 请求裁决/顺序/幂等与故障对账边界；Ghost 无模拟和持久写权，迁移快照不当作 Ghost。
- [ ] 将契约、真实网络、持久化、故障重启、排空、两种更新演练重接最终拓扑；资料、管理与模拟恢复分别验证。
- [ ] 清理冲突配置、脚本、死命令和注释，完成适当构建与回归后按证据更新文档，不以旧档案搬迁日志验收新模型。

## 后续能力：需求触发后实施

- [ ] 实际无缝地图：空间切分/动态负载、邻接 Ghost、连续可见性、跨边界战斗和故障接管；基于上述同一体系扩展，当前不实现算法。
- [ ] 外部 MySQL/Redis/ClickHouse 后端；database 职责先独立，不以产品选择为前提。
- [ ] protobuf/zstd 真实消息路径；生成产物不代表运行接线完成。
- [ ] 专用 Profiler，验证采样开销与启停；当前未集成。
- [ ] OpenTelemetry SDK/OTLP exporter，验证上下文、采样、背压和降级；当前未集成。
- [ ] Kafka/Prometheus/Grafana 部署及具体玩法战斗验证，按实际需求实施。

跨机热备与仲裁未立项；machined 监督重启不作为热备、管理接管或活动恢复证据。
