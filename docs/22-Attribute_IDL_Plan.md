# 属性与 IDL 统一方案

> 目的：建立 Apollo 属性/消息 ID 的统一来源，确保 C++/Lua/数据库/客户端/BI 各模块共享同一套定义，避免重复和漂移。

## 1. 目标
1. **单一源头**：所有属性、消息 ID、枚举在一份配置（Excel/YAML/Proto）中定义，自动生成多语言代码。
2. **多端同步**：生成 C++ 头文件、Lua 表、Protobuf ID、数据库字段、BI 字典。
3. **可扩展性**：新增属性/消息可在配置中追加，不影响旧数据。
4. **元数据**：记录属性的标记（同步、持久化、广播）、默认值、描述。
5. **运行时一致性**：属性定义不仅生成 ID，还要直接驱动服务端同步策略，避免“文档一套、代码一套”。

## 2. 属性定义方案

### 2.1 数据源
- 主配置：`config/excel/AttributeConfig.xlsx` 或 YAML/JSON。
- 每条属性字段示例：
  | 字段 | 含义 |
  |------|------|
  | `AttrId` | 唯一 ID |
  | `Name` | 文本名称 |
  | `Category` | Basic/Combat/Element/Status... |
  | `Type` | int32/int64/float/bool/string |
  | `Audiences` | 同步目标（Owner/AOI/Team/Guild/Service/Persistence） |
  | `Priority` | 同步优先级（Immediate/High/Normal/Low） |
  | `Reliability` | 可靠性（Reliable/Unreliable） |
  | `MinIntervalMs` | 最小同步间隔，用于抖动合并/限频 |
  | `AllowDelta` | 是否允许增量同步 |
  | `IncludeInFullSync` | 是否参与全量快照 |
  | `Flags` | 兼容性标记（DB/Broadcast/BI/Deprecated…） |
  | `DefaultValue` | 默认值 |
  | `Description` | 注释 |

### 2.2 生成目标
- `AttributeIds.h`（C++ 枚举 + 元数据结构）
- `attribute_sync_meta.generated.h`（服务端同步规则表）
- `Attribute.lua`（供服务器 Lua/脚本使用）
- `attribute.proto`（proto 中定义 `enum AttributeId`）
- `db_attribute_map.json`（BI/数据库用于映射列/字段）
- 文档 (`Attribute.md`) 自动生成表格。

### 2.3 运行时同步模型
- 属性容器只负责存值和触发变更事件，不再承担完整同步策略。
- 服务端同步核心基于元数据驱动，按 `audience + priority + reliability` 拆分同步通道。
- 每次本地属性变更生成递增 `version`，用于批次确认、重发和观测。
- 增量同步默认采用合并策略：同一属性在一个窗口内只保留最新值。
- `MinIntervalMs` 只约束重复发送，首次变更应立即下发。
- 全量同步与增量同步使用同一份 schema 过滤，防止 owner/AOI/service 导出字段不一致。

### 2.4 工具链
- 步骤：Excel → Python/Go 工具解析 → 生成文件。
- 校验：工具检查 ID 唯一、Flag 合法、类型匹配。
- 同步校验：检查 `Audiences`、`Priority`、`Reliability`、`MinIntervalMs` 是否落在合法枚举范围。
- 集成：在构建流程中自动执行（如 `cmake --build` 前执行脚本）。

## 3. 消息 ID / Proto

### 3.1 结构
- 在 `Tool/pb/message_registry.yaml` 维护消息 ID → Proto 映射：
  ```yaml
  messages:
    - id: 1001
      name: PlayerMove
      proto: fugumessage/player_move.proto
      reliable: true
      websocket: true
      broadcast_flags: ["AOI"]
  ```
- 生成 `MessageId.h`、`message_id.lua`、`message_id.ts` 等。

### 3.2 生成流程
- 工具读取 YAML → 解析 Proto → 验证消息是否包含 `MSG_ID` 常量。
- 与 NetCore/MessageDispatcher 自动对接。

## 4. BI/数据同步
- 为 BI 输出版生成 `attr_dict.csv`（字段名称、ID、类型、含义），供数据仓库使用。
- DataProxy 在写 DB/TLog 时采用同一份属性字典，确保 ETL 与实时数据一致。

## 5. 版本与发布
- 属性/消息定义版本化（Git 管理），每次修改需审查（检查单、自动测试）。
- SDK 与服务端版本需同步（定义文件的 hash/版本号写入构建产物）。
- 自动生成 CHANGELOG（新增/修改属性/消息 ID）。

## 6. Roadmap
1. 选择单一格式（Excel + YAML）并整理现有属性列表。
2. 编写解析/生成工具，生成 C++/Lua/proto 等文件，替换手写枚举。
3. 整合到构建流程，每次构建前自动验证属性/消息定义。
4. 提供文档/可视化工具展示属性/消息 ID（用于调试、运营）。
5. 将 DataProxy/BI/TLog 等模块改为引用统一字典。

---

通过该方案，属性和消息 ID 将不再散落在多份文档中，而由单一源头控制；同步规则也会随属性定义一起生成并进入运行时，减少维护成本并提高一致性。
