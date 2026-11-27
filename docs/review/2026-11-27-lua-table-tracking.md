# 外部评审：Apollo Lua Table Tracking（Sidecar Metadata）设计

> **来源**：https://chatgpt.com/s/t_6b09209f3f4c81919ddcc34e2bb4e537
> **抓取日期**：2026-11-27（GitHub Actions fetch-share workflow，run 45951653559）
> **性质声明**：本文是**外部建议（ChatGPT 对话）**，不是本仓事实陈述。所有论断在用前必须对照代码复核。Treat as untrusted external advice.

## 核验结论（落盘时对照源码）

| 评审提到的机制 | 仓内 grep 结果 | 判定 |
|---|---|---|
| Sidecar Metadata / Lua Table Tracking 子系统 | `src/`、`docs/` 无匹配 | 文档层面查无——是否已设计过由用户裁定 |
| Mutation Journal / Attribute Transaction | 无匹配 | 文档层面查无 |
| 改 Lua 5.5.1 C 源码（ltable.c/lvm.c/lgc.c 写入观察） | 本仓 `modules/script` 用原生 Lua C API，未见改引擎 Lua 源码 | 文档层面查无实现 |
| 现有 attrsync 钩子窗口（评审应复用） | `modules/game/attrsync`、`script_runtime.cpp:run_attr_hook_window` 已实现且有测试 | 已存在（本文复用它） |

**落地跟踪**（每条完成后勾划并填 commit）：
- [ ] 对照 apollo 现有 ScriptRuntime/attrsync 评估 Sidecar 方案是否必要
- [ ] 若采用：Lua 5.5.1 源码改动可行性验证（不污染系统 Lua）
- [ ] 深度嵌套 table 变更感知的实现路径拍板

---

我建议把 Apollo 的 Sidecar Metadata 设计成一个独立的 **Lua Table Tracking 子系统**，而不是简单做一个 `table* -> metadata` 的哈希表。

它需要解决的不只是属性变化监听，还包括：

1. 如何识别一个 Lua Table 是否属于 Apollo Entity。
2. 如何在不修改 Lua Table 内存布局的情况下维护元数据。
3. 如何追踪深度嵌套 Table。
4. 如何捕获普通赋值、`rawset`、C API 和标准库造成的修改。
5. 如何正确处理 Lua GC、Table 回收和对象复用。
6. 如何把 Lua 修改转换成 Apollo 的 Attribute Transaction。
7. 如何避免普通 Lua Table 产生额外性能开销。
8. 如何在网络同步时只输出稳定的属性 ID，而不是 Lua Table 结构。

我建议采用下面这套设计：

(推荐方案) **Lua 5.5.1 + Sidecar Registry + Mutation Observer + Attribute Binding**

核心原则：

- 不修改 Lua `Table` 的内存布局。
- 不使用 Proxy 作为属性追踪基础。
- 不追踪普通 Lua Table。
- 不让 Sidecar 持有 Lua 对象的裸指针作为长期引用。
- Lua VM 只负责记录变化，不负责业务逻辑。
- Apollo 负责属性校验、事务、Dirty 和网络同步。

---

---

# 1. 总体架构

**Lua 5.5.1 Runtime**

原生 Table / VM / C API / GC

不改变 Table 对象布局

**Lua Table Mutation Observer**

普通写入、rawset、C API、标准库写入

底层统一观察，记录 mutation

**Sidecar Registry**

**Object Metadata**

table\_id / generation / flags

**Ownership Graph**

parent / child / Entity

**Schema Binding**

attr\_id / field\_id / type

**Lifetime Tracking**

GC / weak reference / generation

**Apollo Attribute Transaction**

权限校验 / 类型校验 / 变更历史 / Dirty 合并

**Existing AttrSyncManager**

Hook Window / DeltaBuilder / FrameCodec / Persistence

这里要特别强调：Sidecar Registry 和 Mutation Journal 是两个不同的东西。

- Registry 负责回答：这个 Table 是谁、属于谁、对应哪个属性。
- Journal 负责回答：刚刚发生了什么修改。

不要把两种职责塞到同一个数据结构里。

# 2. Sidecar Metadata 数据模型

## 2.1 TableMetadata

每个受 Apollo 管理的 Lua Table 对应一份 Sidecar Metadata。

```cpp
using LuaTableId = uint64_t;
using EntityId = uint64_t;
using AttrId = uint32_t;
using SchemaId = uint32_t;

enum class TableKind : uint8_t {
    EntityRoot,
    Struct,
    Array,
    Map,
    Internal
};

enum class TrackingState : uint8_t {
    Active,
    Detached,
    PendingGC,
    Retired
};

struct TableMetadata {
    LuaTableId id;
    uint32_t generation;

    EntityId entity_id;

    LuaTableId parent_id;
    AttrId root_attr_id;
    uint32_t field_id;

    SchemaId schema_id;

    TableKind kind;
    TrackingState state;

    uint32_t flags;

    uint64_t mutation_version;
};
```

字段含义：

| 字段 | 作用 |
|--------|--------|
| `id` | Apollo 分配的稳定 Table ID |
| `generation` | 防止 ID 复用造成 ABA 问题 |
| `entity_id` | 所属 Entity |
| `parent_id` | 父 Table |
| `root_attr_id` | 对应的根属性 ID |
| `field_id` | 当前结构成员 ID |
| `schema_id` | 属性 Schema |
| `kind` | Struct、Array、Map 等 |
| `state` | 生命周期状态 |
| `flags` | 追踪和权限标记 |
| `mutation_version` | 该 Table 的变更版本 |

这里我建议 `root_attr_id` 始终保存最顶层的 Apollo 属性 ID。

例如：

```lua
entity.attributes.combat.attack = 200
```

其元数据关系：

```text
Table 100
  entity_id = 1001
  root_attr_id = 10
  parent_id = 0

Table 101
  entity_id = 1001
  root_attr_id = 10
  parent_id = 100
  field_id = 1

Table 102
  entity_id = 1001
  root_attr_id = 10
  parent_id = 101
  field_id = 3
```

最终通过 Schema 解析：

```text
attributes.combat.attack -> attr_id 101
```

注意，Table ID 是运行时身份，不能当作网络协议 ID。

## 2.2 Registry

```cpp
class LuaTableRegistry {
public:
    TableMetadata* find(
        lua_State* L,
        const void* table_identity
    );

    LuaTableId registerTable(
        lua_State* L,
        int index,
        const TableMetadata& metadata
    );

    void unregisterTable(
        LuaTableId id,
        uint32_t generation
    );

    void attachChild(
        LuaTableId parent,
        LuaTableId child,
        uint32_t field_id
    );

    void detachChild(
        LuaTableId parent,
        LuaTableId child
    );
};
```

实际实现中，我不建议直接把 `const void*` 作为长期索引键。

原因是 Lua GC 可以释放 Table，对象内存可能被重新利用。

推荐采用：

- 运行期间通过 Table 对象身份快速查找。
- 对象销毁时清理索引。
- 每次重新注册都增加 generation。
- 延迟事件携带 `id + generation`。
- 不允许使用已经 Retired 的元数据。

## 2.3 Registry 内部容器

建议采用两个索引：

```cpp
struct RegistryStorage {
    // Lua Table identity -> Metadata handle
    IdentityMap identity_map;

    // Stable ID -> Metadata
    MetadataPool metadata_pool;
};
```

`identity_map` 只负责当前存活 Table 的查找。

`metadata_pool` 使用稳定的 slot 或 handle，避免 `unordered_map` 扩容导致内部指针失效。

对于每个 Entity，再维护一个 Root Table 索引：

```cpp
struct EntityTracking {
    EntityId entity_id;

    LuaTableId root_table_id;

    std::vector<LuaTableId> owned_tables;

    uint64_t mutation_version;
};
```

这样可以在 Entity 销毁时快速清理其关联元数据。

---

# 3. 如何在不修改 Table 布局的情况下识别 Lua Table？

这是整个方案最重要的技术点。

我建议使用 **弱引用注册表 + Sidecar Identity Index**。

Lua 5.5 提供 Registry 和 Weak Table 机制，可以用它们建立 Lua GC 对象与 C++ Metadata 之间的关联。

示意：

```lua
-- 概念示意，不是直接给用户使用的 API
weak_registry[tracked_table] = metadata_id
```

弱键保证 Registry 不会因为保存 Table 而阻止其被 GC 回收。

C++ 侧：

```cpp
struct MetadataHandle {
    uint64_t id;
    uint32_t generation;
};
```

通过 Registry 获取：

```cpp
MetadataHandle getMetadata(
    lua_State* L,
    int table_index
);
```

这里有两个容易犯错的地方：

**第一，不能让 Sidecar 自己持有强引用。**

否则：

```text
C++ Registry
    -> Lua Table
    -> C++ Registry
```

会导致 Table 永远无法回收。

**第二，不能仅依赖 Lua 弱表清理 C++ 内存。**

弱表中的关联在 GC 阶段清理，但 C++ Metadata Pool 也需要及时回收。需要通过 Lua GC 生命周期集成，或者在安全的 GC 后处理阶段批量清理已经失效的 handle。

我建议两种身份索引职责分开：

| 机制 | 作用 |
|--------|--------|
| Lua 弱键表 | 判断 Lua Table 是否仍然存活 |
| C++ Identity Map | 快速从 Table 身份找到 Metadata |
| Metadata Pool | 存放 Sidecar 数据 |
| Generation | 防止旧 handle 被错误复用 |

注意：弱键表的迭代和清理时机受 Lua GC 控制，不能假设对象一旦不可达就立即得到销毁回调。

# 4. Lua GC 与 Sidecar 的生命周期管理

这一部分需要比普通 C++ 对象管理更谨慎。Lua Table 的生命周期由 Lua GC 控制，Sidecar 不能自行决定对象何时销毁。

我建议采用双层生命周期管理。

## 4.1 不使用裸指针作为长期身份

可以在 Lua Table 存活期间使用其地址作为快速查找键，但它只是一种临时索引：

```cpp
struct TableIdentity {
    const void* address;
};

struct MetadataHandle {
    uint64_t id;
    uint32_t generation;
};
```

完整关系：

```text
Lua Table address
       |
       v
IdentityMap
       |
       v
MetadataHandle
       |
       v
MetadataPool
```

这意味着：

- Table 地址只在对象存活期间有效。
- MetadataHandle 可以安全地放进 Mutation Journal。
- Table 被回收后，IdentityMap 必须清理。
- Metadata 被复用时，generation 必须变化。

**不要把 `Table*` 放入延迟执行的 Hook 队列。**

## 4.2 GC 回收流程

建议实现以下生命周期：

```text
ACTIVE
  |
  | Lua GC 发现对象不可达
  v
PENDING_GC
  |
  | Lua GC 清理弱引用
  v
RETIRED
  |
  | Apollo 清理 IdentityMap
  v
FREE
```

这里的状态迁移只是逻辑模型。不能简单地假设弱表清理和对象释放一定按上述顺序发生，需要结合 Lua 5.5.1 的实际 GC 阶段实现。

具体规则：

1. Lua GC 是 Table 生命周期的唯一权威。
2. Sidecar 不持有 Table 强引用。
3. GC 阶段不调用游戏业务回调。
4. GC 清理只标记失效 handle。
5. Apollo 在安全点回收 Metadata。
6. Mutation Journal 中的事件通过 generation 验证。

## 4.3 一个重要优化：区分 Entity Root 和普通 Child

不要让每个嵌套 Table 都重复保存一份完整属性信息。

建议：

```cpp
struct TableMetadata {
    LuaTableId id;
    uint32_t generation;

    LuaTableId parent_id;

    uint32_t field_id;

    uint32_t flags;
};
```

根节点额外保存：

```cpp
struct EntityRootMetadata {
    EntityId entity_id;
    AttrId attr_id;
    SchemaId schema_id;
};
```

通过父节点向上解析所属 Entity 和根属性。

但考虑性能，可以给 Child Metadata 缓存：

```cpp
EntityId cached_entity_id;
AttrId cached_root_attr_id;
```

这些缓存只在结构归属变化时更新，不需要每次写入都遍历父节点。

---

# 5. 嵌套 Table 的归属管理

这里我建议引入一个明确的概念：**Ownership Graph**。

Lua Table 本身是引用类型，因此不能简单假设它只有一个父节点。

例如：

```lua
local shared = {
    attack = 100

entity.stats = shared
entity.backup_stats = shared
```

如果允许这种结构，修改：

```lua
shared.attack = 200
```

究竟应该产生一个属性变化还是两个属性变化？

这会影响增量同步、权限和持久化。

我的建议是 Apollo 托管属性采用严格的树形所有权：

- 一个托管 Child Table 只能有一个父节点。
- 不同属性之间不允许共享托管 Table。
- 普通 Lua Table 可以自由共享。
- 如果把普通 Table 挂到托管属性中，必须执行复制或所有权转移。
- 循环引用禁止进入托管属性结构。

这样可以保证属性路径唯一。

## 5.1 Ownership Metadata

```cpp
struct ChildBinding {
    LuaTableId child_id;
    LuaTableId parent_id;

    uint32_t field_id;

    uint32_t slot_index;
};

class LuaOwnershipGraph {
public:
    bool attach(
        LuaTableId parent,
        LuaTableId child,
        uint32_t field_id
    );

    void detach(
        LuaTableId parent,
        LuaTableId child
    );

    bool validateNoCycle(
        LuaTableId parent,
        LuaTableId child
    );

    bool validateSingleOwner(
        LuaTableId child
    );
};
```

这里的 `field_id` 应来自 Schema，不应该直接把 Lua 字符串 key 当成网络属性标识。

## 5.2 替换 Child Table

例如：

```lua
entity.stats = {
    attack = 100

```

随后：

```lua
entity.stats = {
    attack = 200

```

第二次赋值需要完成：

1. 识别旧 Child Table。
2. 从旧 Ownership Graph 脱离。
3. 为新 Table 建立追踪。
4. 根据 Schema 校验新结构。
5. 更新 Entity 属性值。
6. 生成一次根属性替换事件。

不能因为新 Table 被挂载，就忽略原来整个属性结构的替换。

对于性能敏感场景，可以采用 Copy-on-write 或结构化赋值，但第一版不必引入复杂优化。

# 6. Mutation Observer：具体如何捕获写入

这是实现中最容易出现遗漏的地方，也是我建议最先做源码实验的部分。

我查阅了 Lua 5.5.1 的实际源码，有一个重要发现：

**Lua 5.5 并不存在一个可以简单 Hook 的统一 Table 写入函数。**

它有快速写入路径，不能只 Hook `luaH_finishset()`。

例如：

- `luaH_psetshortstr()` 可以直接修改已有字符串字段。
- `psetint()` 走整数快速写入。
- `luaH_fastseti` 可以直接修改 Array 部分。
- `luaH_finishset()` 负责慢速路径。
- `luaH_setint()` 有独立的整数写入路径。
- `luaH_newkey()` 负责创建新 Hash 节点。

这些路径需要分别审计。citeturn198220view0turn652550view0turn652550view1turn198220search5

## 6.1 推荐的源码修改范围

以官方 Lua 5.5.1 为基线：

| 源码文件 | 修改内容 | 优先级 |
|--------|--------|--------|
| `ltable.c` | Hash 写入观察 | P0 |
| `ltable.h` | Array 快速写入观察 | P0 |
| `lvm.c` | VM 写入路径完整性审计 | P0 |
| `lapi.c` | C API 写入路径审计 | P0 |
| `lgc.c` | Table 释放时清理 Sidecar | P0 |
| `lstate.h` | Runtime 扩展上下文 | P1 |
| `lstate.c` | Registry 初始化和释放 | P1 |
| `lobject.h` | 尽量不修改 | — |

我的建议是：不要在每个 VM 指令里复制一套 Apollo 逻辑，而是将 Observer 封装成 Lua Core 内部的统一辅助接口。

## 6.2 两阶段写入通知

建议 Observer 提供两个阶段：

```cpp
enum class MutationDecision {
    Accept,
    Reject
};

class LuaMutationObserver {
public:
    MutationDecision beforeWrite(
        lua_State* L,
        Table* table,
        const TValue* key,
        const TValue* old_value,
        const TValue* new_value
    );

    void afterWrite(
        lua_State* L,
        Table* table,
        const TValue* key,
        const TValue* old_value,
        const TValue* new_value
    );
};
```

实际生产实现不建议让这个接口直接暴露给普通插件，而是 Lua Core 内部调用的 C++ 扩展接口。

流程：

```text
准备写入
   |
   v
检查 Table 是否受 Apollo 托管
   |
   +---- 否 ---> 原生 Lua 写入
   |
   v
读取旧值
   |
   v
Schema / 权限 / 类型校验
   |
   +---- Reject ---> 取消写入
   |
   v
执行 Lua 原始写入
   |
   v
GC Barrier
   |
   v
记录 Mutation
```

这里有一个细节：`beforeWrite` 不能执行 Lua 回调，也不能允许抛出 Lua 异常后留下半完成的 C++ 事务。

它只能做受控校验。

`afterWrite` 也不能直接触发玩法逻辑，只负责记录已经发生的变化。

### 6.3 为什么需要 beforeWrite？

假设：

```lua
entity.health = \

## 6.1 推荐的源码修改范围

以官方 Lua 5.5.1 为基线：

| 源码文件 | 修改内容 | 优先级 |
|---|---|---|
| `ltable.c` | Hash 写入观察 | P0 |
| `ltable.h` | Array 快速写入观察 | P0 |
| `lvm.c` | VM 写入路径完整性审计 | P0 |
| `lapi.c` | C API 写入路径审计 | P0 |
| `lgc.c` | Table 释放时清理 Sidecar | P0 |
| `lstate.h` | Runtime 扩展上下文 | P1 |
| `lstate.c` | Registry 初始化和释放 | P1 |
| `lobject.h` | 尽量不修改 | — |

我的建议是：不要在每个 VM 指令里复制一套 Apollo 逻辑，而是将 Observer 封装成 Lua Core 内部的统一辅助接口。

## 6.2 两阶段写入通知

建议 Observer 提供两个阶段：

```cpp
enum class MutationDecision {
    Accept,
    Reject
};

class LuaMutationObserver {
public:
    MutationDecision beforeWrite(
        lua_State* L,
        Table* table,
        const TValue* key,
        const TValue* old_value,
        const TValue* new_value
    );

    void afterWrite(
        lua_State* L,
        Table* table,
        const TValue* key,
        const TValue* old_value,
        const TValue* new_value
    );
};
```

实际生产实现不建议让这个接口直接暴露给普通插件，而是 Lua Core 内部调用的 C++ 扩展接口。

流程：

```text
准备写入
   |
   v
检查 Table 是否受 Apollo 托管
   |
   +---- 否 ---> 原生 Lua 写入
   |
   v
读取旧值
   |
   v
Schema / 权限 / 类型校验
   |
   +---- Reject ---> 取消写入
   |
   v
执行 Lua 原始写入
   |
   v
GC Barrier
   |
   v
记录 Mutation
```

这里有一个细节：`beforeWrite` 不能执行 Lua 回调，也不能允许抛出 Lua 异常后留下半完成的 C++ 事务。

它只能做受控校验。

`afterWrite` 也不能直接触发玩法逻辑，只负责记录已经发生的变化。

### 6.3 为什么需要 beforeWrite？

假设：

```lua
entity.health = \

# 7. Table 注册与自动追踪

## 7.1 注册入口

我建议不让 Lua 自己注册 Table，而是由 Apollo Entity Binding 自动完成。

例如：

```cpp
void LuaEntityBinding::pushEntity(
    lua_State* L,
    EntityId entity_id
);
```

当 C++ 将 Entity 交给 Lua 时：

1. 创建 Entity Root Table。
2. 创建 Root Metadata。
3. 注册 Table Identity。
4. 创建 Schema Binding。
5. 将 Entity Table 放入 Lua 栈。
6. 返回给 Lua。

Lua 使用：

```lua
entity.health = 100
```

完全不需要知道 Sidecar 的存在。

## 7.2 自动注册嵌套 Table

假设：

```lua
entity.stats = {
    attack = 100,
    defense = 50

```

新 Table 挂到 Tracked Table 上时，需要识别其类型。

建议执行：

```text
Tracked Parent
      |
      v
发现 New Value 是 Table
      |
      v
读取 Parent Schema
      |
      v
检查字段类型
      |
      v
创建 Child Metadata
      |
      v
建立 Ownership
      |
      v
提交写入
```

对于 Schema 未声明的字段，默认拒绝。

如果是 Map 类型，则可以允许动态 Key，但必须明确 Map 的序列化规则。

## 7.3 懒注册与预注册

我建议混合使用：

- Schema 固定的 Struct：创建 Entity 时预注册。
- Struct 内的固定 Child Table：可以预先创建。
- 大型 Array：按需注册实际存在的 Slot。
- Map：插入时注册对应 Entry。
- 临时 Lua Table：永远不注册。

这样能够避免 Entity 创建时大量分配 Sidecar。

---

# 8. 深度变化路径与 Schema 映射

这里我建议不要每次修改都遍历整个 Ownership Graph。

每个 Child Metadata 缓存路径信息：

```cpp
struct TableBinding {
    EntityId entity_id;

    uint32_t root_attr_id;
    uint32_t schema_id;

    uint32_t parent_field_id;

    LuaTableId parent_id;
};
```

如果需要更深的字段定位，采用 Schema 的静态路径索引。

例如：

```yaml
attributes:
  stats:
    id: 10
    type: struct
    fields:
      combat:
        id: 1
        type: struct
        fields:
          attack:
            id: 101
            type: int32
          defense:
            id: 102
            type: int32
```

Lua：

```lua
entity.stats.combat.attack = 200
```

Observer 得到：

```text
root_attr_id = 10
field_path   = [1, 101]
```

Schema 解析后：

```text
effective_attr_id = 101
```

建议 Schema 编译阶段生成路径查找表：

```cpp
struct CompiledField {
    uint32_t field_id;
    uint32_t parent_field_id;

    ValueType type;

    uint32_t wire_attr_id;
};
```

这样 Observer 只需通过当前 Table 的 Schema 和 key，快速找到字段定义。

**不建议每次 mutation 都动态生成字符串路径，例如 `stats.combat.attack`。** 字符串路径只适合日志和调试。

## 8.1 数组

例如：

```lua
entity.skills[3].level = 5
```

建议：

```text
root_attr_id = 20
slot_index   = 3
field_id     = 2
```

需要明确：

- `skills[3] = skill`：替换整个 Slot。
- `skills[3].level = 5`：修改 Slot 的一个成员。
- `table.insert`：结构性插入。
- `table.remove`：结构性删除。
- `table.sort`：数组重排。

这些操作不能全部转换成普通字段 Update。

特别是数组重排，需要定义同步语义。对于游戏技能槽这类固定索引结构，我建议禁止隐式重排；对于背包等动态数组，单独设计 Insert/Remove/Move 操作。

# 9. GC 回收的实际源码接入

我再补充一个非常关键的源码细节。

Lua 5.5.1 的 `lgc.c` 中，`freeobj()` 在释放 Table 时会调用：

```c
case LUA_VTABLE:
    luaH_free(L, gco2t(o));
    break;
```

这提供了一个非常合适的清理入口。citeturn652550view2

我建议在这里增加 Apollo 的 Table 回收通知：

```cpp
static void freeobj(lua_State* L, GCObject* o) {
    switch (o->tt) {
        case LUA_VTABLE: {
            Table* table = gco2t(o);

            // Apollo extension:
            // Remove identity and retire metadata.
            apollo_table_on_gc_free(L, table);

            luaH_free(L, table);
            break;

        // Original Lua cases...


```

这是示意代码，实际应保持官方 `freeobj` 的原有结构。

`apollo_table_on_gc_free` 必须满足：

- 不调用 Lua API。
- 不执行 Lua 回调。
- 不进行 Lua GC。
- 不分配 Lua 对象。
- 不操作已经失效的 Table 内容。
- 只清理 IdentityMap 和 Sidecar 生命周期。

## 9.1 弱引用注册表

同时保留一个 Lua Registry 弱键表：

```lua
setmetatable(weak_registry, {
    __mode = \

# 10. Attribute Transaction 的衔接

Sidecar 不应该直接依赖 `AttrSyncManager`。

我建议增加一个中间层：

```text
Lua Runtime
    |
    v
Mutation Observer
    |
    v
Mutation Journal
    |
    v
Lua Attribute Adapter
    |
    v
Attribute Transaction
    |
    +-- ChangeHistory
    +-- ChangeSink
    +-- Dirty Set
    +-- Hook Window
    +-- DeltaBuilder
```

这样 Lua 只是一个属性修改来源。

未来 C++、Python 或其他脚本运行时也可以使用相同的 Attribute Transaction。

## 10.1 Lua Attribute Adapter

```cpp
class LuaAttributeAdapter {
public:
    void consume(
        EntityId entity_id,
        MutationJournal& journal
    );

private:
    void validate(
        const MutationRecord& mutation
    );

    void apply(
        const MutationRecord& mutation
    );

    void markDirty(
        const MutationRecord& mutation
    );
};
```

这里要区分两个概念：

- Lua 原始 Table 已经被修改。
- Apollo 的权威属性事务已经提交。

我的建议是让 Tracked Table 的写入本身成为事务的受控入口，`afterWrite` 记录待提交事件，帧末只负责消费和分发，而不是等帧末才发现非法写入。

对于存在跨字段约束的复杂修改，使用显式 Batch Transaction：

```lua
entity:beginAttrBatch()

entity.health = 100
entity.stats.attack = 200
entity.stats.defense = 300

entity:endAttrBatch()
```

提交时统一检查跨字段约束。

如果失败，Apollo 必须能回滚这批修改。可以采用 Undo Journal 保存旧值，而不是让帧末的普通 Hook 再去修改原始数据。

## 10.2 不要重复维护属性真相

这一点非常重要。

不要同时维护：

```text
Lua Table Value
C++ Entity Attribute Value
Apollo Shadow Attribute Value
```

三个互相独立的属性副本。

否则迟早会出现：

```text
Lua health = 100
C++ health = 120
Delta health = 110
```

推荐：

- 如果 Entity 的属性以 C++ 存储为权威：Lua Table 是受控视图，写入最终提交到 C++。
- 如果 Entity 的属性以 Lua Table 为权威：C++ 只消费提交后的变更，并从 Lua 读取当前值。
- 不允许两边都可以独立修改同一个属性副本。

考虑 Apollo 现有的 AttrSyncManager，我更推荐第一种，或者保留现有权威存储模式，只把 Lua Table 作为它的受控绑定视图。

---

# 11. 属性序列化如何接入

Sidecar 的 ID 绝对不能直接进入网络协议。

建议分成三种 ID：

| ID | 生命周期 | 用途 |
|--------|--------|--------|
| `LuaTableId` | Lua Runtime | Table 身份 |
| `MetadataHandle` | Sidecar 生命周期 | 查找元数据 |
| `AttrId / FieldId` | Schema 生命周期 | 网络、持久化和属性识别 |

例如：

```lua
entity.skills[3].level = 5
```

Observer 识别出：

```text
LuaTableId = 102
field_id   = 2
slot       = 3
```

Schema 转换：

```text
EntityId = 1001
AttrId   = 20
FieldId  = 2
Slot     = 3
```

DeltaBuilder 最终只接收协议定义的属性标识和数据。

这里需要根据 Apollo 当前 FrameCodec 的实际能力做一个选择：

**固定结构：**

在 Schema 编译阶段将深度字段打平为稳定的 `attr_id`，不需要改变现有整数属性域。

**动态数组：**

如果现有协议只支持单独的 `attr_id`，那么有两个选择：

1. 为固定长度数组预分配稳定的属性 ID。
2. 为动态数组增加 `attr_id + slot_index + field_id` 的复合寻址格式。

我更推荐第一版优先支持固定结构和固定槽位数组，动态数组后续扩展。

字符串继续使用 slice 字节编码，浮点采用 Schema 定义的定点精度，绝不发送 Table 地址、MetadataHandle 或 Lua 内部对象。

---

# 12. 性能设计

Sidecar 不能让 Apollo 所有 Lua Table 的读写都付出额外成本。

我建议把性能优化分成几个层次。

## 12.1 普通 Table 零额外追踪

```lua
local temp = {
    x = 10,
    y = 20

temp.x = 100
```

这类普通 Lua Table 不在 IdentityMap 中。

Observer 查询时发现未注册，直接走原生写入路径。

## 12.2 Tracked Table 快速路径

```cpp
MetadataHandle handle;

if (unlikely(observer_enabled)) {
    if (registry.find(table, handle)) {
        observer.record(handle, key, old_value, new_value);


```

注意这只是示意。

生产实现应尽可能把普通 Table 的检查合并到少数写入位置，而不是让每个 VM 指令重复查询多次。

## 12.3 Dirty 合并

不要在 Observer 中进行：

- 网络编码。
- 字符串路径拼接。
- Lua 回调。
- 数据库操作。
- 完整 Schema 递归验证。
- 跨 Entity 通知。

这些全部放到 Attribute Transaction 或后续消费者中。

## 12.4 预分配

建议预分配：

- Metadata Pool。
- IdentityMap。
- Mutation Journal。
- Dirty Set。
- String Snapshot Arena。

发生容量不足时明确报告，不要在 Lua VM 热路径中无限扩容。

## 12.5 需要测量的性能指标

| 指标 | 测试内容 |
|--------|--------|
| 普通 Table 写入 | 与未修改 Lua 5.5.1 对比 |
| Tracked Table 写入 | String Key / Integer Key |
| 深度写入 | 2、4、8 层 |
| Metadata 查询 | IdentityMap 命中率 |
| GC | 大量 Tracked Table 创建和释放 |
| Mutation Journal | 每帧 1K、10K、100K 次变更 |
| Entity 创建 | 不同属性规模 |
| 内存 | 每个 Tracked Table 的 Sidecar 成本 |

建议将普通 Table 的性能退化控制在测量噪声范围内；Tracked Table 则以每次写入的额外成本和 GC 压力作为单独优化目标。

---

# 13. 边界与安全规则

我建议把下面这些限制写入 Apollo 的设计规范。

| 行为 | 规则 |
|--------|--------|
| 修改普通 Table | 完全允许 |
| 修改 Tracked Table | 经过 Observer |
| `rawset` | 必须被捕获 |
| C API 修改 | 必须被捕获 |
| 修改 Tracked Table 元表 | 禁止 |
| 共享托管 Child Table | 禁止 |
| 循环引用 | 禁止 |
| 删除字段 | 生成 Delete |
| 替换整个 Struct | 生成 Replace |
| GC 回收 | 自动清理 Sidecar |
| Entity 销毁 | 主动注销整棵追踪树 |
| Lua State 销毁 | 清空整个 Registry |
| Journal 溢出 | 禁止静默丢失 |

另外，`debug` 库需要特别处理。不能让脚本通过 `debug.setmetatable` 等接口绕过托管属性的约束。

对于 C API，不能只审查 `lua_rawset`。还需要覆盖：

- `lua_settable`
- `lua_setfield`
- `lua_seti`
- `lua_rawset`
- `lua_rawseti`
- Lua 标准库的数组修改操作
- VM 内部的 Table 构造和赋值指令

这些 API 和底层路径需要做一次完整调用图审查，确保不会漏掉写入入口。

# 14. 推荐的 Apollo 代码组织

我建议把 Lua Runtime Patch 和 Apollo 属性同步模块分开。

```text
apollo/
├── third_party/
│   └── lua-5.5.1/
│       ├── src/
│       └── patches/
│           └── apollo/
│               ├── 0001-table-observer.patch
│               ├── 0002-gc-lifecycle.patch
│               └── 0003-runtime-context.patch
│
├── src/
│   ├── lua/
│   │   ├── runtime/
│   │   │   ├── lua_runtime.cpp
│   │   │   ├── lua_table_observer.cpp
│   │   │   └── lua_gc_bridge.cpp
│   │   │
│   │   ├── tracking/
│   │   │   ├── table_registry.cpp
│   │   │   ├── metadata_pool.cpp
│   │   │   ├── ownership_graph.cpp
│   │   │   └── mutation_journal.cpp
│   │   │
│   │   └── binding/
│   │       ├── entity_binding.cpp
│   │       ├── attribute_binding.cpp
│   │       └── schema_binding.cpp
│   │
│   └── attribute/
│       ├── attribute_transaction.cpp
│       ├── attribute_schema.cpp
│       └── attribute_change_router.cpp
│
└── tests/
    ├── lua/
    │   ├── test_table_observer.cpp
    │   ├── test_rawset.cpp
    │   ├── test_nested_tracking.cpp
    │   ├── test_gc_lifecycle.cpp
    │   ├── test_ownership.cpp
    │   ├── test_array_mutation.cpp
    │   └── test_mutation_journal.cpp
    │
    └── attribute/
        ├── test_lua_cpp_consistency.cpp
        └── test_attribute_transaction.cpp
```

这里我特别建议保留独立 Patch 文件，而不是直接修改 `third_party/lua-5.5.1/src` 后就不再区分上游和 Apollo 的改动。

以后升级 Lua 5.5.x，可以重新应用 Patch 并运行兼容性测试。

# 15. 实施顺序

不要一开始就接入整个游戏实体系统。我建议先做一个独立的 Lua Runtime 实验。

1. **P0：Sidecar 基础设施**
   
   - [ ] Lua 5.5.1 独立编译。
   - [ ] Registry 弱键表。
   - [ ] Metadata Pool。
   - [ ] IdentityMap。
   - [ ] Generation Handle。
   - [ ] Table GC 释放通知。
   - [ ] Runtime 销毁清理。
2. **P0：Mutation Observer**
   
   - [ ] String Key 更新。
   - [ ] String Key 插入和删除。
   - [ ] Integer Key 更新。
   - [ ] Array 快速写入。
   - [ ] Hash 快速写入。
   - [ ] `rawset`。
   - [ ] C API。
   - [ ] VM 指令写入。
   - [ ] Table 构造路径。
   - [ ] 标准库批量修改。
3. **P1：嵌套追踪**
   
   - [ ] Struct 自动注册。
   - [ ] Ownership Graph。
   - [ ] Child 替换。
   - [ ] Child 脱离。
   - [ ] 共享引用检测。
   - [ ] 循环引用检测。
   - [ ] Schema 路径解析。
4. **P1：属性事务**
   
   - [ ] ValueSnapshot。
   - [ ] Mutation Journal。
   - [ ] Dirty Set。
   - [ ] BeforeWrite 校验。
   - [ ] Undo Journal。
   - [ ] 批量事务。
   - [ ] ChangeHistory。
5. **P2：Apollo 集成**
   
   - [ ] Entity Binding。
   - [ ] Attribute Schema。
   - [ ] AttrSyncManager。
   - [ ] DeltaBuilder。
   - [ ] Hook Window。
   - [ ] Persistence。
   - [ ] C++ / Lua 一致性验证。
6. **P2：性能和稳定性**
   
   - [ ] GC 压力测试。
   - [ ] Journal 溢出测试。
   - [ ] 长时间运行测试。
   - [ ] 多 Lua State 测试。
   - [ ] 普通 Table 性能对比。
   - [ ] Tracked Table 性能对比。

# 16. 最终技术决策

我把几个容易混淆的设计选择总结一下：

| 技术问题 | 最终选择 |
|--------|--------|
| 是否修改 Lua Table 结构 | 不修改 |
| 是否使用 Proxy | 不作为核心方案 |
| 是否使用弱引用 | 是 |
| 是否使用 C++ IdentityMap | 是 |
| 是否 Hook Lua GC | 是，仅清理生命周期 |
| 是否 Hook Table 写入 | 是，覆盖快速和慢速路径 |
| 是否追踪所有 Table | 否 |
| 是否支持深度变化 | 是 |
| 是否允许共享托管子表 | 否 |
| 是否在 VM 中调用 Lua 回调 | 绝对不允许 |
| 是否直接同步网络 | 否 |
| 是否沿用 AttrSyncManager | 是 |

## 17. 我认为还需要做的一个关键修正

之前的设计中有一个潜在问题：把 `luaH_finishset()` 当作主要写入观察点是不够的。

Lua 5.5.1 的快速写入可以直接完成修改，例如 `luaH_psetshortstr()` 和 `luaH_fastseti`。因此必须在实际写入位置覆盖，而不是只在慢速写入路径加 Hook。citeturn198220view0turn198220search5turn198220search1

此外，Sidecar 的 GC 清理、Mutation Journal 和 Attribute Transaction 必须独立测试，不能依赖业务测试间接证明它们正确。

**我的最终判断：Sidecar Metadata 非常适合 Apollo，但它应该被实现为 Lua Runtime 的通用对象追踪基础设施，而不是 Apollo 属性同步的临时辅助类。**

这样以后 Apollo 即使增加：

- Lua Entity Debugger
- Lua Table 热更新检查
- Entity 属性审计
- Lua 内存归属分析
- 属性变化回放
- 脚本热重载状态迁移

也可以复用同一套对象身份和生命周期系统。

官方源码入口：

- urlltable.c：Table 实现https://www.lua.org/source/5.5/ltable.c.html
- urllvm.c：VM 执行https://www.lua.org/source/5.5/lvm.c.html
- urllgc.c：GC 实现https://www.lua.org/source/5.5/lgc.c.html
- urllapi.c：C APIhttps://www.lua.org/source/5.5/lapi.c.html
- urlLua 5.5 官方手册https://www.lua.org/manual/5.5/

下一步最有价值的是先实现一个独立的 `lua-table-observer` 原型，用真实 Lua 5.5.1 源码证明所有写入路径都能被捕获，再将它接入 Apollo 的 `lua-attr-sync.md`。这能避免在整个属性系统上投入之后才发现底层监听存在遗漏。

---

If you want, I can:

- 继续描述变更追踪设计
- 介绍 Ownership Graph 实现细节
- 说明 Lua GC 与 Sidecar 互动机制