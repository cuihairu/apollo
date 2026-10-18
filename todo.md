# Apollo 实施清单

目标职责：gateway、loginServer、baseServer、worldServer、cell、database、machined。worldServer 管理执行进程与 Space/Cell，cell 执行模拟；Cell 是空间分区，不是进程。Space 对应现有 Scene 的空间语义，Instance 保留临时玩法生命周期，不增加同义容器或擅自改代码名。

依据：[BigWorld 与 Apollo 对照](docs/design/bigworld-apollo-mapping.md)、[架构](docs/guide/architecture.md)、[当前决策](docs/architecture/adr.md)。按下面依赖顺序实施，每项以真实验收勾选；库原语或旧拓扑测试不能代替最终接线。跨服保留原 Base、区服和存储归属，当前不启用无缝地图，接口预留未来扩展。

## P0：文档与命名

- [x] 建立 BigWorld 原词 → Apollo 角色 → 现有代码 → 实现缺口的中英文对照；保留真实第三方名称，清除冲突目标，不保留历史备选。
- [x] 明确 worldServer/CellAppMgr、cell/CellApp、Space/Scene、Cell 分区及 Instance 玩法边界；默认一个执行进程，每 Space 一个 Cell。
- [x] 明确 Base 长期对象、Real/Cell 实体、Witness 部分原语及 Ghost 预留，不声称完整 BigWorld 已实现。
- [x] 明确注册、拓扑权威与位置投影，machined 只监督本机，World/cell 独立部署不要求同机 machined。
- [x] 区分支持协议更新的进程替换与仅代码更新；Base、cell、World 管理端各交接不同状态，TCP/会话/活动连续分别验收。
- [x] 保持 VitePress 原生 locales：中文根目录、英文 en，各自正文与导航，新增对照入口并纳入文档 CI。
- [x] 专用 Profiler、OpenTelemetry SDK/OTLP exporter 标为未集成，自研日志、MetricRegistry、span/trace 不作为集成证据。

## P1：独立 Base 与 database

- [ ] 完成 baseServer 在默认编队中的资料归属、基础业务与结算迁入；独立入口已具备，离场或 Instance 销毁不卸载长期资料。
- [x] baseServer/database 进默认编队先落地：dev_fleet.sh 拉起 database 9200（data-dir/journal 落编队运行目录）+ baseServer 9201（存储后端 database 9200，存储不可达即拒起、由 machined 退避重试；编队位置在 database 之后）。进程级验收：双成员 ready 且无退避重启、--probe 经编队内 database 真存真读（player_900001.json 落盘读回）、machined 监督 7 成员在列（重启抖动仅遗留 game 占位项，非本次引入）。双进程尚无 machined 成员注册（号=0）。剩余：资料归属切换（WorldApp 弃档案宿主）、基础业务与结算迁入、离场/Instance 销毁不卸载长期资料验收。
- [ ] 完善 database 的有界异步保存队列（磁盘 I/O 移出 RPC 泵线程）、存储路由与写入代次；独立入口、Base-facing 加载/保存和文件 journal 已具备，不以 MySQL 为前提。
- [x] 有界保存队列先落地（446626fd）：SaveQueue 排队+在途容量上限（saveQueueCapacity，默认 4096，0=不设界），满载背压拒收不丢数据——载荷 journal 先段，脏位/下轮 flush 重取；磁盘 I/O 已在独立 worker 面（P1-4）、存储路由与写入代次已随 PlayerStorage 接口与 journalSeq 水位落地（499111be）。满载拒收 1 例新增；build-off 85/85、build-examples 85/85、san 84/84 无发现、drill 全链断言全过。
- [ ] 从当前 WorldApp/模拟宿主移出完整档案、长期写权和直接存储依赖；定义只读玩法快照与返回业务请求，禁止跨界使用本地 Anchor 指针修改资料。
- [ ] Base 串行修改资料，database 对玩家保存排序并验证写入代次/版本，拒绝旧进程与旧快照覆盖。
- [x] 单进程存档串行化先落地（499111be）：PlayerStore 以 storeMutex_ 为单一串行点，自动保存/RPC 受理/保存回调/迁移冻结四线原子交错，落盘 I/O 全在锁外；SaveQueue 按玩家分槽（playerId % 槽数）同玩家 FIFO 落盘、在途合并拒绝；PersistJournal 类内互斥、sink 锁外执行、append 返回段全局序；SaveTask 携带 journalSeq，landedSeqs_/adoptWatermarks_ 双水位令 drain 回放拒绝旧快照倒灌；freeze/adopt 先等排空再同步写回。新增并发 append/drain、同玩家串行合并、RPC+自动保存竞加重启三例；build-off 85/85、build-examples 85/85、san 84/84 无发现、drill 全链断言全过。
- [x] 跨进程写入代次 database 半先落地：DbSaveRequest.writeGen 上 wire（0=无版本写端兼容受理）、PlayerData.writeGen 随档案持久化（重启即恢复地板）。database 以已落盘档案 writeGen 为地板只验单调不分配（已代决：写端自持代次，方向同 world-cell-management 草案 §3/§8/§10）——旧代次迟到写/旧快照回放如实拒绝档案不动、同代次重试幂等回执不重复落盘（first-landed truth wins）、无版本写端受理且不清地板。Base 侧代次自发分配（锚点播种+每写递增）仍待后续切片。新增 write_generation_ordering 七段全链用例；build-off 受影响 10/10（含 BaseDatabaseProcessSmoke 真双进程）、san 树 6 例无发现。
- [ ] 真实保存载荷接入 database，资料变更与去重凭据持久提交；区分传输、受理、执行和持久结果，覆盖重试、乱序、失败与重启。
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
