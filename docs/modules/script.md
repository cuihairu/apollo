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
- **模块热替换**（§3.2）：`swap_module(ModuleSpec{name, version, deps}, source)`——依赖校验 → 编译（mode "t"）→ 冒烟（新表带 `smoke()` 则须返回 true）→ 全过才原子换 `_G[name]`，任一步失败旧表不动；旧活跃表进冷冻区（仅留最近一份），`rollback_module` 恢复并丢弃坏版本；换表成功调新表 `onReload(old)` 钩子（失败不回滚，记 fault），广播 `ModuleReloaded` 经 `set_reload_listener`；`active_modules()` 为 §7.2 版本矩阵。模块私有状态零迁移纪律照 §3.2（状态在 C++ 权威侧）。
- **观测面**：`stats()`（调用/fault 分类计数/指令粗检累计/reload 成败）、`memory_used()`、`reset_budget()`。

### 边界（留后续批）

- 热替换工程面：工作线程预加载 + smoke 用例集（§3.2 Phase A）、current 版本指针持久化（防坏版本崩溃循环）、灰度 per 场景线程、协程升级语义宿主侧接线
- 契约表装载（§3.4 contract.lua 一致性闸）、字节码缓存（§3.5 manifest 三元组）
- 属性钩子 `onAttrChanged`（§5）、公式即钩子（§5.3）、帧级钩子预算执法（§5.2）
- 故障熔断状态机（§6.1 模块级 3 次/1000 tick）、attach 调试（§7）、GM 命令面（§7.5）
- `require`/package 面未打开（模块加载暂走宿主 `call_string` 面）

## 测试

`tests/test_script_runtime.cpp`（16 用例）：白名单可用、沙盒缺省、二进制 chunk 拒绝、指令预算中断与状态恢复、预算 per-call 重充、内存上限、pcall 错误隔离、fault sink、具名模块注入；热替换原子换表、失败保旧、依赖拒载、冒烟闸、onReload 钩子、冷冻区回滚、reload 广播。三树门禁（GAME_MODULE ON/OFF）+ sanitizer 全绿。
