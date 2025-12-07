# TLog 与 BI 规范

> 目的：统一 Apollo MMORPG 的业务日志（TLog）格式、采集流程和 BI 指标解析，确保数据分析、风控、GM 运营等需求有可靠数据源。

## 1. 总体架构

```
GameServer / Zone / DataProxy
    │
    │ TLog Events (TCP/NNG/HTTP/Batch)
    ▼
LogAgent / Collector
    │
    ▼
Kafka Cluster (Topic 按事件类型划分)
    │
    ├─ Realtime: Flink/Stream -> 告警/监控
    └─ Offline: ClickHouse/Hive -> BI 报表/ETL
```

## 2. 日志格式

### 2.1 通用字段
```
timestamp | platform | server_group | server_id | player_id | account_id | event | delta | balance | reason | action_id | extra_json
```
- `timestamp`: ISO8601 / epoch毫秒。
- `platform`: iOS/Android/PC。
- `server_group`: 大区/分组。
- `server_id`: 64-bit ServerID。
- `event`: 事件类型（枚举）。
- `delta`: 本次变化值（如金币+500）。
- `balance`: 操作后剩余值。
- `reason`: 字符串或枚举，说明来源（副本、充值等）。
- `action_id`: 可选，用于串联复杂操作。
- `extra_json`: 可扩展字段（装备ID、怪物ID等），JSON 字符串。

### 2.2 事件分类
| 类别 | 示例 |
|------|------|
| MoneyFlow | GOLD_ADD, GOLD_SUB, DIAMOND_ADD |
| ItemFlow | ITEM_ADD, ITEM_DEL, ITEM_USE |
| LevelFlow | LEVEL_UP, EXP_ADD |
| QuestFlow | QUEST_ACCEPT, QUEST_FINISH |
| TradeFlow | AUCTION_SELL, MARKET_BUY |
| SocialFlow | GUILD_JOIN, FRIEND_ADD |
| CombatFlow | BATTLE_RESULT, PVP_RANK |
| System | LOGIN, LOGOUT, BAN, GM_ACTION |

事件定义可存于 `config/tlog/events.yaml`，包含字段：
```yaml
- event: GOLD_ADD
  category: MoneyFlow
  description: "增加金币"
  delta_type: int64
  balance_field: gold
  extra_fields:
    - name: source
      type: string
    - name: scene_id
      type: uint32
```

## 3. 采集与传输

### 3.1 LogAgent
- 部署在每台业务服务器，接收游戏进程的日志（NNG/TCP/UDP/文件 tail）。
- 支持批量、缓冲、重试，确保日志不丢失。
- 可选：直接写文件（本地），由 Filebeat/FluentBit 采集。

### 3.2 传输可靠性
- 使用异步写 + WAL；若 Kafka 不可用，LogAgent 缓存到本地（磁盘），恢复后补发。
- 重要事件可同步写（成功后才返回游戏逻辑）。

## 4. Kafka Topic 与 Schema

| Topic | 说明 |
|-------|------|
| `tlog.money` | 金币/钻石等货币变化 |
| `tlog.item` | 道具获得/消耗 |
| `tlog.quest` | 任务相关 |
| `tlog.trade` | 交易/市场 |
| `tlog.social` | 社交事件 |
| `tlog.system` | 登录、封禁、GM 操作 |

每个 Topic 使用统一 schema（Avro/JSON/Protobuf）。建议使用 Avro/Protobuf 以保证 schema 演进。

## 5. 实时处理
- Flink/Stream 应用：
  - 在线人数、活跃度.
  - 经济监控（货币产出/消耗）。
  - 风控（异常交易、外挂行为）。
  - 告警（货币突增、道具异常发放）。
- 将实时指标写入 Redis/ClickHouse，供运营后台展示。

## 6. 离线处理
- Kafka 数据落地 HDFS/S3，通过 Spark/Hive/ClickHouse ETL。
- 分层：ODS → DWD → DWS → ADS。
- BI 报表：留存、LTV、ARPU、货币池、地区分析、活动效果等。
- 与 `docs/18` 中 DataProxy 的 Hybrid Schema 联动，确保列化字段与 BI 表一致。

## 7. 数据字典与版本
- 维护 `tlog_dict.xlsx`/YAML，记录所有事件、字段、含义、单位等。
- 数据字典版本与代码同步，发布时附带版本号，避免 ETL 错用。
- 提供自动生成文档 + BI 接口。

## 8. 安全与隐私
- 日志内容脱敏（例如账号、IP 仅哈希或部分展示）。
- Kafka/HDFS 等存储设置 ACL，限制访问。
- 敏感操作（GM、支付）日志保留更长时间，并入侵略的审计系统。

## 9. 工具与监控
- LogAgent 控制台：查看发送速率、失败率、队列长度。
- Kafka 监控：Lag、吞吐、滞留。
- ClickHouse 监控：Copy 进度、查询延迟。
- 数据质量：自动校验（例如金币总量变化 = TLog Delta 累加）。

## 10. Roadmap
1. 确定 TLog 事件字典与字段标准。
2. 开发 LogAgent 与 SDK（游戏服务端 API）。
3. 部署 Kafka/ClickHouse 集群，建立基础 ETL。
4. 实现实时报警/监控任务（Flink）。
5. 与运营后台打通报表，定期生成分析报告。

---

通过该规范，游戏内所有关键事件均被记录、可实时分析、可追溯，支持 BI、风控、运营策略。***
