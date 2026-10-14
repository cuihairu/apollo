---
title: Script 模块
---

# Script 模块

Lua 沙盒宿主（`apollo::script`），设计见 [scripting-lua.md](/design/scripting-lua)。动态归 Lua、硬实时归 C++；本模块只承载运行时底座，玩法逻辑面在后续批。

## 依赖

```cmake
find_package(unofficial-lua CONFIG REQUIRED)
target_link_libraries(my_target PUBLIC apollo::script)
```

- `unofficial::lua::lua`（vcpkg lua port，5.5 线锁定——设计稿 §2 版本纪律：锁 5.5 主线、补丁位随 vcpkg，不做 overlay port 自持）。
- 零 apollo 内部依赖；故障事件以注入 sink 上抛，不直接耦合日志库。

## ScriptRuntime（沙盒宿主）

```cpp
apollo::script::ScriptConfig cfg;
cfg.instruction_budget = 200000;   // per-call 指令预算（0 = 不限）
cfg.memory_limit_bytes = 1 << 22;  // 内存上限（0 = 不限）
apollo::script::ScriptRuntime rt(cfg);

rt.set_fault_sink([&](const apollo::script::ScriptFaultEvent& e) { /* ERROR 旁路日志 + 计数 */ });
rt.register_module("apollo.test", kApiFuncs);  // §4.1 启动期注入具名模块表
rt.call_string("boot.lua", source);            // pcall 隔离，失败返回 false 不崩进程
rt.get_global_bool("ready");
```

### 交付面

- **一状态一 lua_State**（§2：一逻辑线程一状态，无互锁）；自定义 allocf 做内存上限（超限拒绝分配 → Lua memory error）。
- **标准库白名单**：base/string/table/math/utf8/coroutine；io/os/debug 不打开。
- **base 危险面清除**：`dofile`/`loadfile`/`collectgarbage` 置 nil；`load` 替换为 `mode "t"` 文本装载（二进制 chunk 拒绝）。
- **指令预算**：COUNT hook 粗检 10k 步进，超限 `lua_error` 中止当前调用（死循环天然覆盖）。
- **故障隔离**：`call_*` 全走 `lua_pcall`，错误分类 `kSyntax`/`kRuntime`/`kMemoryLimit`/`kInstructionBudget` 入 `ScriptFaultEvent`（chunk 名 + 错误串），状态可继续。
- **具名模块注入**：`register_module(name, luaL_Reg*)` 走全局具名表（§4.1 白名单制，禁止脚本按名查服务容器）。
- **模块热替换**（§3.2）：`swap_module(ModuleSpec{name, version, deps}, source)`——依赖校验 → 编译（mode "t"）→ 冒烟（新表带 `smoke()` 则须返回 true）→ 全过才原子换 `_G[name]`，任一步失败旧表不动；旧活跃表进冷冻区（仅留最近一份），`rollback_module` 恢复并丢弃坏版本；换表成功调新表 `onReload(old)` 钩子（失败不回滚，记 fault），广播 `ModuleReloaded` 经 `set_reload_listener`；`active_modules()` 为 §7.2 版本矩阵。模块私有状态零迁移纪律照 §3.2（状态在 C++ 权威侧）。装载路径结构探测全 raw 读（冒烟/onReload/契约结构探针不经元表——`__index` 恶意 error 不再把错误抛出保护区崩进程）。
- **属性钩子窗口**（§5.1/§5.2）：`set_attr_hook("onAttrChanged")` 注册全局钩子函数（签名 `(entity, attr, old, new)`，返回整数修订值，nil/非整数 = 不修饰）；`run_attr_hook_window(changes, whitelist, frame_budget)` 在重算后广播前逐条调用——白名单外跳过计数、帧预算耗尽剩余跳过（`reset_attr_frame()` 下一帧恢复）、钩子错误保持原值记 fault 且状态可继续；负载为 C++ 侧快照拷贝（`ScriptRuntime::AttrChange`，int64 对，无锁——同属 owning thread）；无钩子窗口零调用。
- **模块熔断**（§6.1）：`call_module(module, func)` 模块 handler 调用（fault 归因到模块）；滑窗 W tick 内 M 次 FAULT → 隔离 Q tick（W/M/Q/max 启动期配置，默认 1000/3/600/4800）；隔离期调用直接短路返回 false 不进 Lua（`stats().circuit_rejects` 计数）；隔离期满半开放行一次，探针成功销账解除、再 FAULT 隔离跨度翻倍（封顶 max）；**热更新自动清零**（swap/rollback 成功即新版本新账）；宿主每 tick 喂 `set_tick(t)`；状态面子集 `is_quarantined`/`quarantine_remaining`/`quarantined_modules`（名单+剩余 tick）。
- **熔断状态持久化**（§6.1，防坏版本崩溃循环）：`export_breaker_state()`/`import_breaker_state(blob)`——隔离中条目（绝对 tick 折算剩余期）+ WARN 去抖账跨重启延续（防重启告警风暴）；小端 TLV（魔数+版本），格式不符（魔数/版本/截断/尾随字节）拒载且状态不动；宿主自管落盘（journal/状态文件）与重启注入。
- **契约表装载**（§3.4）：`load_contract(source)`——contract.lua（apollo_gen 同批吐的服务端双形态）编译 → 结构校验（routes 表必备）→ 路由完整性闸（bin 有 handler 而 Lua 无即拒换，handler 为全局名或一层 `module.func` 点路径）→ 原子换 `_G["contract"]` 与 C++ 数据面（任一步失败旧表不动）；数据面 `contract_routes()`（msg_id → handler）、`contract_write_whitelist()`（§3.4 写白名单数据化，直接喂 `run_attr_hook_window`）、`contract_version()`；`call_route(msg_id)` 契约路由分发（未注册 id / handler 不可达 / 执行错误返回 false）。
- **字节码缓存**（§3.5）：构建期 `lua_precompile` 工具产 `<name>.luac` + manifest 三元组（源 hash / 产物 hash / 格式签名——Lua 版本 + 类型尺寸集，跨小版本必不同）；运行期 env 门控只读装载（`APOLLO_LUA_BYTECODE_CACHE`，空/未设 = 关）。`run_chunk`/`swap_module`/`load_contract` 三装载点共用 `load_source`——三元组校验全过走 `luaL_loadbufferx` "b" 分支（`stats().cache_hits`/`cache_misses` 计数，缓存关闭不计数），任一不过（源变 / 签名变 / 产物缺失损坏 / 清单未命中）回退源码编译，装载永不因缓存失败；失效规则（Lua 版本变 / 源变 / 装载器逻辑变）全在构建期重跑管线。红线不变：沙盒 `load` 对运行期字符串仍拒 bytecode（mode "t"），缓存路径只服务自家构建产物、不经 `load`。
- **§6.1 事件面**：三级事件经 fault sink——WARN（单次消耗 ≥ 预算 × `warn_threshold_percent`（默认 80；0 = 关闭）且未超限，同 chunk 256 tick 去抖，不计 fault）、FAULT（既有分类计数）、TRIP/RECOVER（熔断触发/解除：半开销账、热更、回滚三路解除均发）；`ScriptFaultEvent` 结构化字段封闭集 `kind/chunk/message + ts_wall_ms/tick/consumed/budget`（引擎侧统一填充，下游按字段路由不解析文本）。
- **attach eval**（§7.1）：`eval_debug(source, result)`——admin 单入口投递、owning 线程 tick 边界由宿主择机调用；同一沙盒面（白名单/指令预算/内存上限全管辖，死循环可打断）；表达式优先、语句回退（REPL 惯例）；结果串截断（`debug_eval_result_max`，默认 4KB）+ 同 tick 次数限流（`debug_eval_max_per_tick`，默认 8，0 = 不限）；**失败面 result = 错误原文**（限流/语法/运行错——与成功结果同一出参，故障隔离不炸会话）。宿主侧执行队列已随 Scene 批交付（`Scene::enqueue_debug_eval` → tick 首拍固定调试点 → 按 id 回传，跨线程投递自持互斥）。
- **观测面**：`stats()`（调用/fault 分类计数/指令粗检累计/reload 成败/attr_hook 调用与跳过计数/WARN 去抖计数/熔断拒调计数/缓存命中与回退计数）、`memory_used()`、`reset_budget()`。
- **状态面只读内省**（§7.2）：`gc_state()`——宿主 allocf 计数 `used_bytes`（上限执法真值）+ `limit_bytes` + Lua 自视角 `lua_gc_kib` + 增量参数 `gc_pause/stepmul/stepsize`（`LUA_GCPARAM` 只读查询，value 传 -1；宿主恒增量模式，无公开 mode 读取 API）；`reload_history()` 热替换审计环（上限 64 条丢最旧）——swap 成功/拒载与 rollback 全入账（拒载也是事件：缺依赖/编译/求值/非表/冒烟拒，原因截断 256），记录 `LuaReloadRecord{name, from_version(0=首次装载), to_version, tick, success, reason}`，进程内即可查「各模块跑的哪个版本、怎么到的这个版本」；`env_snapshot(module, max_entries)` 模块 env 顶层深度 1 快照——`lua_next` 裸遍历（不走 `__pairs`/`__index`）、值类型化取值（不走 `lua_tolstring` 就地转换）、不调任何 Lua 函数，条目数上限截断（`truncated` 标志）、单值超 512 字节截断，模块名不在全局面 = 未找到。owning 线程 tick 边界调用（同 §7.1 eval 窗口纪律）；聚合呈现面归 observability 后续批。

### 边界（留后续批）

- 热替换工程面：工作线程预加载 + smoke 用例集（§3.2 Phase A）、current 版本指针持久化（防坏版本崩溃循环）、灰度 per 场景线程、协程升级语义宿主侧接线
- apollo_gen 吐 contract.lua 产物（当前 Lua 表形态手写，生成器接线随 protobuf 批）、版本偏序执法（契约版本 ≥ 脚本补丁版本）
- 公式即钩子（§5.3）
- ScriptFaultEvent 宿主上下文字段（scene/entity_id/module+version——随场景接线批补）
- attach transport（admin 单入口/control 通道转发到目标进程——admin/control 面批次）、GM 命令面（§7.5）、§7.2 协程清单（随 §8 异步底座批）
- `require`/package 面未打开（模块加载暂走宿主 `call_string` 面）

## 测试

`tests/test_script_runtime.cpp`（56 用例）：白名单可用、沙盒缺省、二进制 chunk 拒绝、指令预算中断与状态恢复、预算 per-call 重充、内存上限、pcall 错误隔离、fault sink、具名模块注入；热替换原子换表、失败保旧、依赖拒载、冒烟闸、onReload 钩子、冷冻区回滚、reload 广播；属性钩子修订值、白名单外跳过、错误保持原值、帧预算跳过与下一帧恢复、nil 不修饰、无钩子空窗口；熔断三次触发、滑窗过期、半开恢复、半开翻倍、热更清零、M 可配置、call_module 分发、缺 handler 记 fault；契约装载数据面、缺 handler 拒换、坏结构拒载、失败保旧、契约路由分发、契约白名单喂钩子窗口；WARN 80% 触发、256 tick 去抖、阈值下静默、阈值上调静默、下调触发、0 关闭、TRIP/RECOVER 事件、热更清账发 RECOVER、事件结构化字段；eval 表达式求值与语句执行、错误隔离、预算管辖、结果截断、同 tick 限流与次 tick 恢复；熔断状态导出导入往返、跨重启隔离延续、tick 折算、未隔离不持久化、去抖账跨重启、坏 blob 拒载。三树门禁（GAME_MODULE ON/OFF）+ sanitizer 全绿。

`tests/test_bytecode_cache.cpp`（10 用例）+ `lua_precompile` 冒烟：manifest 行解析（合法/坏 hex/字段缺失/注释行）、缓存命中计数与执行语义、三失效规则回退（源 hash 变 / 格式签名变 / 产物损坏）、过 hash 闸后 "b" 装载失败兜底回退、清单缺失与条目未命中回退、env 未设零计数、契约装载走缓存。

`tests/test_script_state.cpp`（11 用例）：GC 面宿主计数同源 `memory_used` + Lua 自视角与增量参数只读查询；reload 审计成功入账（首次 from=0）与 tick 溯源、五类拒载原因（缺依赖/编译/求值/非表/冒烟）与超长原因截断、rollback from/to 翻转、审计环上限 64 丢最旧；env 快照原始类型渲染（串/整数/浮点/布尔/整数键/table/function）、条目上限截断、模块缺席/零上限/空名/非表全局、元方法恶意表不触（`__pairs`/`__index`/`__tostring` error 表照常快照）、单值 512 字节截断；契约表 hostile `__index` 装载正常（装载路径 raw 探测）。
