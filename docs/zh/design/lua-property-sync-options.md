# Apollo Lua 属性/表变化感知：方案对比

> 2026-11-27 用户令：把几种方案研究后整理，说明各种利弊，不替用户拍板。
> 来源：外部评审 `docs/review/2026-11-27-lua-table-tracking.md`（ChatGPT 设计），
> 对照本仓现状 `docs/zh/design/{scripting-lua,attribute-sync,lua-attr-sync}.md`
> 与源码 `modules/game/attrsync`、`modules/script`、`modules/game/world/src/scene.cpp`。
> 外部评审是建议不是本仓事实；下表「仓内状态」已对照源码。

## 0. 问题定义（要解决的到底是什么）

Lua 脚本改一个属性/表字段后，引擎要做到：
1. **感知**：知道哪个实体的哪个字段变了（含深度嵌套 `self.bag[3].level = 4`）
2. **转换**：变成 Apollo 的属性变更（attr_id + 值 + 可见域），进现有 attrsync
3. **下发**：DeltaBuilder → FrameCodec → 客户端（现有链路已通）
4. **不误伤**：普通 Lua table 不付额外开销；GC 后 handle 不悬空；rawset/C API 不漏

难点全在 1 和 2 —— 3 已有，4 是各方案的代价差异。

## 1. 方案全集（五种）

### 方案 A：现状 —— C++ 宿主写 + Lua 钩子窗口修订
**机制**：C++ 写属性 → ChangeHistory/dirty → 帧末 Scene 开钩子窗口 →
`ScriptRuntime::run_attr_hook_window` 调 Lua `onAttrChanged(entity, attr, old, new)` →
Lua 返回修订值写回 → apply_attr_hook 正规落账。**Lua 不能主动写，只能改宿主写的值**。
- 仓内状态：**已实现且有测试**（`test_attr_sync_attr_hook.cpp` / `test_scene_attr_hook_bridge.cpp`）
- ✅ 利：零引擎改动；白名单（write_whitelist）+ 帧预算 + lua_pcall 保护 + 错误保持原值，安全边界完整；与 attrsync 同链路无第二真相
- ❌ 弊：**Lua 无法主动发起属性变更**（玩法脚本要改 hp 得绕回 C++）；负载只有 int64 四元组，**没有深度结构**；钩子是帧末批量非实时

### 方案 B：声明式 API 写（lua_write_whitelist + 路径参数）
**机制**：注入 `apollo.attr.set/add/get`，内部直调 `AttrSyncManager` 正规写；深度用路径参数
`apollo.attr.skills(entity, 3, "level", 4)` 显式表达，不开放自由 table 引用。
- 仓内状态：**设计**（`docs/zh/design/lua-attr-sync.md`），未实现
- ✅ 利：改动小（三个 C 函数注入）；写入即通知，复用现有 dirty/ChangeSink/钩子；白名单可拦核心权威属性；路径参数让深度可增量化（只脏叶子 id）
- ❌ 弊：**玩法脚本写法受限**（不能自然写 `self.hp = 8500` 或 `self.bag[3].level=4`，必须调 API）；路径语法需要每个容器手工设计；深度上限要约束

### 方案 C：BigWorld 式 PropertyOwner 冒泡
**机制**：脚本写 → `DataType::attach(value, owner, index)` → 容器值本身是 PropertyOwnerBase
子类 → 子变更 `onOwnedPropertyChanged(change)` 沿父链冒泡到 Entity → 按属性旗标分发。
深度变更 = path 序列（叶→根索引链）+ 叶子值，位压缩下发。
- 仓内状态：BigWorld 仓有完整实现（`lib/entitydef/property_owner.*` / `property_change.*`）；**apollo 无**
- ✅ 利：业界验证过的深度变更模型；path+叶子值增量精确（不整表重发）；旗标（OWN/OTHER/GHOSTED/BASE/PERSISTENT）声明期决定分发
- ❌ 弊：**要照搬整套 PropertyOwner/DataType attach 架构**（apollo 现在是 int64+slice，无 owner 链）；Python setattr 拦截不能直接搬到 Lua；工作量大

### 方案 D：Sidecar Metadata + Mutation Observer（外部评审推荐）
**机制**：不改 Lua Table 布局；弱键表（Lua 存活判定）+ C++ IdentityMap（快速查身份）+ Metadata Pool（sidecar 数据）+ Generation（防 handle 复用）；**改 Lua 5.5.1 C 源码**（ltable.c 快/慢写入路径、lvm.c、lapi.c、lgc.c）挂写入观察 → Mutation Journal → Attribute Transaction → 复用 AttrSyncManager。
- 仓内状态：外部评审，**未实现、未验证可行性**；apollo 用原生 Lua C API，未见改引擎 Lua 源码
- ✅ 利：能捕获**所有写入**（普通赋值/rawset/C API/stdlib/快速路径）；深度嵌套+GC 生命周期完整；是通用基础设施（调试器/审计/热迁移复用）；普通 table 零开销
- ❌ 弊：**要 fork/改 Lua 5.5.1 引擎源码**（污染维护、升级成本、独立编译、ABI 风险）；评审自己也说必须先做 `lua-table-observer` 原型证明所有写入路径可捕获；工作量和风险最高

### 方案 E：pyi 静态类型（非运行时同步，补充项）
**机制**：把 def 属性类型生成到 `entity_types.pyi` 的 class 里（`hp: int`），Pylance 静态显示，不参与运行时同步。
- 仓内状态：**已实现**（`entityTypeStubs.ts` 渲染 + 53 测试过），但真机生成链未确认生效
- ✅ 利：零运行时成本；编辑器直接显示类型/补全；与运行时方案不冲突
- ❌ 弊：**只解决"显示"，不解决"感知与同步"**；且要真机确认生成触发链（def 索引→mappings→写文件）

## 2. 对比矩阵

| 维度 | A 现状钩子 | B 声明式 API | C BigWorld owner | D Sidecar observer | E pyi 静态 |
|---|---|---|---|---|---|
| 解决"显示类型" | ❌ | ❌ | ❌ | ❌ | ✅ |
| Lua 主动写属性 | ❌ | ✅ | ✅ | ✅ | — |
| 深度嵌套变更 | ❌ | ✅（路径参数） | ✅（path） | ✅（自动追踪） | — |
| rawset/C API 捕获 | — | ❌（绕过API） | ❌（依赖 attach） | ✅ | — |
| 引擎改动 | 无 | 无 | 中（attach 链） | **大（改 Lua C）** | 无 |
| 工作量 | 已完成 | 小 | 中-大 | **大** | 已完成 |
| 真机风险 | 低 | 低 | 中 | **高** | 生成链未验 |
| 通用复用 | 低 | 低 | 中 | 高（基础设施） | 低 |

## 3. 组合建议（不是拍板，是给出可选搭配）

- **显示问题**（你当前最痛的）：单独走 E，先确认真机生成链生效——与运行时方案无关
- **Lua 主动写 + 深度**：B 是低成本路径；C 是架构完整路径
- **全自动追踪（连 rawset 都不漏）**：只有 D 能做到，但代价是改 Lua 引擎源码——评审建议先做原型验证再决定
- 现实搭配：**E（显示）+ B（写）+ A（钩子校验）**，D 作为长期研究原型，C 作为参考模型

## 4. 需要你拍板的点

1. 当前优先解决**显示**（E）还是**运行时同步能力**（B/C/D）？
2. 深度嵌套是否必须支持？如果只在 `__init__` 里赋值标量，B 足够；如果玩法频繁改 `self.bag[i].field`，要 C 或 D
3. 是否接受改 Lua 5.5.1 源码（D 的前提）？这是最大分水岭
4. 评审的 `lua-table-observer` 原型要不要先做（验证 D 可行性）？评审自己推荐这个作为第一步

> 本表不替你选。你指哪个方向，我再出对应的落地计划。
