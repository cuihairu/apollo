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
- **模块热替换**（§3.2）：`swap_module(ModuleSpec{name, version, deps}, source)`——依赖校验 → 编译（mode "t"）→ 冒烟（新表带 `smoke()` 则须返回 true）→ 全过才原子换 `_G[name]`，任一步失败旧表不动；旧活跃表进冷冻区（仅留最近一份），`rollback_module` 恢复并丢弃坏版本；换表成功调新表 `onReload(old)` 钩子（失败不回滚，记 fault），广播 `ModuleReloaded` 经 `set_reload_listener`；`active_modules()` 为 §7.2 版本矩阵。模块私有状态零迁移纪律照 §3.2（状态在 C++ 权威侧）。装载路径结构探测全 raw 读（冒烟/onReload/契约结构探针不经元表——`__index` 恶意 error 不再把错误抛出保护区崩进程）。宿主侧 global 读写与字段探测全 raw（`raw_getglobal`/`raw_setglobal`/`raw_getfield`，栈语义同原生——`_ENV` 与模块表可被脚本运行期挂 `__index`/`__newindex` 恶意 error，call 路径与 global 访问面同样不崩进程）。**两段式热替换（§3.2 Phase A/B）**：`prepare_module(ModuleSpec, source)` 在独立预备沙盒（懒建第二 ScriptRuntime）走完整换表协议——编译 → 求值 → 冒烟 → 装预备镜像，返回 `PreparedModule{name, version, deps, bytecode, bytecode_hash, error}`（产物 = `lua_dump` strip=0 自家字节码 + fnv1a64 hash，与 §3.5 `lua_precompile` 口径一致；跨独立 lua_State 无法 xmove，走进程内 bytecode 交接，不经沙盒 `load`）；`commit_module(prepared)` 在 owning 线程 tick 边界做 hash 完整性校验（空产物/调包拒载 + fault + 审计入账）→ `"b"` 装载（Lua 自校产物头签名）→ 求值 → 表校验 → 与 `swap_module` 共用换表尾（原子换表 + 冷冻区 + 熔断清账 + onReload）；**冒烟不重跑**（预备面已验——重复自检把 Phase A 省下的成本搬回 Phase B，还引入二次执行差异面）；预备镜像按拓扑序累积（依赖模块须先经 prepare 备齐，跨预备面 swap 的模块不在镜像内报 `missing dependency on preparer`；owning 面 commit 同样先查依赖在位）；控制面串行调用契约（prepare/commit 不并发），宿主线程选择归控制面（`apollo_script` 零 apollo 依赖不引线程池）。**current 版本指针持久化**（§3.2 防坏版本崩溃循环）：`ScriptConfig.version_store_path`（patch 目录版本清单；空 = 关闭）——换表成功与回滚成功即整表读改写 + 原子 rename 回写 `<name>\t<version:016x>`（Windows CRT rename 不替换已存在目标时回退先删后 rename——tmp 已写完才删，目标位置不出现半截文件）；`current_version_manifest()` 供控制面重启读「最新已知健康版本」。写时机只有两处（swap/commit 成功、rollback 成功）——prepare 不写（版本尚未激活），预备沙盒另建配置清路径避免误写。清单与 §3.5 缓存 manifest 同族格式但访问面相反（运行期写、控制面读），分文件 `module_version_store`。**patch 目录读与 hash 校验控制面已落地**（Phase A 工程面第 1 步「读 patch 目录 → 校验 hash」）：`load_patch_directory(dir)`（`patch_loader`，manifest 驱动逐条目）——源 hash 必过（不符 = 篡改/损坏拒载进 errors）；字节码 hash / format_sig 不符 = 丢字节码留源码（产物回退仍可用）；manifest 缺失 = 整体错误。只负责「读 + 校验 + 带出产物」，编译/冒烟/换表归 `prepare_module`，deps 拓扑序归控制面编排。
- **属性钩子窗口**（§5.1/§5.2）：`set_attr_hook("onAttrChanged")` 注册全局钩子函数（签名 `(entity, attr, old, new)`，返回整数修订值，nil/非整数 = 不修饰）；`run_attr_hook_window(changes, whitelist, frame_budget)` 在重算后广播前逐条调用——白名单外跳过计数、帧预算耗尽剩余跳过（`reset_attr_frame()` 下一帧恢复）、钩子错误保持原值记 fault 且状态可继续；负载为 C++ 侧快照拷贝（`ScriptRuntime::AttrChange`，int64 对，无锁——同属 owning thread）；无钩子窗口零调用。
- **模块熔断**（§6.1）：`call_module(module, func)` 模块 handler 调用（fault 归因到模块）；滑窗 W tick 内 M 次 FAULT → 隔离 Q tick（W/M/Q/max 启动期配置，默认 1000/3/600/4800）；隔离期调用直接短路返回 false 不进 Lua（`stats().circuit_rejects` 计数）；隔离期满半开放行一次，探针成功销账解除、再 FAULT 隔离跨度翻倍（封顶 max）；**热更新自动清零**（swap/rollback 成功即新版本新账）；宿主每 tick 喂 `set_tick(t)`；状态面子集 `is_quarantined`/`quarantine_remaining`/`quarantined_modules`（名单+剩余 tick）。
- **熔断状态持久化**（§6.1，防坏版本崩溃循环）：`export_breaker_state()`/`import_breaker_state(blob)`——隔离中条目（绝对 tick 折算剩余期）+ WARN 去抖账跨重启延续（防重启告警风暴）；小端 TLV（魔数+版本），格式不符（魔数/版本/截断/尾随字节）拒载且状态不动；宿主自管落盘（journal/状态文件）与重启注入。
- **契约表装载**（§3.4）：`load_contract(source)`——contract.lua（apollo_gen 同批吐的服务端双形态）编译 → 结构校验（routes 表必备）→ 路由完整性闸（bin 有 handler 而 Lua 无即拒换，handler 为全局名或一层 `module.func` 点路径）→ 原子换 `_G["contract"]` 与 C++ 数据面（任一步失败旧表不动）；数据面 `contract_routes()`（msg_id → handler）、`contract_write_whitelist()`（§3.4 写白名单数据化，直接喂 `run_attr_hook_window`）、`contract_version()`；`call_route(msg_id)` 契约路由分发（未注册 id / handler 不可达 / 执行错误返回 false）。**生成器产物装载面**：contract.lua 由 apollo_gen 吐的 `routes` 平表 + 整数 `write_whitelist` id 数组 + `version` 三件与 loader 消费面同形（scripting-lua §3.4 装载面形态），`script_tests` 集成用例读仓内 golden 产物走全装载路径锚死——改 writer 或契约忘形态即红。
- **字节码缓存**（§3.5）：构建期 `lua_precompile` 工具产 `<name>.luac` + manifest 三元组（源 hash / 产物 hash / 格式签名——Lua 版本 + 类型尺寸集，跨小版本必不同）；运行期 env 门控只读装载（`APOLLO_LUA_BYTECODE_CACHE`，空/未设 = 关）。`run_chunk`/`swap_module`/`load_contract` 三装载点共用 `load_source`——三元组校验全过走 `luaL_loadbufferx` "b" 分支（`stats().cache_hits`/`cache_misses` 计数，缓存关闭不计数），任一不过（源变 / 签名变 / 产物缺失损坏 / 清单未命中）回退源码编译，装载永不因缓存失败；失效规则（Lua 版本变 / 源变 / 装载器逻辑变）全在构建期重跑管线。红线不变：沙盒 `load` 对运行期字符串仍拒 bytecode（mode "t"），缓存路径只服务自家构建产物、不经 `load`。
- **§6.1 事件面**：三级事件经 fault sink——WARN（单次消耗 ≥ 预算 × `warn_threshold_percent`（默认 80；0 = 关闭）且未超限，同 chunk 256 tick 去抖，不计 fault）、FAULT（既有分类计数）、TRIP/RECOVER（熔断触发/解除：半开销账、热更、回滚三路解除均发）；`ScriptFaultEvent` 结构化字段封闭集 `kind/chunk/message + ts_wall_ms/tick/consumed/budget`（引擎侧统一填充，下游按字段路由不解析文本）。
- **attach eval**（§7.1）：`eval_debug(source, result)`——admin 单入口投递、owning 线程 tick 边界由宿主择机调用；同一沙盒面（白名单/指令预算/内存上限全管辖，死循环可打断）；表达式优先、语句回退（REPL 惯例）；结果串截断（`debug_eval_result_max`，默认 4KB）+ 同 tick 次数限流（`debug_eval_max_per_tick`，默认 8，0 = 不限）；**失败面 result = 错误原文**（限流/语法/运行错——与成功结果同一出参，故障隔离不炸会话）。宿主侧执行队列已随 Scene 批交付（`Scene::enqueue_debug_eval` → tick 首拍固定调试点 → 按 id 回传，跨线程投递自持互斥）。
- **观测面**：`stats()`（调用/fault 分类计数/指令粗检累计/reload 成败/attr_hook 调用与跳过计数/WARN 去抖计数/熔断拒调计数/缓存命中与回退计数）、`memory_used()`、`reset_budget()`。
- **状态面只读内省**（§7.2）：`gc_state()`——宿主 allocf 计数 `used_bytes`（上限执法真值）+ `limit_bytes` + Lua 自视角 `lua_gc_kib` + 增量参数 `gc_pause/stepmul/stepsize`（`LUA_GCPARAM` 只读查询，value 传 -1；宿主恒增量模式，无公开 mode 读取 API）；`reload_history()` 热替换审计环（上限 64 条丢最旧）——swap 成功/拒载与 rollback 全入账（拒载也是事件：缺依赖/编译/求值/非表/冒烟拒，原因截断 256），记录 `LuaReloadRecord{name, from_version(0=首次装载), to_version, tick, success, reason}`，进程内即可查「各模块跑的哪个版本、怎么到的这个版本」；`env_snapshot(module, max_entries)` 模块 env 顶层深度 1 快照——`lua_next` 裸遍历（不走 `__pairs`/`__index`）、值类型化取值（不走 `lua_tolstring` 就地转换）、不调任何 Lua 函数，条目数上限截断（`truncated` 标志）、单值超 512 字节截断，模块名不在全局面 = 未找到。owning 线程 tick 边界调用（同 §7.1 eval 窗口纪律）；聚合呈现面归 observability 后续批。
- **性能归因分桶**（§7.3 Lua 侧）：同一 COUNT hook 双职能的第二职能——per-chunk 指令计数（hook 已付成本上分桶，10k 步进量化同 stats 口径）+ 墙钟累计（宿主调用边界 steady 掐表，微秒）；入账点 = `call_string`/`call_module`（按模块名）/`call_route`（按 handler 名）/`call_global`/attr 钩子窗口（按注册名）/`eval_debug` 六点，查找失败不执行不入账；查询面 `profile_snapshot()`（拷贝返回，chunk 名字典序）+ `profile_reset()`；超标事件状态面：指令预算中止时 `over_budget_history()` 环（上限 64 条丢最旧）记 `{chunk, tick, consumed, budget}`（事件面 `ScriptFaultEvent` 同拍仍发）。C++ 侧外采（frame marker/符号不剥离）与 MetricRegistry 聚合呈现归 observability/owning 接线批。
- **异步任务模型**（§8，L1 交接原语 + L2 协程面）：`register_async_service(name, handler, timeout_ms)` L0 执行层扩展点（宿主按批次插 Redis/DB/跨服真实后端，受理即返回）；`call_coroutine(module, func)` 业务协程形态运行 handler（跨 yield 业务以协程承载，熔断隔离期短路同 `call_module`）；Lua 侧 `apollo.async.call(svc, ...)` 实参类型化快照（bool/int/double/string/nil，table/function 不取不触元方法）后 yield 挂起（未知服务/主线程调用/未登记协程即 error 快速失败）；完成面 `complete_async`/`fail_async` 任意线程入队 + `drain_async()` tick 边界统一投递（交接物是消息不是 future——同线程也不 inline，完成只入队；迟到/重复/无主完成丢弃 + `stats().async_late` 计数）；超时上界 per 服务可配（0 = 取 `async_default_timeout_ms` 全局默认），超时先到以超时错误 resume（迟到结果丢弃 + 计数）；`cancel_pending(reason)` 协作式取消（下一 drain 以取消错误终止——停机/玩家下线收尾）；`set_async_validator` re-validate 钩子（完成投递前校验挂起协程上下文，失效以错误终止不注入僵尸结果）；**注入错误（超时/取消/失效）fault 照发但不计模块熔断**（运营终态归宿主，停机取消不打业务模块进隔离；真实执行错误照常归因熔断）；三条纪律全数落地：resume 只在 drain 边界固定点 / resume 后 re-validate / 超时上界 + 协作式取消。Lua 长跳纪律：yield/error 抛出点前释放宿主侧 RAII 局部（`lua_yieldk`/`lua_error` 跳栈不走 C++ 析构——实参 vector 作用域收在挂起点前、错误串复制进 Lua 后 swap 释放）。
- **协程清单**（§7.2，§8.2 协程登记表的呈现面）：`coroutine_snapshot()` 拷贝返回全量协程行 `LuaCoroutineInfo{handle, status, suspend_reason, origin, born_tick, age_ticks, pending_request, timeout_ms, leaked}`——status `suspended`（挂起中：`await <svc>` 或裸 `yield`）/`ready`（已登记未首跑；running/normal 为 resume 瞬态在快照点不可见）；origin 归因键 = `module:func`（`call_coroutine` 起）或 `coroutine.create`/`coroutine.wrap`（脚本自建——`create/wrap/resume` 同步替换进登记表，清单无盲区且指令 hook 与预算跨 yield 管辖）；终态懒清扫（快照点除名跑完/出错的协程，registry 锚定防 GC）；超龄检测 `async_leak_threshold_ticks`（0 = 关）命中即 `leaked=true`（§8.3 纪律 3 泄漏检测对象）；owning 线程 tick 边界调用（同 §7.1 纪律）。

### 边界（留后续批）

- 热替换工程面：灰度 per 场景线程、协程升级语义宿主侧接线
- 版本偏序执法（契约版本 ≥ 脚本补丁版本）
- 运行期消费接线：宿主启动装载（cell `--script-contract` / boot 调 `load_contract`）——契约装载后 handler 面须先行就位（现状零 Lua reflect handler 面，缺失即一致性闸拒载），随玩法 Lua 批
- 公式即钩子（§5.3）
- ScriptFaultEvent 宿主上下文字段（scene/entity_id/module+version——随场景接线批补）
- attach transport（admin 单入口/control 通道转发到目标进程——admin/control 面批次）、GM 命令面（§7.5）
- §8 L0 执行层真实后端（Redis/DB/跨服——`register_async_service` 扩展点已就位，随各自批次插入）；`apollo.async.all` 并发组合器（API 形态随消费批拍板）
- `require`/package 面未打开（模块加载暂走宿主 `call_string` 面）

## 测试

`tests/test_script_runtime.cpp`（58 用例）：白名单可用、沙盒缺省、二进制 chunk 拒绝、指令预算中断与状态恢复、预算 per-call 重充、内存上限、pcall 错误隔离、fault sink、具名模块注入；热替换原子换表、失败保旧、依赖拒载、冒烟闸、onReload 钩子、冷冻区回滚、reload 广播；属性钩子修订值、白名单外跳过、错误保持原值、帧预算跳过与下一帧恢复、nil 不修饰、无钩子空窗口；熔断三次触发、滑窗过期、半开恢复、半开翻倍、热更清零、M 可配置、call_module 分发、缺 handler 记 fault；契约装载数据面、缺 handler 拒换、坏结构拒载、失败保旧、契约路由分发、契约白名单喂钩子窗口、生成器产物装载面 2 例（读仓内 golden contract.lua 全装载路径：形态探针 + handler stub 注册 + 数据面与探针同值断言；无 handler 面直装拒载走一致性闸）；WARN 80% 触发、256 tick 去抖、阈值下静默、阈值上调静默、下调触发、0 关闭、TRIP/RECOVER 事件、热更清账发 RECOVER、事件结构化字段；eval 表达式求值与语句执行、错误隔离、预算管辖、结果截断、同 tick 限流与次 tick 恢复；熔断状态导出导入往返、跨重启隔离延续、tick 折算、未隔离不持久化、去抖账跨重启、坏 blob 拒载。三树门禁（GAME_MODULE ON/OFF）+ sanitizer 全绿。

`tests/test_bytecode_cache.cpp`（10 用例）+ `lua_precompile` 冒烟：manifest 行解析（合法/坏 hex/字段缺失/注释行）、缓存命中计数与执行语义、三失效规则回退（源 hash 变 / 格式签名变 / 产物损坏）、过 hash 闸后 "b" 装载失败兜底回退、清单缺失与条目未命中回退、env 未设零计数、契约装载走缓存。

`tests/test_script_async.cpp`（20 用例，§8 三纪律 + §7.2 协程清单）：往返语义（受理 yield → complete 入队 → drain 边界投递——drain 前/后 done 落位双断言，交接物是消息不是 future）、实参类型化快照五型、未知服务快速失败、主线程调用拒绝、超时错误 resume + 迟到完成丢弃计数、重复完成计 late、fail_async 错误串、协作式取消（双协程下一 drain 终止 + pcall 可见错误原文 + 取消在 drain 生效期快照仍 2 行）、注入错误不计熔断（fault 照发 + 不隔离）、re-validate 失效终止不注入僵尸结果（后置语句不执行）、清单字段（born/age/timeout/pending/origin/reason）、超龄 leaked、脚本自建协程 create/resume 登记与终态懒清扫、wrap 登记与错误传播（pcall 捕获面）、指令预算跨 yield 管辖（协程内死循环可打断）、熔断隔离期 call_coroutine 短路、真实执行错误归因熔断（完成投递后跑到 error 三次触发隔离）、跨线程 complete_async、顺序双 await 重挂起记账（reason/pending 迁移）。ASan/UBSan/LSan 全绿（Lua 长跳泄漏零容忍——yield/error 抛出点资源先释放）。

`tests/test_script_phase_a.cpp`（13 用例，§3.2 Phase A/B）：prepare→commit 往返（产物字节与 hash、预备不碰活跃区、审计入账 from=0）、版本矩阵 `active_modules()`；预备面四类拒载原因原文（编译/求值/冒烟/缺依赖）且预备失败不入 owning 审计环；commit 交接完整性（无预备直接 commit 拒载 + fault chunk 归因 + 审计拒载入账；调包单字节 hash 不符拒载）；预备镜像拓扑序（缺依赖拒载、直接 swap 的模块不算预备镜像、依赖先行双 commit 求值可见依赖值、反序 commit 依赖在位拒载后重试即过）；**commit 冒烟不重跑**（模块体跑一次、smoke 计数停在预备面值）；冷冻区回滚（commit 双版本 → rollback 版本翻转与审计三入账）。四树门禁（build / sanitizer / OFF / examples）全绿。

`tests/test_script_version_store.cpp`（15 用例，§3.2 current 版本指针持久化）：清单行解析与整表原子写往返、缺文件返回空清单、坏行跳过（注释/空行/字段缺失/坏 hex/尾随字段）、临时文件无残留与二次覆盖；runtime 集成面——swap/commit/rollback 三处回写落盘、同名覆盖保留其余模块条目、空路径关闭不写盘、prepare 不写（版本尚未激活）、拒载不写；**崩溃循环防护核心场景**——版本 1 健康 → 版本 2 判坏 → 回滚 → 进程崩溃（作用域退出）→ 重启新 runtime 同路径读清单 current 仍为 1（判坏版本 2 不再默认装载）；跨 runtime 持久性（A 写入析构 → B 读取）；预置清单直接可读。四树门禁（build / sanitizer / OFF / examples）+ ASan/UBSan/LSan 全绿。

`tests/test_script_patch_loader.cpp`（12 用例，§3.2 Phase A 工程面 patch 目录读与 hash 校验）：manifest 驱动装载（清单序带出源码与源 hash）、manifest 缺失整体错误、源 hash 不符拒载（篡改拦截）、源文件缺失拒载、无 luac 纯源码、字节码 hash 不符丢字节码留源码、format_sig 不符丢字节码留源码（跨 Lua 小版本）、有效字节码带出、清单序保持、空目录整体错误；控制面端到端——patch 目录 → prepare → commit 落 current 清单、篡改 patch 进不了 prepare。四树门禁（build / sanitizer / OFF / examples）+ ASan/UBSan/LSan 全绿。

`tests/test_script_state.cpp`（21 用例）：GC 面宿主计数同源 `memory_used` + Lua 自视角与增量参数只读查询；reload 审计成功入账（首次 from=0）与 tick 溯源、五类拒载原因（缺依赖/编译/求值/非表/冒烟）与超长原因截断、rollback from/to 翻转、审计环上限 64 丢最旧；env 快照原始类型渲染（串/整数/浮点/布尔/整数键/table/function）、条目上限截断、模块缺席/零上限/空名/非表全局、元方法恶意表不触（`__pairs`/`__index`/`__tostring` error 表照常快照）、单值 512 字节截断；契约表 hostile `__index` 装载正常（装载路径 raw 探测）；hostile `_ENV` 元表全宿主路径存活（call_module/get_global/set_global/换表提交/快照面）、hostile 模块元表 call 路径与契约完整性闸、宿主路径栈平衡回归（`raw_setglobal` 弹值语义）；profile 分桶六入账点（call_string 指令与墙钟、call_module 查找失败不入账、call_route 按 handler 名、attr 钩子按注册名 + eval）、超标事件环入账与 64 上限丢最旧、快照拷贝语义与 reset。
