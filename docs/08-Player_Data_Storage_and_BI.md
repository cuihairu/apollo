# 玩家数据存储方案：性能与 BI 的平衡之道 (Hybrid Storage Design)

> **核心矛盾**:
>
> - **高性能 (Game)**: 游戏逻辑喜欢对象化、结构紧凑的数据（如 C++ Struct / Protobuf），读写越快越好，最好一次 IO 搞定所有属性。
> - **便利性 (BI/Ops)**: 运营和数据分析师喜欢关系型数据（SQL），希望所有属性都是独立的列，方便 `SELECT * FROM players WHERE level > 50 AND gold > 10000`。

为了同时满足这两个需求，专业架构推荐采用 **"关键字段列化 + 全量数据 Blob + 行为日志 TLog"** 的混合存储模式。

---

## 1. 数据库表结构设计 (Hybrid Schema)

即使使用 MySQL，也不要把 200+ 个战斗属性设计成 200 个列。

### 1.1 核心表结构 (`t_character`)

```sql
CREATE TABLE `t_character` (
    -- 索引区：用于逻辑查找
    `char_id` BIGINT NOT NULL COMMENT '角色ID',
    `account_id` BIGINT NOT NULL COMMENT '账号ID',

    -- 运营/BI 高频查询区 (Extracted Columns)
    -- 这些字段在 Save 时，从内存从提取出来单独存列
    `name` VARCHAR(64) NOT NULL COMMENT '角色名',
    `level` INT DEFAULT 1 COMMENT '等级 (用于分布统计)',
    `vip_level` INT DEFAULT 0 COMMENT 'VIP等级 (用于大R分析)',
    `power` BIGINT DEFAULT 0 COMMENT '战斗力 (用于实力分层)',
    `gold` BIGINT DEFAULT 0 COMMENT '金币 (经济监控)',
    `diamond` BIGINT DEFAULT 0 COMMENT '钻石 (核心存量)',
    `create_time` DATETIME COMMENT '创角时间 (留存分析)',
    `last_login` DATETIME COMMENT '最后登录 (流失分析)',
    `map_id` INT DEFAULT 0 COMMENT '所在地图 (卡点分析)',

    -- 核心数据区 (Binary Blob)
    -- 包含所有属性、Buff、技能CD、任务进度等海量细节
    -- 游戏读写只认这个字段，高效且兼容性强 (Protobuf可扩展)
    `data_bin` MEDIUMBLOB COMMENT 'Protobuf 序列化数据',

    PRIMARY KEY (`char_id`),
    INDEX `idx_acc` (`account_id`),
    INDEX `idx_level_power` (`level`, `power`), -- 方便运营后台查询
    INDEX `idx_last_login` (`last_login`)      -- 方便找流失用户
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 1.2 为什么这样设计？

1.  **性能极高**: `data_bin` 是一坨二进制，游戏服务器 Deserialize 极快。新增一个“火抗性”属性，**不需要** `ALTER TABLE`，直接改 Protobuf 定义即可，DB 无感知。
2.  **满足 80% 的 BI 需求**: 运营通常只关心等级分布、大 R 流失、货币通胀。这些关键数据做成了列 (`level`, `gold`)，可以直接 SQL 查询，速度飞快。
3.  **满足 100% 的游戏逻辑**: 无论多复杂的嵌套结构（如 `List<Item>`），都可以塞进 Blob。

---

## 2. 属性的序列化存储 (Protobuf Design)

在 `data_bin` 内部，建议使用 Protobuf 的 `map` 特性来存储属性，以兼顾灵活性和存储空间（稀疏存储）。

```protobuf
message PlayerProfile {
    // 基础信息
    uint64 id = 1;
    string name = 2;

    // 属性模块 (核心设计)
    // key: 属性ID (Define in C++ enum/Excel)
    // value: 属性值
    // 优势: 只有非0的属性才占用空间。如果是 500 个属性但玩家只有 3 个有值，则只存 3 个。
    map<int32, int64> attributes = 10;

    // 模块化数据
    BagData bag = 11;
    QuestData quest = 12;
    SkillData skill = 13;
}
```

**BI 如何分析 `data_bin` 里的冷门数据？**

- **离线 ETL**: 写一个简单的 Python/Go 工具，每天晚上把从库的 `t_character` 导出，解析 Protobuf Blob，展平为 Hive/ClickHouse 的大宽表。

---

## 3. 真正的 BI 核心：TLog (Transaction Log)

**千万不要试图通过查询数据库快照来做精细的 BI 分析。**
数据库存的是**当前状态 (Snapshot)**，BI 需要的是**变化过程 (Flow)**。

- **例子**: 玩家只有 100 金币。
  - **DB 状态**: `gold = 100`。
  - **BI 问题**: 这 100 是充值来的？还是打怪掉的一点点攒的？还是昨天有 100 万 今天花剩下的？
  - **结论**: DB 回答不了这个问题。

### 3.1 埋点日志系统设计

你需要一套 **LogServer**。GameServer 不写 DB 日志，而是发类似于 `UDP/TCP` 的日志包给 LogServer，由 LogServer 落盘为文件，然后由 Flume/Filebeat 采集到大数据平台。

**日志格式标准 (TLog 标准)**:

```text
时间 | 平台 | 区服 | 玩家ID | 事件名 | 变化量 | 变化后存量 | 动作原因 | 关联对象
```

**示例日志**:

1.  **MoneyFlow (货币流水)**:
    `2024-12-06 10:00:01 | Android | S1 | 10086 | AddGold | +500 | 1500 | KillBoss | BossID:9981`
2.  **ItemFlow (道具流水)**:
    `2024-12-06 10:05:30 | Android | S1 | 10086 | DelItem | -1 | 0 | Consume | ItemID:HP_Potion`
3.  **LevelFlow (升级流水)**:
    `2024-12-06 11:20:00 | Android | S1 | 10086 | LevelUp | 10->11 | 11 | ExpFull | NULL`

### 3.2 BI 架构图

```mermaid
graph LR
    GS[GameServer] --> |Log Packet| LogAgent[LogServer/Agent]
    LogAgent --> |Write| LocalFile[本地日志文件]
    LocalFile --> |Filebeat| Kafka[Kafka 消息队列]

    Kafka --> |Realtime| Flink[实时计算 (GM监控/在线人数)]
    Kafka --> |Offline| HDFS/S3[数据仓库 (Hive/ClickHouse)]

    subgraph DataWarehouse
        ODS[原始层] --> DWD[明细层]
        DWD --> DWS[汇总层: 留存/LTV/ARPU]
    end
```

---

## 4. 总结

**“既要性能，又要 BI” 的最终解决方案：**

1.  **运行时 (Game)**: 使用 `C++ Object`，极致速度。
2.  **持久化 (DB)**: 使用 **Hybrid 模式**。
    - **Blob**: 存全量，保证不丢数据，开发方便。
    - **Columns**: 存 10-20 个核心维度 (等级/金钱/战力)，方便后台管理和简单统计。
3.  **深度分析 (BI)**: 使用 **TLog 流水日志**。
    - 不要指望 DB 能分析出“玩家行为习惯”。
    - 全靠埋点日志导入 ClickHouse/Doris 进行多维分析。

这种架构是腾讯、网易等大厂的标准做法，兼顾了服务器的 TPS 和 数据分析的深度。
