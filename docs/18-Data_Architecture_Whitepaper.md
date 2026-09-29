# 数据架构白皮书

> 目的：统一 Apollo MMO 的玩家数据、运营 BI、缓存策略、TLog 方案，解决现有文档中“Blob VS 拆表”冲突，为 DataProxy 和数据库设计提供权威参考。

## 1. 设计原则
1. **游戏性能优先**：逻辑服以 Protobuf/对象序列化方式读写，减少 DB 往返。
2. **可查询性**：运营和 BI 核心字段需列化，支持 SQL 查询/聚合。
3. **最终一致性**：大部分数据异步落地，关键资产（充值、交易）支持同步保障。
4. **混合存储**：主 DB（MySQL 8.0）+ Redis Cache + Kafka/ClickHouse TLog；必要时引入 Mongo/ES 处理特定需求。
5. **可扩展性**：分库分表、水平扩容、在线迁移；统一 DataProxy 层屏蔽底层变化。

## 2. 数据分类与落地

| 数据类型 | 特点 | 落地策略 | 示例 |
|----------|------|----------|------|
| 玩家主数据 | 强一致、结构复杂 | Hybrid Schema：核心列 + Blob (Proto) | 等级、战力、属性、背包 |
| 运行时状态 | 高频变更、短生命周期 | Redis / 内存，定期落库 | 在线状态、场景位置、队伍 |
| 经济/交易 | 强一致、审计敏感 | MySQL 事务 + TLog 双写 | 交易、拍卖、充值记录 |
| 社交数据 | 中等一致性、关联查询多 | MySQL 分表 + Redis 缓存 | 公会、好友、邮件 |
| 行为日志 | 追加写、分析为主 | TLog -> Kafka -> ClickHouse | 金币流水、关卡日志 |
| 配置/静态 | 只读/少更新 | 配置系统 + CDN/版本管理 | Excel → Lua/JSON |

## 3. Hybrid Schema 详解

### 3.1 表结构（示例 `t_character`）
```sql
CREATE TABLE `t_character` (
    `char_id` BIGINT NOT NULL,
    `account_id` BIGINT NOT NULL,
    `name` VARCHAR(64),
    `level` INT,
    `vip_level` INT,
    `power` BIGINT,
    `gold` BIGINT,
    `diamond` BIGINT,
    `map_id` INT,
    `last_login` DATETIME,
    `data_bin` MEDIUMBLOB,
    `version` BIGINT DEFAULT 0,
    PRIMARY KEY (`char_id`),
    KEY `idx_account` (`account_id`),
    KEY `idx_level_power` (`level`, `power`)
);
```

### 3.2 Blob 内容
- 使用 Protobuf（`PlayerProfile`）序列化：包含属性 map、背包、任务、技能等稀疏数据。
- Blob 更新采用新旧版本对比（乐观锁 `version` 字段）避免覆盖。
- DataProxy 负责 Protobuf 序列化/反序列化，业务逻辑只操作内存对象。

### 3.3 列化字段
- 只为高频查询/筛选字段列化（等级、战力、经济、地图、时间戳等）。
- 更新流程：内存对象更新 → 标记脏字段 → DataProxy 在写 DB 时同步更新列值。

## 4. DataProxy 层

### 4.1 职责
- 对业务屏蔽数据库细节，提供 `LoadPlayer`, `SavePlayer`, `QueryPlayers`, `UpdateEconomy` 等 API。
- 维护 Redis + MySQL 一致性（Write-Through/Write-Behind 策略）。
- 管理分库分表路由（ShardingKey = char_id/account_id）。
- 异步写队列 + WAL（本地日志）保障断电恢复。

### 4.2 写策略
| 场景 | 策略 |
|------|------|
| 玩家主数据 | Write-Behind（先写 Redis/内存，异步批量刷 MySQL），关键操作强制 flush |
| 货币变化 | 双写：Redis 立即更新 + TLog 记录 + 异步落库；关键交易同步写 DB |
| 行为日志 | 仅写 TLog（Kafka） |
| 公会/社交 | 视需求可使用 Write-Through（先写 DB，再更新缓存） |

### 4.3 缓存策略
- Redis Cluster：玩家快照、排行榜、限时活动状态；使用 Key 版本号 + TTL。
- 分布式锁/去重：使用 `SETNX` 或 RedLock 管理账号登录/操作。
- 缓存更新：通用模式（invalidate + reload），或通过订阅消息（Pub/Sub）同步。

## 5. storage.xml 语句规范（2026-09-29 补强）

> 落库侧的唯一语句源。与契约侧（`sdks/contract/`，docs/36 决策 #3）对称：**def 管线上传输，storage.xml 管落库**。参考形态取自 MyBatis mapper 的「语句声明化 + 启动全量解析即败 + 参数化」三件套（调研全文见 docs/design/xml-generation.md §6），明确不引入的部分见 §5.5。

### 5.1 语句外置（statement 显式声明）

```xml
<?xml version="1.0" encoding="UTF-8"?>
<storage version="1" dialect="mysql8">
  <!-- 列映射：显式、单向引用契约 attr 名（storage.xsd 的 keyref 锁悬垂） -->
  <resultMap id="character_row" table="t_character">
    <column field="level" attr="level" type="int32"/>
    <column field="gold"  attr="gold"  type="int64"/>
    <column field="blob"  type="bytes" blob="true"/>  <!-- blob 列不做 attr 映射，整体序列化 -->
  </resultMap>

  <statement id="insert_character" kind="insert" resultMap="character_row">
    <param name="char_id" type="int64"/>
    <param name="level" type="int32"/>
    <param name="gold" type="int64"/>
    <param name="blob" type="bytes"/>
    <sql>INSERT INTO t_character (char_id, level, gold, blob)
         VALUES (#{char_id}, #{level}, #{gold}, #{blob})</sql>
  </statement>

  <statement id="journal_append" kind="insert">
    <param name="char_id" type="int64"/>
    <param name="seq" type="int32"/>
    <param name="payload" type="bytes"/>
    <sql>INSERT INTO t_journal (char_id, seq, payload)
         VALUES (#{char_id}, #{seq}, #{payload})</sql>
  </statement>

  <statement id="select_character" kind="select" resultMap="character_row">
    <param name="char_id" type="int64"/>
    <sql>SELECT char_id, level, gold, blob FROM t_character WHERE char_id = #{char_id}</sql>
  </statement>
</storage>
```

- **statement 四要素**：id（全局唯一，xs:key）+ SQL 模板 + 参数列表 + 结果映射（select/update 回填用）。
- **启动解析成 prepared statement 池**：db-app 启动时全量解析 storage.xml，逐条 `PREPARE`；**任一语句解析失败、`#{}` 占位符与 `<param>` 声明不齐、resultMap 引用悬垂 → 启动即败**（聚合报错，与契约解析器同纪律）。
- **改 SQL 不重编**：SQL 模板是数据不是代码——调优索引提示、改写等价 SQL 只改 storage.xml 重启（或热重建，§5.6）。

### 5.2 「改什么不重编」的分界

| 改动 | 是否重编 | 为什么 |
|---|---|---|
| SQL 文本（等价改写/索引提示/方言微调） | **否**——启动重载 prepared 池 | 语句是数据 |
| `<param>` 增删/改型 | 是——row struct 与绑定代码再生成 | 类型变了 |
| `<resultMap>` 列映射增删 | 是——生成物随映射走 | 列↔字段绑定是编译期类型 |
| 表结构（DDL） | 否（本文件不管 DDL，Flyway 管，§9） | 分工 |

### 5.3 ResultMap 走代码生成，禁运行时反射

- 生成器（sdks/gen 同族、独立二进制、**不进运行时链接图**——决策 #4）读 storage.xml 的 resultMap + statement 声明，产出：`row struct`（每 resultMap 一个强类型结构）+ 按名绑定代码（列名→字段，类型转换在声明层完成）+ 语句 id→强类型函数（「按名找语句」收敛为「按名生成的函数」，名字错在编译期红）。
- **运行时零反射**：装载/写回都是编译期定型的强类型路径——热路径上没有字符串→字段的猜测（对照 MyBatis Mapper 动态代理：C++ 无此机制，生成期直接产函数，接口绑定思想保留）。
- 生成物与 `sdks/cpp/generated` 同纪律：提交基线 + CI 比对（改 storage.xml 忘再生成 → 红）。

### 5.4 参数化强制：只有 `#{}`，没有 `${}`

- 语法层面**不存在**字符串拼接形态：storage.xsd 只认 `#{identifier}` 占位符；SQL 模板中出现 `${` 即校验失败（不是警告）。
- `<param>` 显式声明 + 启动校验 `#{}` 集合 ⊆ `<param>` 集合——错拼参数名在启动即败，不留到运行期。
- 理由落到 MyBatis 的教训：`${}` 是注入面；C-44 实测缺陷簇（不转义/条件转义/静默默认）在「唯一参数化路径」下结构性消失。

### 5.5 明确不抄的 MyBatis 机制

| 不引入 | 理由 |
|---|---|
| 动态 SQL 标签（`<if>/<foreach>/<where>` 运行期解释器） | 游戏服语句面小而稳（write-behind 三条 + 列提升读写 + 少量查询）；查询形态固定，参数化模板够用；迷你语言解释器是负资产 |
| ORM 延迟加载 / N+1 / 会话级缓存（SqlSession/identity map） | 内存权威模型（attribute-sync §3）：进程内属性即真相，无逐行惰性取数场景；写路径走 **write-behind journal**（先日志后快照，决策 #6），不靠 ORM 事务语义兜底 |
| 二级缓存 `<cache/>` | 破坏单写者与 write-behind 纪律（Redis 层同理，只做缓存不做一致性） |
| 运行期热换单条语句 | 热更走「全量重建再换」（同 §5.6），与 MyBatis 只给重建一条路同构 |

### 5.6 热更形态

FileWatcher/GM 触发 → 工作线程全量重解析 storage.xml + 全漏斗校验 → tick 边界原子重建 prepared statement 池 → 失败保旧池 + 告警。不是增量改池中单条语句，是整体重建再换（scripting-lua §3.2 换表协议同构）。

### 5.7 与 def 契约的零交叉边界（重申，决策 #5）

- **attrs.xml 完全不知道 storage.xml 的存在**：契约文件里没有任何持久化字段/位/声明（SYNC_DB 0x04 被解析器显式拒绝，`persist/column/table` 等属性名在白名单外）——换库、改表、调语句，契约零改动、`schema_hash` 零变化。
- **storage.xml 单向引用契约**：列映射可引用 attr 名作映射源，方向恒为 storage→contract；storage.xsd 的 xs:keyref 锁死悬垂（引用了不存在的 attr 即校验失败）。
- 换 DB（MySQL→PostgreSQL）= 换一份 storage.xml 语句集 + 方言标记（`dialect`）；契约、生成器、SDK、客户端全部不动——这是「学 MyBatis 语句即数据」的直接红利（docs/36 问 11）。

## 6. 分库分表与扩容

### 6.1 分片规则
- 初期：账户维度（`char_id % N`）分片；`N` 根据容量预估。
- 采用 ShardingSphere 或自研路由：DataProxy 维护配置，动态刷新。
- 支持水平扩容：新增分片后，通过后台工具迁移数据（online re-sharding）。

### 6.2 索引与归档
-冷数据（离线玩家、历史活动）定期归档至冷库（另一套 MySQL/ClickHouse）。
- 归档策略：依据 `last_login`、`server merge` 产出的玩家列表。

## 7. TLog / BI 流水

### 7.1 流程
```
GameServer → LogAgent → Kafka → (实时) Flink → (离线) ClickHouse/Hive
```
- LogAgent 采用 TCP/NNG，保障写入成功（异步 + 重试）。
- Kafka Topic 按类型划分：MoneyFlow、ItemFlow、LevelFlow、QuestFlow 等。
- ClickHouse 表支持近实时查询，提供 GM/BI 报表。

### 7.2 日志规范
```
timestamp | platform | server | player_id | event | delta | balance | reason | extra_json
```
- 事件枚举与 Protobuf 定义同步；DataProxy 可在写数据时触发 TLog。

## 8. 数据恢复与一致性
- **WAL/备份**：所有异步写操作在本地 WAL 记录，崩溃后重放；DB 定期全量 + 增量备份。
- **热备**：MySQL 主从复制 + MGR；Redis Cluster 多副本。
- **一致性保障**：关键资产操作使用事务或“请求落地后再响应”的策略；普通属性采用最终一致。
- **容灾演练**：定期演练 DB 故障、缓存丢失、Kafka 延迟等场景。

## 9. 工具与流程
- **Schema 管理**：使用 Flyway/Liquibase 或自研工具维护 SQL 版本。
- **数据校验**：定期比对 Redis vs MySQL，检查脏数据。
- **数据导出**：提供玩家数据导出/导入工具（GM 调试、问题排查）。
- **监控**：MySQL QPS/延迟、Redis 命中率、DataProxy 队列长度、Kafka backlog。

## 10. 统一决策
- 官方方案：采用 Hybrid Schema + DataProxy + TLog 系统。`docs/07` 与 `docs/08` 的差异以本白皮书为准。
- Blob 仅对游戏逻辑敏感数据使用；BI/运营关注的字段必须列化。
- DataProxy 是唯一入口，所有服务通过它访问持久层；不能直接绕过写 DB。
- **落库语句唯一源 = storage.xml**（本文件 §5）：语句外置 + 参数化强制 + ResultMap 生成化；def 契约与 storage.xml 零双向交叉。

## 11. Roadmap
1. 确定 `PlayerProfile` Protobuf 与列化字段列表，生成 C++/Lua/DB schema。
2. 实现 DataProxy v1（Load/Save + Redis 缓存 + 写队列）。
3. 搭建 Kafka/ClickHouse TLog 流水，定义日志枚举。
4. 完成分库分表规划、迁移工具、备份策略。
5. 接入监控/告警，制定操作流程（数据修复、回档）。
6. db-app 批次（todo 批次 2）：storage.xsd + storage.xml v1 + 语句池装载 + row struct 生成接线（§5）。

---

该白皮书为后续 DataProxy、DB 设计、BI 接入提供统一标准。***
