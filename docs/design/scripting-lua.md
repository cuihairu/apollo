# Apollo Lua 脚本层设计（scripting-lua）

> 状态：设计稿（评审中）。前瞻依据：仓库规划（docs/qa/q101-script-language.md 等）已把脚本语言方向定为 Lua。本设计与 `docs/analysis/ioc-review.md`（容器分工）、`docs/design/attribute-sync.md`（属性钩子/公式）、`docs/design/net-abstraction.md`（异步回主线程）、`docs/design/sdk-contract.md`（客户端契约）互为引用。

---

## 执行摘要

1. **分工一句话：动态归 Lua，硬实时归 C++。** 玩法逻辑（任务/技能行为树/AI/公式/掉落）进 Lua 以换热更与策划自助；网络收发、属性存储/同步、AOI、ECS、持久化留在 C++。这条边界决定后面一切接口设计。
2. **C++ 服务对 Lua 只开"启动期注入的具名模块表"**（`apollo.attr`/`apollo.db`/`apollo.scene`），绑定即校验、缺失在启动期报错——**禁止脚本按名字反查服务容器**（ioc-review.md P0-1 的字符串查找缺陷严禁在脚本层复制）。
3. **热替换协议**：工作线程预加载+编译 → 主线程 tick 边界一次原子换表 → 失败回滚旧表；模块带版本与依赖声明，灰度先行（单个 scene 试跑）。没有运行期增删 C++ bean（C++ 侧运行期注册/unregister API 随 ioc-review 删除）。
4. **属性钩子只有一个接入点**：`onAttrChanged(entity, id, old, new)` 挂在属性管线的"重算后、广播前"阶段（attribute-sync.md §10 tick 阶段 2/4 之间），白名单位 + 指令预算防脚本拖垮广播；脚本对"最终可见值"只读，对白名单做最终修饰。
5. **公式系统从 C++ 搬进 Lua 函数**：docs/03 的自研表达式编译方案废弃；派生属性公式 = Lua 函数 + 依赖图留在 C++（重算调度不依赖脚本引擎）。
6. **沙盒**：无 io/os 表、指令数预算/帧、内存上限、每实体脚本时间片轮转；脚本异常隔离（错误 → 日志 + 实体行为回退），不崩进程。

---

## 1. 定位：边界在哪里

| 维度 | C++（宿主层） | Lua（玩法层） |
|---|---|---|
| 责任 | 传输/存储/属性权威/同步/场景与 AOI/ECS/持久化/网络 | 任务、AI 决策、技能逻辑、公式、掉落、GM 校验、UI 数据拼装 |
| 热更 | 重启或灰度换版本 | 秒级热替换 |
| 写属性权限 | 任何属性（C++ 逻辑、权威） | 仅白名单字段（§4），否则只发"意图" |
| 性能纪律 | 无预算（但禁止锁/分配垃圾） | 指令预算 + 每帧时间片 |

判据：**该逻辑是否需要在运行中变更/由策划迭代？是 → Lua；否且高频 → C++。** 属性值存储本身永不进 Lua（多维：GC 压力、权威旁路、热更时数据归属不清）。

## 2. 宿主与运行时

- **一逻辑线程一 `lua_State`**（复用 `modules/base/thread_pool` 无关，直接依附于 scene/空间线程）：状态隔离、无互锁、热点无 OS 无关开销；实体数据留在 C++，Lua 侧只是"逻辑引用"（userdata/lightuserdata wrapper），不复制实体状态。
- **绑定层选型**：
  - 首选 sol2（header-only、支持安全调用约定、错误传播清晰），若引入第三方受限则用 150 行手写 `lua_CFunction` 薄绑定（本设计的接口面很小，两种都够）。
  - 不为"性能"做过度优化：跨边界调用频率上限是每帧每实体若干次意图调用，远低于 C++ 内部调用；真正高频路径（属性设置、移动）本来就不许过脚本。
- 每个 `lua_State` 预加载：标准库白名单（`base/string/table/math/bit32`）+ `apollo.*` 模块 + 错误处理框架（禁 `os/io/debug`，`load` 禁 bytecode）。

## 3. 模块加载与热替换

### 3.1 模块形态

```
modules/lua/
├── src/                # 共享 Lua 源（随版本打包）
├── dev/                # 开发热更目录（仅 dev profile 监控）
└── patch/<ver>/        # 线上补丁目录（灰度发布，按版本目录下发）
```
- 模块 = Lua 源文件 + 头注释声明的元信息（`--- module: quest, version: 42, deps: attr, ship`）。加载器（C++ 侧 ~200 行）扫描目录、校验依赖图、编译到独立 chunk（`luaL_loadbuffer`，不 join 全局）。
- **模块私有状态进模块表（upvalue），不进 `_G`**——热替换只换模块表引用，系统其他部件通过"模块句柄"（registry ref）取用；同一模块旧代码的残留引用在换表后的 GC 周期回收（引用计数不可行，靠版本判活跃）。

### 3.2 热替换协议（原子性与回滚）

```
控制面（GM 面板/运维工具）→ admin 消息 → 主线程：
  Phase A（工作线程，不打断逻辑）
    1. 读 patch 目录 → 校验 hash → 编译变更模块（含 deps 拓扑序）
    2. 冒烟：在新状态跑"自检用例集"（每模块必须带 smoke 测试，CI 同源）
  Phase B（主线程，tick 边界，O(1)）
    3. 全量替换涉及模块的表引用（旧表进"冷冻区"）
    4. 广播 `ModuleReloaded{module, version}`（脚本侧 onReload 钩子做重注册，如事件订阅重挂）
  Phase C（观察期）
    5. 灰度 scene 试跑 N tick（指令预算/错误计数监控）
    6. 超阈值 → 主线程换回旧表快照（冷冻区 → 活跃区），广播 reload 伪事件
```

- 同模块一帧内至多一次替换；替换期间到达的实体消息按"旧逻辑处理完当前帧"——确定性优先，不做细粒度事务（玩家感知为一帧延迟）。
- 属性/实体数据在 C++ 侧，**热替换不携带任何玩家状态**——这是"数据不进脚本"决策的直接红利。

### 3.3 与配置热更的协同

- L1 静态配置（attribute-sync.md §2.1）分版本目录下发；新旧版本并存（旧客户端的场景继续用旧配置），**配置版本随实体快照一起发给客户端**（`attr_schema_version`，见 attribute-sync.md §7.2）——防止热更配置后新旧实体语义漂移。
- 配置变更如需脚本配合（数值改版导致公式重写）由同一次补丁携带：先配置、后脚本，脚本版本高于配置版本才启用（版本偏序校验）。

## 4. C++ 服务暴露：接口形态与开销

### 4.1 启动期注入（唯一通道)

```
Server::start()（Bootstrap 阶段，见 ioc-review.md §6 阶段 4）
  1. 容器（core::di）装配完成 —— 服务对象全部存在
  2. binding 注册表：显式列出暴露面（白名单制，不是自动反射全部服务）
     apollo.attr   → AttrServiceAPI（绑定函数 ≤ 10 个：get/set/emit/dirty/onChange/...）
     apollo.scene  → SceneAPI（spawn/move/broadcast...）
     apollo.db     → DbAPI（query/execute + 协程式回调）
     apollo.net    → SessionAPI（send_event/subscribe）
  3. 每个模块表挂到 lua_State 根注册表，启动自检：调一次冒烟函数验证句柄有效
```

- **绑定即校验**：运行期 Lua 侧不存在"按字符串找服务"路径（无 `apollo.services.find("db")`）；模块表里缺什么函数在启动自检处一次性暴露，而不是运行期 nil 报错。
- 取出的是**稳定指针句柄**（lightuserdata 或自定义 metatable），非每调用新分配；绑定函数签名尽量薄（表参数/返回表），跨边界零拷贝（只转指针 + 共享 byte buffer）。

### 4.2 禁止事项（明确列为代码评审红线）

1. 禁止把 `ApplicationContext`（旧字符串容器）或任何"按名查服务"API 绑进 Lua（ioc-review.md P0-1）。
2. 禁止脚本直接调属性存储的写接口绕过白名单（§5 的钩子入口是唯一写途经）——否则权威、校验、审计三件事全部失效。
3. 禁止脚本持有 C++ 实体裸指针跨 tick 保留（实体可能释放）——Lua 侧实体句柄 = 弱引用 + 有效性查询 `entity:alive()`，失效访问返回错误而非 UB。
4. 禁止在阶段 3（simulate 之后）以外的 tick 阶段改属性（见 §5 钩子时序）——保持单写者纪律的时序可裁剪性。

## 5. 属性钩子：onAttrChanged 的接入形态

### 5.1 时序（与 attribute-sync.md §10 对齐）

```
simulate 阶段（C++/Lua 业务改属性、打脏）
recalc 阶段（派生属性重算，纯 C++ 依赖图；公式从 Lua 调用的例外见 §5.3）
  └─ 脚本钩子窗口：dirty 属性集固定后、广播收集前
     for each (entity, attr) in dirty:
        if attr in ScriptWriteWhitelist:
            new = lua:onAttrChanged(entity, attr, old, new)   // 只允许返回修饰值或不变
            （拒绝白名单外属性的脚本修改——返回值废弃 + 计数告警）
collect 阶段（§3.2 的 delta/快照组装）
```

- 钩子收到的 `old/new` 是 C++ 侧快照拷贝（两个 int64/一个 string 引用），**无锁**：同属 owning thread。
- **重入防护**：钩子内部调用 `apollo.attr.set()` 视为二次变更——落进**下一 tick** 的 dirty（当前 tick 集合已定），禁止在钩子里改同一属性（死循环保护：同一 (entity,attr) 每帧至多允许 N 次脚本修饰）。
- 监听注册：C++ 侧的 hook table 用 Lua ref 存回调（`apollo.attr.register_listener(entity_scope, handler)`），热替换时旧回调同旧表一起失效（新模块用 `onReload` 重挂——§3.2 事件钩子）。

### 5.2 开销控制

- 默认空载零调用：`ScriptWriteWhitelist` 为空位图时跳过整个钩子窗口（一张 64 位位图判空，O(1)）。
- 指令预算：整个钩子窗口每帧合计 ≤ X 条指令（默认如 200k，约 0.5ms 量级，按实测校准）；超预算 → 本帧剩余钩子跳过 + 计数告警，下一帧恢复。
- 订阅式发散的替代品：希望"属性变化驱动 AI"的系统不要人手一个 listener——在脚本层做**提升抽象**（`apollo.ai.watch(entity, attrs, handler)` 由框架层批量合并监听，避免 N 实体 × M 监听器全量扫描）。

### 5.3 公式即钩子的特例

- L4 派生属性的公式函数挂在同一回调机制的"recalc 前置"变体：`calc_attr(entity, id) -> value`。依赖图在 C++（拓扑序调度、环检、重算集），公式体在 Lua。
- 公式热替换 = 一次模块热替换（公式函数所在模块），依赖不变则重算集零变化（只对旧公式已验证的实体 lazily 重算——默认下一时刻自然用新公式，无迁移动作）。
- 性能：recalc 频率高的属性（如 HP 回复）公式留在 C++（白名单外，脚本无权改），需要脚本化的才走 Lua——**默认全 C++，脚本化是显式选择**。这是 §1 判据在公式上的落点。

## 6. 沙盒与性能预算

| 维度 | 机制 | 触发后的行为 |
|---|---|---|
| 指令数 | 每状态每帧预算（lua_sethook COUNT 细粒度两段：粗检每 10k 指令、精检超标） | 中止当前脚本调用 → 报错入日志 → 该实体回退默认行为（如站立不动） |
| 内存 | 每状态上限 + `lua_gc` 步长限制（增量 GC，不允许 full GC 卡帧） | 超限 → 拒绝新分配请求 → 该模块标记不健康，进入观察 |
| 错误 | C++ 侧 try-catch-lua 错误（`lua_pcall` 分类：语法/运行时/内存） | 运行时错误 → 日志 + 计数 + 行为回退；**不崩进程**，不掉 sku |
| 死循环 | 指令预算天然覆盖（区别于超时信号方案，无需多线程 watchdog） | 同指令超限路径 |
| 跨状态隔离 | 一场景一线程一状态；场景迁移 = 状态迁移（杀一个起一个，玩家数据在 C++ 不受影响） | — |

- 调试面：错误审计通道（错误摘要带模块名+版本+数据快照）进 `docs/design/attribute-sync.md` 的 BI 旁路，独立于游戏通道，运维可查"热更升级后某模块错误率突增"。
- 灰度联动：模块健康分（错误计数/预算超限率）驱动 §3.2 的回滚阈值。

## 7. 与其余设计的交集

| 关联设计 | 落点 |
|---|---|
| ioc-review.md | C++ 容器启动期装配一次、注入模块表；容器删除运行期注册后，热更只发生在脚本表层面，二者解耦且互不越界 |
| attribute-sync.md | 钩子窗口时序（§5.1）；公式钩子（§5.3）；配置版本随实体下发（§3.3 ↔ attr_schema_version） |
| net-abstraction.md | 脚本发起的异步操作（DB 查询/跨服请求）经会话层完成回调按 tick 边界 resume 协程（§异步模型）；Lua 对网络只看到"发消息/订阅消息/回调"三件套，背压与重连对脚本不可见 |
| sdk-contract.md | 属性/意图消息的契约同时约束脚本端（脚本写的字段必须是契约字段——脚本白名单从契约生成） |
| ssengine-reference.md | 异步 DB 模型与协程 resume 共用底座；定时器轮驱动脚本的 schedule（`apollo.timer.repeat`） |

---

*基线：apollo main @ 35a9c528。*