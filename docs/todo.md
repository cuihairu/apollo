# TODO 任务计划（批次化重排，2026-09-29）

> 重排依据：docs/36-MMO_Frameworks_Comparative_Analysis.md §1 决策追溯表（#1-#18）。
> 原则：每批一个可独立验收的垂直切片；批次内引用决策号（36 号表 #n）与设计文档；验收 = 测试绿 + 生成物基线比对。
> 原组件/功能/SDK/脚本/监控/文档共 22 项全部入册，无删除——Python 绑定一项按决策 #12 降级为「不做，理由登记」。

## 批次 1：Entity 契约系统 ✅ 已交付（2026-09-29）

决策依据：#3（XML+XSD 契约）、#4（生成器不进运行时链接图）、#5（契约↔存储解耦）、#11（8 位 SYNC_* 掩码）。

- [x] def 文件格式定稿：`sdks/contract/{attrs,messages,entities,errors}.xml`（一行一属性紧凑风格）+ `apollo.xsd`（xs:key/keyref/enumeration；错拼标签/属性报错，无静默缺省）
- [x] def 解析器：`modules/contract`（strictWalk 白名单、predict 权限位、sync 掩码 7 合法位 + SYNC_DB 0x04 显式拒绝、别名类型、单继承 + DFS 环检测、attr id 分段校验）
- [x] 生成器骨架：`sdks/gen/apollo_gen`（独立二进制）→ C++ 头 + JSON 投影（`sdks/cpp/generated/` 提交基线）
- [x] 契约与存储解耦验证：契约文件零持久化字段（`persist/column/table` 词汇报错）；storage.xml 语句规范落 docs/18 §5（批次 2 实现）
- [x] 测试：XSD/解析器错误用例（错拼/重复 key/非法枚举/分段/别名/继承矩阵/环）、解析往返 + 不动点、SHA-256 向量、schema_hash 格式无关性、生成物 golden check、constexpr 编译期闸
- [ ] 遗留到批次 7：Unity/Cocos/JS 消费 JSON 投影（投影本身已产出）

## 批次 2：db-app 数据库服务组件（原「高优先级」）

决策依据：#5（storage.xml 服务端私有）、#6（write-behind journal + 列提升，先日志后快照）、#17（MySQL 8.0 + Redis + ClickHouse；PG 留方言缝、Mongo 不留）。

- [ ] storage.xsd + storage.xml v1（docs/18 §5 语句规范落地：id+SQL 模板+param+resultMap，仅 `#{}` 参数化，禁 `${}`）
- [ ] 启动全量解析 → prepared statement 池；任一语句坏 → 聚合报错启动即败；热更 = 全量重建再换
- [ ] ResultMap 代码生成接线：storage.xml → row struct + 绑定代码（生成器不进运行时，决策 #4；禁运行时反射）；改契约/存储忘再生成 → CI 红
- [ ] 数据库连接池（MySQL）+ 断线重连（docs/36 问 5 教训：KBE 无自动重连）
- [ ] 异步存档接口：write-behind journal（先 journal_append 后快照，脏实体平滑刷库——决策 #6 改造 KBE Archiver 算法）
- [ ] AccountDB / CharacterDB / WorldDB 语句集（Hybrid Schema：核心列 + blob，docs/18 §3）
- [ ] 与 BaseApp/CellApp 的 RPC 通道（挂批次 4 的 appmgr 寻址）
- [ ] 自动数据持久化（原功能项并入）：Entity 自动序列化、脏数据检测与同步（attribute-sync §8/§10 水位机制复用）、数据库自动备份策略（docs/18 §8）

## 批次 3：cell-appmgr 空间应用管理器（原「高优先级」）

决策依据：#9（场景实例 Zone，否决无缝世界）、#10（AOI 独立服务）、#16（Battle 独立实例）、#15（ServerID 64 位分段）。

- [ ] CellApp 实例管理 + 空间分区负载均衡 + 动态边界调整
- [ ] Entity 跨 CellApp 迁移调度（ghost 不做——决策 #9 已否决；跨 Zone 迁移走显式 handoff）
- [ ] 过载自动扩容（指标来自批次 8 采集面）
- [ ] AOI 服务对接（网格+四叉树+shard，docs/17 §5）
- [ ] Battle 实例生命周期挂接（短生命周期进程、独立 tick 率，docs/25 §3；帧同步只留适配缝——决策 #18）

## 批次 4：base-appmgr 基础应用管理器（原「中优先级」）

决策依据：#13（Consul/Etcd 注册 + 心跳，否决 UDP 广播发现）、#15（ServerID 分段）。

- [ ] BaseApp 实例管理 + 状态跟踪（心跳 5s/30s，docs/05 §2.3）
- [ ] 新客户端连接分配（负载均衡）
- [ ] 服务注册/发现接线（先 Consul 或 Etcd 二选一，写明取舍）

## 批次 5：AI 导航/寻路

- [ ] Recast/Detour 接入评估（NavMesh 构建、地形数据工具）
- [ ] A* / 路径平滑；服务端 NPC 自动移动（挂 Zone 主循环，docs/17）
- [ ] 寻路请求走线程池旁路（不占 20Hz 全序主线程——决策 #8）

## 批次 6：脚本系统

决策依据：#12（Lua 5.4 + sol2 白名单；否决 KBE 全功能 Python 的攻击面）。

- [ ] 脚本抽象层设计（Lua 单语言；抽象层只留宿主 API 边界，不做多语言运行时）
- [ ] Lua 5.4 + sol2 嵌入与绑定；脚本可写面 = 契约生成白名单（predict 位交集，sdk-contract §5）
- [ ] 脚本热更新机制（换表协议：全量构建再原子替换，对照 storage 池热更同构）
- [ ] ~~Python 嵌入与绑定~~：不做——决策 #12 明确否决（攻击面 + 热更失控；理由登记留档）

## 批次 7：客户端 SDK 投影

决策依据：#4（sdks/ 布局）、#11（sync 掩码多端同源）。

- [ ] Unity SDK（消费 apollo_contract.json：属性 id/掩码/消息 schema 代码生成）
- [ ] Cocos2d-x SDK（同 JSON 投影）
- [ ] JavaScript/WebSocket SDK（浏览器直连 L1 帧定界的 WS 子族）
- [ ] UE4/UE5 SDK（COND_* 思想已在决策 #11 对照；投影同源）

## 批次 8：监控与运维

决策依据：#14（Prometheus/Kafka/ClickHouse，只学定位不学实现）、#13（注册心跳）。

- [ ] 性能指标采集（tick 耗时/队列深度/AOI shard 负载 → Prometheus）
- [ ] LogAgent → Kafka → ClickHouse TLog 流水（docs/18 §7）
- [ ] Console 监控接口 + Web 监控界面（运行时 ConsoleEvent，docs/00）
- [ ] 服务器状态可视化（含批次 2/3/4 组件的容量与迁移视图）

## 批次 9：文档完善（持续）

- [ ] 各组件详细设计文档（随批次 2-8 同步产出，不后置）
- [ ] QA.md 答案编写
- [ ] API 参考文档（契约生成物 + 宿主 API 面）
- [ ] 部署运维文档（K8s/云环境，含回档演练，docs/18 §8）

## 批次推进纪律

1. 每批一个 PR/提交序列；全量测试绿才合入（既有测试不破）。
2. 契约/存储定义文件改动必须同步再生成，golden check 红即阻断（CI 双闸）。
3. 批次间依赖：2←1（契约 hash 进存档元数据）；3/4←2（存档 RPC）；6←1（白名单由契约生成）；7←1（JSON 投影）；8 最后收口但采集面随各批先行埋点。
