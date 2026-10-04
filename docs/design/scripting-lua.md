# Apollo Lua 脚本层设计（scripting-lua）

> 状态：设计稿（评审中）。前瞻依据：仓库规划（docs/qa/q101-script-language.md 等）已把脚本语言方向定为 Lua。本设计与 `docs/analysis/architecture-review.md`（容器分工）、`docs/design/attribute-sync.md`（属性钩子/公式）、`docs/design/net-abstraction.md`（异步回主线程）、`docs/design/sdk-contract.md`（客户端契约）互为引用。

---

## 执行摘要

1. **分工一句话：动态归 Lua，硬实时归 C++。** 玩法逻辑（任务/技能行为树/AI/公式/掉落）进 Lua 以换热更与策划自助；网络收发、属性存储/同步、AOI、ECS、持久化留在 C++。这条边界决定后面一切接口设计。
2. **C++ 服务对 Lua 只开"启动期注入的具名模块表"**（`apollo.attr`/`apollo.db`/`apollo.scene`），绑定即校验、缺失在启动期报错——**禁止脚本按名字反查服务容器**（architecture-review.md P0-1 的字符串查找缺陷严禁在脚本层复制）。
3. **热替换协议**：工作线程预加载+编译 → 主线程 tick 边界一次原子换表 → 失败回滚旧表；模块带版本与依赖声明，灰度先行（单个 scene 试跑）。没有运行期增删 C++ bean（C++ 侧运行期注册/unregister API 随 architecture-review 删除）。
4. **属性钩子只有一个接入点**：`onAttrChanged(entity, id, old, new)` 挂在属性管线的"重算后、广播前"阶段（attribute-sync.md §10 tick 阶段 2/4 之间），白名单位 + 指令预算防脚本拖垮广播；脚本对"最终可见值"只读，对白名单做最终修饰。
5. **公式系统从 C++ 搬进 Lua 函数**：docs/03 的自行开发表达式编译方案废弃；派生属性公式 = Lua 函数 + 依赖图留在 C++（重算调度不依赖脚本引擎）。
6. **沙盒**：无 io/os 表、指令数预算/帧、内存上限、每实体脚本时间片轮转；脚本异常隔离（错误 → 日志 + 实体行为回退），不崩进程。
7. **契约语义的运行时载体（sdk-contract §10.6 v3，2026-09-29 补）**：业务消息 handler 绑定与属性写白名单的数据来自 `contract.lua`（生成器同批吐的 Lua 契约表：attr 表/消息路由/白名单）——装载与热更走 §3.2 同一换表协议；服务端契约变更零 C++ 重编的业务面全在 Lua 侧承接（见 §3.4）。
8. **在线调试与性能归因（§7，2026-09-29 补）**：attach 执行 = **admin 单入口 + control 通道转发 + tick 边界沙盒 eval**（KBE telnet / skynet debug_console 先例；权限分 Passive Query / Controlled Action）；Lua 状态面四清单（内存 GC / 协程 / 模块版本 / env 采样）挂 observability 树 `/script` 分支；性能归因 = 指令 hook 双职能（预算执法 + per-module 耗时统计）+ C++ 侧外采（perf/Tracy 类，不自建）；死循环检测 §6 已有（指令预算天然覆盖）；**不做断点式调试器**（单写者线程冻结 + tick 确定性破坏）。
9. **异步任务模型与热更补强（§3.5/§8，2026-09-29 补）**：单写者线程下的异步封装 = **C++ 侧回调 + request_id 交接、Lua 侧协程**（§8.1 对比表定 future/promise 不引入——std::future 的 get 阻塞违 tick 纪律、JS Promise 的微任务语义由「协程 + tick 边界 resume」以更简形态获得；skynet.call 协程范式先例 skynet.lua:227）；三纪律 = resume 只在边界点 / resume 后 re-validate / 超时上界 + 协作式取消。热更补四件（§3.2）：current 版本指针持久化（防坏版本崩溃循环）、协程升级语义（旧协程跑完旧表）、灰度粒度 per 场景线程、模块私有状态零迁移纪律；§3.5 字节码缓存（构建期预编译 + manifest 三元组校验、Lua 版本头防错载）；§2 补 Lua 5.4 版本锁定（sol2 全支持；LuaJIT 仅实测瓶颈再评估）。
10. **绑定层与版本定案（2026-09-30 修订，同日再修订版本策略）**：弃 sol2（上游维护停滞——重模板编译成本 + Lua 版本升级强耦合的双风险），**原生 Lua C API 薄绑定**（`luaL_Reg` 函数表 + userdata 包装，数百行——绑定面小是前提）；**Lua 5.5 线、版本随 vcpkg**（先定 5.5.1，同日再修订为「锁 5.5 主线、补丁位跟 vcpkg lua port，当前 5.5.x」——不自行 pin 补丁版、不做 overlay port 自持；小版本升级仍显式批次；不落 5.4 中间态；number = int64/double 语义不变，battle-determinism 确定性纪律不受影响）；sdk-contract「sol2 桥」同步更名「C-API 搬运桥」（其 §10.6 附节更新注——桥本就是自写代码，去 sol2 只换搬运函数族）。
11. **GM/运营命令面（§7.5，2026-09-30 补）**：指令表注册表驱动（`gm_commands` 声明 + 参数 schema 校验——GM 面无 eval 权限）；写权限 = contract.lua `gm_write` 独立白名单（与 `predict` 分列，写走属性钩子同路不绕管线）；level 三级分级 + 高危双人复核（框架 AccessController 之上的业务粒度）；`gm_audit` 独立审计表（wall+tick 双写，拒绝也落）；GM 输入 = admin 会话 intent 流（battle-determinism 复算自动含）；管道与 attach 同路零新增。
12. **故障通知契约（§6.1，2026-09-30 补）**：三级事件（WARN 软阈值去抖 / FAULT 中止+回退 / TRIP 熔断）→ 冷路径单点 `onScriptFault`（每状态一个 C++ 固定函数位，非虚无注册，O(1) 三件事：ERROR 旁路日志 + MetricRegistry 计数器 + 熔断状态机）；**引擎不提供脚本实现的故障 SPI**（看门狗不依赖被看护者 / 热路径回调违 tick 纪律 / pcall 自处理已是业务通道）；模块级熔断（3 次/1000 tick → 隔离 600 tick，半开翻倍，热更换版本自动清零）；ScriptFaultEvent 字段封闭、wall+tick 双写；对外分发（webhook 等）全在 exporter 侧（logging §5.2）——故障路径禁同步 IO/HTTP/跨进程调用。

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
  - **原生 Lua C API 薄绑定（2026-09-30 定案：弃 sol2）**——上游维护停滞（2026-09-30 用户核查判定），重模板头文件编译成本高、且与 Lua 版本升级强耦合（第三方绑定库在新 Lua 版本上的适配永远是单点滞后）；apollo 的绑定面本来就小（§8 三件套 + `apollo.*` 注入表 + attr 访问器），手写 `luaL_Reg` 函数表 + userdata 包装 = 数百行可控，零第三方维护风险。sol2 的收益场景（大规模 class 模板绑定）在 apollo 不存在——「两种都够」的旧权衡在天平变险后定死为手写。
  - **Lua 版本锁定纪律（2026-09-29 补，2026-09-30 两度修订：定 5.5 线、版本随 vcpkg）**：**锁 5.5 主线、补丁位跟 vcpkg lua port**（当前 5.5.x）——首次嵌入即直接落 5.5 线，不落 5.4 中间态（无迁移包袱，且弃 sol2 后无绑定库兼容矩阵拖累版本选择）；具体版本不自行 pin 死、也不做 overlay port 自持（依赖单一来源：lua 与 openssl 等同走 vcpkg baseline，用户 2026-09-30 指令）。升级分两档：**补丁位（5.5.x 内）随 vcpkg 依赖解析浮动**——同 minor 内 bytecode 格式与 C API 稳定，§3.5 manifest 版本头校验守尾；**小版本（5.5→5.6）= 显式批次**：bytecode 格式头（§3.5）、C API 兼容面（自有绑定层——禁用 API 的编译器可见）、integer/浮点语义（number = int64/double——battle-determinism §2 确定性纪律）随版本一起动、带回归验证，不随依赖解析静默发生。LuaJIT 备选受限（语言子集 5.1 系 + 部分 5.3 扩展、bytecode 与 PUC Lua 不通用）——默认 PUC Lua；LuaJIT 只在 §7.3 归因实测瓶颈落在 VM 时再评估（先测后换，同「不自建 profiler」纪律）。
  - 不为"性能"做过度优化：跨边界调用频率上限是每帧每实体若干次意图调用，远低于 C++ 内部调用；真正高频路径（属性设置、移动）本来就不许过脚本。
- 每个 `lua_State` 预加载：标准库白名单（`base/string/table/math/bit32`）+ `apollo.*` 模块 + 错误处理框架（禁 `os/io/debug`；`load` 禁**未校验 bytecode**——运行期收到的一切字节串按源码处理；预编译产物例外走 §3.5 专用装载路径：构建管线产出 + hash/版本头校验，不经 `load`）。

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
- **回滚要持久化 current 版本指针（2026-09-29 补）**：内存换表只在进程活着时有效——回滚前崩溃会让重启重新装载已判坏版本（坏版本崩溃循环）。回滚动作同时回写持久 current 指针（patch 目录版本清单），重启默认装载「最新已知健康版本」。
- **协程升级语义**：换表时仍挂起的旧协程**继续在冷冻区旧表上执行至完成或超时**（异步协程本有超时上界，§8），新协程全部用新表——不做「热更中途改写运行中协程」（等价于断点式冻结，§7.4 同判）；旧协程持有的旧函数引用随完成自然消亡（与 §3.1 模块表 GC 回收同路）。
- **灰度粒度 = per 场景线程**：一状态一线程（§2）使换表可按场景线程独立执行——admin 命令投递进各目标场景线程任务队列、各自下一 tick 边界换（§7.1 attach 同型）；灰度即「部分 scene 先换 + 健康分观察 + 全量推广」，全局版本矩阵在 §7.2 状态面可查。（Phase B 的「主线程」在多场景线程下读作「目标场景线程」。）
- **模块私有状态零迁移纪律**：热更**不迁移**旧模块 upvalue 状态、也不提供 `onUpgrade(old_state)` 式迁移函数——需要跨版本保留的状态必须放 C++ 权威数据或 DB（§1「数据不进脚本」的推论）；模块 upvalue 只放可重建缓存/计数器。提供迁移设施 = 鼓励把权威数据搬进脚本，红线不开。

### 3.3 与配置热更的协同

- L1 静态配置（attribute-sync.md §2.1）分版本目录下发；新旧版本并存（旧客户端的场景继续用旧配置），**配置版本随实体快照一起发给客户端**（`attr_schema_version`，见 attribute-sync.md §7.2）——防止热更配置后新旧实体语义漂移。
- 配置变更如需脚本配合（数值改版导致公式重写）由同一次补丁携带：先配置、后脚本，脚本版本高于配置版本才启用（版本偏序校验）。

### 3.4 契约表装载（contract.lua，sdk-contract §10.6 v3，2026-09-29 补）

- `contract.lua` 是 apollo_gen 从契约源同批吐的 Lua 表（attr 表/消息路由/写白名单——semantic.json 同内容的服务端双形态）：`require` 即用，无编译环节（可选 luac 预编译随 Lua 发行版自带）。装载与热更走 §3.2 同一换表协议：新表编译 → 冒烟（路由完整性校验：bin 里有而 Lua 无 handler 即拒换——sdk-contract §10.6 装载期一致性闸在脚本侧的执行点）→ tick 边界原子换。
- **消息 handler 绑定**：上行业务消息经 C++ 帧路由按 `contract_route`（id→handler 名）分发到 Lua——handler 在模块表内注册，模块头注释声明 `handles: msg_a, msg_b`，加载器校验其与契约路由表一致，缺失启动红。
- **写白名单数据化**：§4/§5 的 `ScriptWriteWhitelist` 位图从 contract.lua 构建（白名单仍由契约 `predict` 位生成——sdk-contract §5 机制不变，载体从生成常量变为契约表数据）；热换契约表时白名单随 tick 边界同换。
- 版本偏序沿用 §3.3 同一规则：契约表版本 ≥ 消费它的脚本补丁版本才启用——契约加字段与使用该字段的 handler 补丁必须同批或先表后补。

### 3.5 字节码缓存与补丁形态（2026-09-29 补）

- **成本账**：每次启动与每次热更 Phase A 都要 `luaL_loadbuffer` 全量模块——模块到百级、单文件数 KB 时，整包编译是可感知的启动成本（重启路径上串行发生在装载线程）。缓存 = 把编译从运行期挪到构建期。
- **形态**：**patch 目录直接携带预编译 chunk**——构建管线对每模块产 `<name>.luac`（luac -o 或宿主 `string.dump`），manifest 记三元组 `(源 hash, bytecode hash, Lua 版本头)`；装载按 manifest 校验：源 hash 防篡改、**版本头不匹配即拒载并回退源码编译**（bytecode 格式与 Lua 小版本绑定——§2 版本锁定纪律的执行点）。dev 目录仍用源码（改完即载，缓存无收益）。
- **失效规则只有三条**：Lua 版本变 / 源变 / 装载器逻辑变——其余一律命中；不做版本清单之外的磁盘管理。
- **红线不变**：`load` 对**运行期字符串**禁 bytecode（§2）；缓存产物走专用装载路径（`luaL_loadbuffer` 的 bytecode 分支 + manifest 校验），不经 `load`——「禁 bytecode」禁的是不可信来源，不是自家构建产物。
- §3.4 contract.lua 同批预编译（变更低频但每次启动都载）。

## 4. C++ 服务暴露：接口形态与开销

### 4.1 启动期注入（唯一通道)

```
Server::start()（Bootstrap 阶段，见 architecture-review.md §6 阶段 4）
  1. 容器（core::di）装配完成 —— 服务对象全部存在
  2. binding 注册表：显式列出暴露面（白名单制，不是自动反射全部服务）
     apollo.attr   → AttrServiceAPI（绑定函数 ≤ 10 个：get/set/emit/dirty/onChange/...）
     apollo.scene  → SceneAPI（spawn/move/broadcast...）
     apollo.db     → DbAPI（query/execute + 协程式回调，底座 §8）
     apollo.net    → SessionAPI（send_event/subscribe）
  3. 每个模块表挂到 lua_State 根注册表，启动自检：调一次冒烟函数验证句柄有效
```

- **绑定即校验**：运行期 Lua 侧不存在"按字符串找服务"路径（无 `apollo.services.find("db")`）；模块表里缺什么函数在启动自检处一次性暴露，而不是运行期 nil 报错。
- 取出的是**稳定指针句柄**（lightuserdata 或自定义 metatable），非每调用新分配；绑定函数签名尽量薄（表参数/返回表），跨边界零拷贝（只转指针 + 共享 byte buffer）。

### 4.2 禁止事项（明确列为代码评审红线）

1. 禁止把 `ApplicationContext`（旧字符串容器）或任何"按名查服务"API 绑进 Lua（architecture-review.md P0-1）。
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

### 6.1 故障通知契约：三级事件、冷路径单点与模块熔断（2026-09-30 补，architecture-review §19.5.1 / P-4）

§6 表内「报错入日志 → 实体回退」只定义了**引擎内部**处置；「引擎 → 运维/业务」的对外通知契约（事件长什么样、谁消费、脚本层有没有可编程钩子）此前零设计，本节定型。立场先行：

**引擎不提供由脚本实现的故障 SPI（评审红线）**，三条理由：① 故障时刻脚本层是全场最不可信的组件——看门狗的回调不能依赖被看护者（预算超限的模块去执行 onFault，等于让病灶自己打报告）；② 热路径回调破坏 tick 预算与确定性（故障处置必须 O(1)，回调体内做什么无法约束）；③ 脚本自处理通道已存在——handler 用 pcall 自捕获是业务逻辑（自捕获即不触发 fault，语义现状已对），引擎契约不与它重叠。§19.5.1 的候选方向「冷路径单点」据此**落为只读出口**，不是脚本可实现的 SPI。

- **三级事件**（同一出口，级别决定动作）：

| 级别 | 触发 | 引擎内动作 | 对外 |
|---|---|---|---|
| WARN | 单次调用消耗 ≥ 预算 80% 且未超限 | 无（执行完毕） | 事件一条（去抖：同 (module,handler) 每 256 tick 至多一条） |
| FAULT | 超预算 / 运行时错误 / 沙盒违例 | 中止 + 实体回退（§6 表既有语义） | 事件一条 + 计数器 +1 |
| TRIP | 熔断触发/解除（见下） | 模块级隔离 | 事件一条（低频离散） |

- **ScriptFaultEvent 字段（结构化，进 ERROR 旁路日志行）**：`ts_wall_ms + tick` 双写（clock-and-time §7 纪律）、`scene`（场景线程标识）、`entity_id`（有实体上下文时）、`module + version`（§3.1 头注释元信息——「哪个版本的哪个模块」直接可答）、`handler`、`kind`（budget_exceeded / runtime_error / sandbox_violation / circuit_tripped / circuit_recovered）、`consumed/budget`（指令数，§7.3 hook 双职能顺带产出）、`action`（warned / aborted_rolled_back / quarantined）。字段集**封闭**——下游（collector/exporter/webhook）按字段路由，不解析自由文本。
- **出口 = 冷路径单点 `onScriptFault(const ScriptFaultEvent&)`**：每 `lua_State`（即每场景线程，§2）一个 C++ 侧固定函数位，非虚、无注册表、不可被脚本替换；默认实现三件事全部 O(1)：ERROR 旁路日志一行（logging §3.1——不进共享队列）、MetricRegistry 计数器自增（`script_fault_total{module,kind}` / `script_budget_warn_total{module}`）、熔断器状态机推进（下条）。**明令禁止**在故障路径做同步 IO/HTTP/跨进程调用——对外分发权在 collector/exporter 侧（logging §5.2 webhook 同一出口），引擎只产事件与计数器。
- **熔断器（模块级，防「同一坏模块每 tick 烧一遍」）**：同 module 在滑动窗 W=1000 tick 内第 M=3 次 FAULT → 标记 `quarantined` 持续 Q=600 tick——隔离期内该模块 handler 调用直接短路（走实体回退默认行为，不再进入 Lua）；解除走**半开**：隔离期满放行一次，再 FAULT 则 Q 翻倍（封顶一次长冷却，具体上限随容量批校准）；**热更新自动清零**（§3.2 换表即新版本新账——修好的模块不需要等冷却，这是「热更即修复通道」的直接红利）。熔断状态进 §7.2 状态面（隔离名单/剩余 tick/历史），W/M/Q 为启动期配置非硬编码。
- **幂等与聚合（对外不打爆的保证）**：引擎侧去抖只做 WARN 一级（上表）；FAULT 级靠**计数器语义**天然聚合——下游看到的是 `script_fault_total` 的速率而非逐条事件；TRIP 本身低频。告警阈值/静默窗口/限频是 exporter 侧规则（logging §5.2），不进引擎——引擎侧再配一套告警规则 = 两处真相。
- **确定性与回放**：fault 的**处置**（中止/回退/熔断隔离）是世界线程 tick 内的确定性状态迁移，复算重放自动复现；fault **事件本身**不进录制输入流（引擎自产观测数据，不是世界输入）——battle-determinism §5 的输入序列不含它，回放正确性不依赖「告警是否发生过」。

## 7. 在线调试与性能归因：attach 执行 / Lua 状态内省 / profile（2026-09-29 增补）

> 语义层以 `docs/architecture/observability-watcher-and-runtime-introspection-design.md`（architecture/ 代）为准——Watcher Tree、Passive Query vs Controlled Action 分级、AccessController；本节接脚本域三件事（attach 执行、Lua 状态面、性能归因）与一个立场（不做断点调试器）。通道与归属沿用 G-5 两截落位（net-abstraction §7 P3，architecture-review §16.8.3-⑤）：检测原语内嵌 ScriptHost（模块内），交互工具归 apps/ admin 面。

### 7.1 attach 执行：admin 单入口 + tick 边界沙盒 eval

- **先例**：KBE 每进程自带 telnet 服务，密码门禁 + 可配执行层（cellapp.cpp:292-293 `pTelnetServer_->start(telnet_passwd, telnet_deflayer, …)`），命令解码后进 Python 解释器**在线执行**（telnet_handler.cpp:801-812，`PyUnicode_DecodeUTF8(command)` 后 eval）；skynet debug_console 是一个 console 服务、`call` 任意服务在线执行 Lua（architecture-review §16.2，debug_console.lua:147-174）。
- **apollo 取舍**：不学「每进程一个监听口」——收敛为**单 admin 入口**（认证/审计/权限分级只做一处）+ control 通道转发到目标进程/目标场景线程（skynet 形态）；单进程阶段 admin 入口即本机。认证后默认关、只绑内网（KBE passwd 先例）。
- **执行语义（单写者纪律）**：调试命令投递进 owning 场景线程的任务队列，**下一 tick 的固定调试点**执行、结果回传——任何线程不直接摸 `lua_State`（§2 一线程一状态的推论；skynet 的 call 同构：console 投消息、目标服务在自己上下文执行）。
- **eval 环境 = 独立 debug env**：§6 沙盒白名单的受限超集——只读查询 API + 少量受控动作（GC 步进、模块健康复位）；无业务写权限（不给 `apollo.db` 写路径）；结果大小上限 + 速率限制（防误操作打爆控制通道）；指令预算同样管辖 eval（超时打断并上报——tick 确定性优先于调试便利）。
- **权限分级**（observability 文档口径）：状态查询 = Passive Query（认证后默认可用）；eval 执行 / GC 等动作 = Controlled Action（更高授权 + 审计日志——谁何时执行了什么，§6 错误审计通道的同型旁路）。

### 7.2 Lua 状态面（查什么——四清单）

| 面板 | 内容 | 出处/机制 |
|---|---|---|
| 内存与 GC | per-state 内存（`collectgarbage("count")`）、GC 步长/暂停参数、上限水位与超限事件 | §6 内存维度的读出口 |
| 协程清单 | 每协程状态（running/suspended/normal/dead）、挂起计数、等待的 wake 条件、超龄协程（泄漏检测） | 协程式异步底座（§8 异步模型；§4.1 `apollo.db` / §9 net 交集） |
| 模块与版本 | 已装载 chunk/模块表 + 每模块当前运行版本、历次热替换记录 | §3.2 热替换审计——「线上各模块跑的哪个版本」一查便知，版本漂移即热更事故定位 |
| env 采样 | 指定模块 env 顶层值只读快照（深度/字节数受限） | §3.1 模块私有状态的受控视察 |

- 语义层挂 observability 文档的 `/script` 路径分支：前三类 = Value/Collection 节点（Passive Query），eval = Action 节点（AccessController 管辖）；趋势类（错误率/预算超限率时间序列）归 MetricRegistry——「metrics=趋势、watcher=现状」口径照该文档。
- **同型先例（KBE watcher）**：子系统各自 `initializeWatcher()` 把值挂进路径树（serverapp.cpp:165-177：Network/Resmgr/threadPool/WatchPool 各自挂），远程经网络通道 `queryWatcher` 查询（:181-198），GUI 消费端是外挂工具 guiconsole/WatcherWindow——「检测原语在进程内、聚合呈现在外挂工具」与 G-5 两截落位同构。

### 7.3 性能归因（profile）：指令 hook 双职能 + C++ 侧外采

- **Lua 侧：同一个 hook 双职能**。§6 的 `lua_sethook` COUNT hook 本来就在热路径必装（预算执法）——归因统计是它的第二职能：per-module/per-handler 指令计数与墙钟累计分桶，tick 末聚合进 §7.2 状态面与 MetricRegistry。「哪个模块的钩子吃掉了 §5.2 的预算」「哪个公式最热（§5.3）」直接可答。增量 = 计数器分桶，不新增热路径机制（hook 成本已付）。
- **C++ 侧：不自建 profiler**。采样式外部工具（perf/Tracy 类）与单写者线程模型天然兼容——线程名即场景归因；模块义务只有两条：帧阶段标记（frame marker，对齐 attribute-sync §10 六阶段的采样区间）+ 符号不剥离。先例：BW bw_profile / KBE guiconsole 均为外挂工具进程（architecture-review §16.8.2 表——「聚合呈现在外挂工具」两家同型）。
- **死循环检测 = 已有设计**（§6：指令预算天然覆盖，区别于超时信号方案、无需多线程 watchdog）——本节只补一件事：超标事件从「日志 + 实体回退」升级为同时进状态面告警（哪个模块/哪个 handler 触发、当帧指令数）。

### 7.4 debug 能力边界（明确不做的，防未来再议）

- **不做断点式调试器**：断点挂起 = 单写者线程**整场景冻结**（该线程上全部实体的 tick 停摆）+ tick 确定性破坏（G-7 论证的反面）——MMO 逻辑服的调试形态是「在线 eval + 状态内省 + 离线审计（§6）」三件，不是「停世界打断点」。KBE telnet / skynet debug_console 同样只有 eval 与查询，无断点调试器。
- 预防式调试的另一半已在 §3.2：热替换前的工作线程冒烟（smoke 用例集 + 灰度 scene 试跑 + 健康分回滚）——新版本先在影子环境证明自己。
- 分期：本节全部跟随 G-5 同批（net-abstraction §7 P3 两截落位）——检测原语随 owning 模块落地，admin 交互面归 apps/；dev 期临时手段 = 日志 + §6 错误审计 + §3.2 版本审计。

### 7.5 GM/运营命令面（2026-09-30 补，design-gap-inventory #6）

§7.1-§7.4 是框架侧调试；玩法 GM（发道具/封禁/踢人/改属性）此前零设计（「GM」在设计文档中仅作为 admin 消息来源出现）——本节补三件：指令表/权限分级/审计存储。**管道零新增**：GM 命令与 attach 同路（admin 单入口 → control 通道 → 目标场景线程 **tick 边界**执行，§7.1）。

- **指令表 = 注册表驱动，不是 eval**：GM 命令是 Lua 模块内声明的具名函数（模块头注释 `gm_commands: give_item, kick, ban …`——§3.4 `handles:` 声明同型），参数带 schema（数量/类型/范围），C++ 侧分发前校验；**GM 面不给任意 eval 权限**（eval 是 §7.1 的 Controlled Action，高危且审计，不作为运营日常工作面）。指令注册表装载期校验（重名/未注册 handler 启动红），与 §3.4 装载闸同族。
- **写权限与契约的关系**：GM 可改属性面 = contract.lua 独立白名单 **`gm_write`**（与脚本 `predict` 白名单分列——GM 面 ⊇ 脚本面是配置事实，不是机制重叠）；GM 写走 `apollo.attr.set` 同一条钩子/审计路（§5）——**不提供绕过属性管线的直改通道**（权威/校验/审计三合一在钩子入口，§4.2 红线 2 的 GM 版）。只读查询（玩家属性/背包）走 §7.1 debug env 只读 API，零新增。
- **权限分级接 observability AccessController**：GM 命令声明 level（1 查询 / 2 操作 / 3 高危——封禁/回档/发币级），运营角色 → level 映射进配置；level 3 默认**双人复核**（发起 + 授权两账号；BW/KBE 无此层——apollo 加强项，部署可降级为单人 + 高频审计告警）。框架侧 Passive/Controlled 分级（observability 文档）不变，GM level 是其上的业务粒度。
- **审计存储**：每条 GM 命令落 **`gm_audit` 独立审计表**——（操作者账号, wall_ms + tick 双写（clock-and-time §7）, 目标实体/账号, 命令名, 参数快照, 结果码）；与 journal 分域（journal = 世界状态变更流，gm_audit = 运维行为流）；查询面挂 observability `/gm` 分支（谁何时对谁做了什么，一查便答）；**被拒命令同样落审计**（拒绝也是事件）。
- **确定性与回放**：GM 命令经 admin 会话上行 = 普通 intent 流的一类——battle-determinism §5 输入录制（session=admin）天然含 GM 输入，复算重放自动包含 GM 干预；GM 改世界与玩家操作在判定域同权（tick 边界输入），不破确定性。
- **踢人/封禁的执行位**：会话断开 = net 层 close（net-abstraction §2 四动作）；封禁 = login-app 准入闸门消费的风控记录（violation_score 同族，net-abstraction §4.3）——GM 命令是触发器，机制归各自域，不在脚本层重造。

## 8. 异步任务模型：单写者线程下的 Redis/DB/跨服（2026-09-29 补）

> 业务逻辑收敛到场景线程单写者（§2 / attribute-sync §10）后，「一次 Redis 读 / DB 查询 / 跨服调用」不能再原地阻塞等待——等待即卡整个场景的 tick。本节定架构层的异步封装：**C++ 侧回调 + request_id 交接，Lua 侧协程**，两层各自取最优模型。（§9 交集表原引的「§异步模型」即本节，悬空引用就此闭环。）

### 8.1 三种模型对比（为什么这么选）

| 模型 | 优点 | 缺点（游戏服语境） | 判定 |
|---|---|---|---|
| 回调 | 零依赖；控制流显式；引擎派发友好（完成统一在 tick 边界投递） | 回调地狱、错误处理分散；**生命周期陷阱**：回调触发前实体/会话可能已销毁（每个回调都要有效性校验） | **C++ 侧采用**（交接原语，§8.2） |
| Future/Promise | 值语义容器、可组合（when_all/then）；JS Promise 链 + async/await 观感好 | C++ `std::future` 无 then、`get()` **阻塞**——tick 内 get 即卡帧（红线）；带 then 的 future（folly/asio）需引入执行器框架，收益与复杂度不成比例；JS Promise 本质 = 微任务调度 + 状态机，其「每轮清空微任务队列」与「tick 边界 resume」是同构语义的两种实现——Lua 协程 + 固定调试点已是更简形态 | **不引入**（不引 future 框架；JS 风格观感在 Lua 协程里天然获得） |
| 协程 | 顺序代码写异步（脚本/策划友好）；**挂起零线程成本**——单线程上万个挂起协程不占 OS 线程，与单写者模型天作之合；挂起点显式（yield 即「等 IO」，超时/审计可挂在挂起点上） | 跨 yield 不变量：世界状态在挂起期间变了（实体死/下线/场景迁移）→ **resume 后必须 re-validate**；协作式不抢占 → 超时上界必须显式 | **Lua 侧采用**（C++20 协程不进引擎侧——C++ 侧异步点少且都是基础设施，复杂度不划算） |

**先例**：skynet 全协程原生——`skynet.call` 即挂起当前协程等 response 消息（lualib/skynet.lua:227 `coroutine_yield "SUSPEND"`、:18-26 resume/yield 原语绑定、:415 wakeup 派发），「消息到达 → 对应协程 resume」是其整个调度范式；BW/KBE 脚本侧在已读范围内均为回调式生命周期方法（onTimer/onXxx 族），无协程设施。apollo 取 skynet 的协程观感 + 自己的 tick 纪律（resume 只在边界点）。

### 8.2 分层封装（架构层级）

```
L2 脚本层（Lua 协程）   local r = apollo.db.query(...)   -- yield 挂起，返回时已有结果
                         apollo.async.all{a=..., b=...}    -- 并发组合：yield 一次等全部（合并超时）
L1 交接层（C++，场景线程） request_id → 完成回调 → 场景线程任务队列（与 net 上行同一条 MPSC 路）
L0 执行层（C++，IO/DB 线程池） hiredis 异步接口 / DB Command 队列 / InterServerLink RequestReply
                         ——阻塞或异步 IO 都不发生在场景线程
```

- **执行层**：Redis/DB/跨服的真实 IO 在独立线程池（modules/base/thread_pool）或异步客户端（hiredis event hook）上执行；连接池与 Command 队列归 modules/data（ssengine-reference §4.1 异步 DB Command 模型的第一消费者即本节）；跨服 RequestReply 走 InterServerLink（net-abstraction §5.7——其 §7 已定 RequestReply 仅限控制面，request_id 即本层对应物）。
- **交接层**：完成 → `(request_id, result/error)` 打包进 owning 场景线程任务队列——**交接物是消息不是 future**（跨线程 future 的共享状态是锁/原子同步，消息是单向投递，与 net-abstraction §3「IO 线程永不持游戏数据锁」同一条纪律）。C++ 业务侧确需异步时用同型回调 + request_id，不用 `std::future::get`（红线：tick 内任何阻塞等待）。
- **脚本层**：`apollo.db.query(...)` 等 API 内部 yield 并登记 request_id；完成回调进场景线程后在**固定调试点**（tick 边界，与 §7.1 attach 同一位点）resume 并注入结果/错误——对脚本呈现同步语义。协程登记表（句柄/挂起原因/超时/所属实体）即 §7.2 协程清单的数据源。

### 8.3 三条纪律（游戏服特有约束）

1. **resume 只在 tick 边界固定点**：不在 IO/DB 线程碰 `lua_State`（§2 一线程一状态的推论）——完成回调先入队，边界点统一 resume；确定性优先于延迟。
2. **resume 后 re-validate**：yield 期间世界可能已变——注入结果前校验实体 `alive()`、会话 active、场景未迁移（§4.2 红线 3 的协程版）；失效则协程以错误终止（脚本侧 pcall 捕获，走 §6 错误审计），不注入僵尸结果。
3. **超时上界 + 协作式取消**：每个挂起协程必带 timeout（默认随 API 类别配置：DB 秒级、跨服百毫秒级）；超时先到则以超时错误 resume（迟到结果丢弃 + 计数）；玩家下线/场景销毁时 pending 协程**执行至下一挂起点后以取消错误终止**——Lua 协程无法安全强杀，只能协作式收尾；超龄未回收即 §7.2 泄漏检测的对象。

## 9. 与其余设计的交集

| 关联设计 | 落点 |
|---|---|
| architecture-review.md | C++ 容器启动期装配一次、注入模块表；容器删除运行期注册后，热更只发生在脚本表层面，二者解耦且互不越界 |
| attribute-sync.md | 钩子窗口时序（§5.1）；公式钩子（§5.3）；配置版本随实体下发（§3.3 ↔ attr_schema_version） |
| net-abstraction.md | 脚本发起的异步操作（DB 查询/跨服请求）经会话层完成回调按 tick 边界 resume 协程（§8）；Lua 对网络只看到"发消息/订阅消息/回调"三件套，背压与重连对脚本不可见 |
| sdk-contract.md | 属性/意图消息的契约同时约束脚本端（脚本写的字段必须是契约字段——白名单由契约 predict 位生成、载体为 contract.lua，§3.4）；业务消息 handler 按契约路由绑定（bin↔路由逐条对齐的装载期闸） |
| ssengine-reference.md | 异步 DB 模型与协程 resume 共用底座；定时器轮驱动脚本的 schedule（`apollo.timer.repeat`） |
| observability-watcher-and-runtime-introspection-design.md（architecture/ 代） | §7 状态面挂其 Watcher 树 `/script` 分支（Value/Collection=查询、Action=受控执行、AccessController 权限分级）；「metrics=趋势、watcher=现状」口径沿用；检测原语/聚合工具两截归属同 G-5 |
| logging.md | §6 脚本错误审计与 §7.3 错误率计数经 BI 旁路在 collector 汇合（logging §5）；崩溃摘要的离线符号化依赖 §7.3 符号不剥离纪律 |

---

*基线：apollo main @ 35a9c528。2026-09-29 补 §3.4 与执行摘要 7（契约表 contract.lua 装载——sdk-contract §10.6 v3 的服务端语义载体：handler 路由绑定/写白名单数据化/换表协议同构/版本偏序）。同日增补 §7「在线调试与性能归因」（attach 执行/Lua 状态内省/profile——语义层引用 architecture/observability-watcher-and-runtime-introspection-design.md；先例 KBE telnet 在线 eval（telnet_handler.cpp:801-812、cellapp.cpp:292-293）+ KBE watcher 路径树（serverapp.cpp:165-181，guiconsole 消费端）+ skynet debug_console；原 §7 交集表顺移 §8，net-abstraction §2 的外部引用同步）。同日再补（第二批）：§2 Lua 版本锁定纪律（sol2 + Lua 5.4，LuaJIT 门槛）与 bytecode 红线精确化（禁未校验来源、预编译产物走专用路径）；§3.2 热更四补（current 版本指针持久化/协程升级语义/灰度粒度 per 场景线程/模块私有状态零迁移）；新增 §3.5 字节码缓存（构建期预编译 + manifest 三元组 + 三条失效规则）；新增 §8「异步任务模型」（三模型对比定 C++ 回调+request_id、Lua 协程；先例 skynet lualib/skynet.lua:18-26/:227/:415；三纪律 re-validate/超时/协作式取消；ssengine-reference §4.1 与 InterServerLink §5.7 接线），原 §8 交集表顺移 §9（§7.2/§9 内部引用同步，§9 net 行的悬空「§异步模型」引用闭环）；摘要 9 联动。2026-09-30 修订（第三批）：§2 绑定层弃 sol2 定原生 Lua C API 薄绑定 + 版本锁 5.5.1（上游维护停滞判定，用户指令；绑定面小前提；§3.4 luac 表述去版本号）——摘要 10、battle-determinism §2 Lua 行、sdk-contract §10.6 附节更新注、docs/todo 批次 6 同步；36 号 #12/#19 与 deep-dive §12 的 sol2/Lua 5.4 历史表述修正登记 design-gap-inventory §3（随下一 36 号批次）。2026-09-30 增补（第四批）：§7.5 GM/运营命令面（design-gap-inventory #6——指令表/权限分级/审计存储三件；管道与 §7.1 attach 同路；gm_write 白名单与 predict 分列；gm_audit 独立审计表；GM 输入进 battle-determinism 复算）——摘要 11 联动。2026-09-30 再修订（第五批）：§2 版本策略从「锁 5.5.1」改「**5.5 主线随 vcpkg lua port（当前 5.5.x）**」——补丁位随依赖解析浮动（§3.5 版本头校验守尾）、小版本升级仍显式批次、删 overlay port 自持兜底（用户指令：版本跟着 vcpkg 走）——摘要 10、battle-determinism §2 Lua 行、sdk-contract §10.6 注、docs/todo 批次 6 同步；同批执行 36 号 #12/#15/#17/#19 与 deep-dive §12 历史表述修正（design-gap-inventory §3 与登记簿回填）。2026-09-30 增补（第六批，architecture-review §19.4 P-4）：新增 §6.1「故障通知契约」（三级事件 WARN/FAULT/TRIP + 冷路径单点 onScriptFault + 模块熔断 + ScriptFaultEvent 字段封闭集 + 幂等聚合与确定性分界；明令不做脚本实现的故障 SPI——§19.5.1 候选方向的正式落盘）——摘要 12 联动；§7.2/§7.3 状态面与计数器被 §6.1 引用为其消费面。*