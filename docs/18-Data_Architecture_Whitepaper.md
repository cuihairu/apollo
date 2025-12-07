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

## 5. 分库分表与扩容

### 5.1 分片规则
- 初期：账户维度（`char_id % N`）分片；`N` 根据容量预估。
- 采用 ShardingSphere 或自研路由：DataProxy 维护配置，动态刷新。
- 支持水平扩容：新增分片后，通过后台工具迁移数据（online re-sharding）。

### 5.2 索引与归档
-冷数据（离线玩家、历史活动）定期归档至冷库（另一套 MySQL/ClickHouse）。
- 归档策略：依据 `last_login`、`server merge` 产出的玩家列表。

## 6. TLog / BI 流水

### 6.1 流程
```
GameServer → LogAgent → Kafka → (实时) Flink → (离线) ClickHouse/Hive
```
- LogAgent 采用 TCP/NNG，保障写入成功（异步 + 重试）。
- Kafka Topic 按类型划分：MoneyFlow、ItemFlow、LevelFlow、QuestFlow 等。
- ClickHouse 表支持近实时查询，提供 GM/BI 报表。

### 6.2 日志规范
```
timestamp | platform | server | player_id | event | delta | balance | reason | extra_json
```
- 事件枚举与 Protobuf 定义同步；DataProxy 可在写数据时触发 TLog。

## 7. 数据恢复与一致性
- **WAL/备份**：所有异步写操作在本地 WAL 记录，崩溃后重放；DB 定期全量 + 增量备份。
- **热备**：MySQL 主从复制 + MGR；Redis Cluster 多副本。
- **一致性保障**：关键资产操作使用事务或“请求落地后再响应”的策略；普通属性采用最终一致。
- **容灾演练**：定期演练 DB 故障、缓存丢失、Kafka 延迟等场景。

## 8. 工具与流程
- **Schema 管理**：使用 Flyway/Liquibase 或自研工具维护 SQL 版本。
- **数据校验**：定期比对 Redis vs MySQL，检查脏数据。
- **数据导出**：提供玩家数据导出/导入工具（GM 调试、问题排查）。
- **监控**：MySQL QPS/延迟、Redis 命中率、DataProxy 队列长度、Kafka backlog。

## 9. 统一决策
- 官方方案：采用 Hybrid Schema + DataProxy + TLog 系统。`docs/07` 与 `docs/08` 的差异以本白皮书为准。
- Blob 仅对游戏逻辑敏感数据使用；BI/运营关注的字段必须列化。
- DataProxy 是唯一入口，所有服务通过它访问持久层；不能直接绕过写 DB。

## 10. Roadmap
1. 确定 `PlayerProfile` Protobuf 与列化字段列表，生成 C++/Lua/DB schema。
2. 实现 DataProxy v1（Load/Save + Redis 缓存 + 写队列）。
3. 搭建 Kafka/ClickHouse TLog 流水，定义日志枚举。
4. 完成分库分表规划、迁移工具、备份策略。
5. 接入监控/告警，制定操作流程（数据修复、回档）。

---

该白皮书为后续 DataProxy、DB 设计、BI 接入提供统一标准。***
