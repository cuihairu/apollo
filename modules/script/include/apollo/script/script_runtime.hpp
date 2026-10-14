// ScriptRuntime 沙盒宿主（scripting-lua.md §2/§4/§6 第一批）。
//
// 交付面：
//   - 一状态一 lua_State（§2：一逻辑线程一状态，无互锁）
//   - 标准库白名单（base/string/table/math/utf8/coroutine；io/os/debug 不开）
//   - base 危险面清除（dofile/loadfile/collectgarbage 禁；load 只收文本）
//   - 指令预算（COUNT hook 粗检 10k 步进，超限中止当前调用）
//   - 内存上限（自定义 allocf，超限拒绝分配）
//   - pcall 隔离（错误分类入 fault 事件，不崩进程，状态可继续）
//   - 具名模块注入（§4.1 启动期注入唯一通道：register_module）
//
// 交付面：字节码缓存（§3.5 manifest 三元组校验，回退源码编译）、attach 调试（§7.1）。

#pragma once

#include "apollo/script/lua_bytecode_cache.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <lua.hpp>

namespace apollo {
namespace script {

struct ScriptConfig {
    // 每次调用的指令预算（0 = 不限）。设计稿 §6：每状态每帧预算——
    // 帧级执法归 §5.2 钩子窗口批，本批为 per-call 语义。
    std::uint64_t instruction_budget = 100000;
    // 每状态内存上限（0 = 不限）。超限 → 拒绝新分配（Lua 转 memory error）。
    std::size_t memory_limit_bytes = 8u << 20;
    // §6.1 模块熔断（W/M/Q 启动期配置非硬编码）：滑窗 W tick 内 M 次 FAULT
    // → 隔离 Q tick；半开探针再 FAULT 隔离翻倍，封顶 max。
    std::uint64_t breaker_window_ticks = 1000;
    std::uint32_t breaker_fault_limit = 3;
    std::uint64_t breaker_quarantine_ticks = 600;
    std::uint64_t breaker_max_quarantine_ticks = 4800;
    // §7.1 attach eval：同 tick 次数上限（0 = 不限）与结果串大小上限。
    std::uint32_t debug_eval_max_per_tick = 8;
    std::size_t debug_eval_result_max = 4096;
    // §6.1 WARN 软阈值（占预算百分比，默认 80；0 = 关闭 WARN）。
    std::uint32_t warn_threshold_percent = 80;
};

struct ScriptFaultEvent {
    enum class Kind {
        kSyntax,           // 编译错误（含二进制 chunk 拒绝）
        kRuntime,          // 运行时 error
        kMemoryLimit,      // 内存超限
        kInstructionBudget, // 指令预算超限
        kWarnBudget,       // §6.1 WARN：单次消耗 ≥ 预算 80%（256 tick 去抖）
        kCircuitTripped,   // §6.1 TRIP：模块隔离
        kCircuitRecovered, // §6.1 隔离解除（半开销账/热更/回滚）
    };
    Kind kind;
    std::string chunk;    // 来源名（chunkname / 模块名 / handler 名）
    std::string message;  // 错误串
    // §6.1 结构化字段（封闭集；引擎侧统一填充，下游按字段路由不解析文本）。
    std::uint64_t ts_wall_ms = 0;  // wall 双写（clock-and-time §7 纪律）
    std::uint64_t tick = 0;        // 宿主 tick（set_tick 喂入）
    std::uint64_t consumed = 0;    // 本次指令消耗（10k 步进量化）
    std::uint64_t budget = 0;      // 配置预算（0 = 不限）
};

struct ScriptStats {
    std::uint64_t calls = 0;
    std::uint64_t faults = 0;
    std::uint64_t fault_syntax = 0;
    std::uint64_t fault_runtime = 0;
    std::uint64_t fault_memory = 0;
    std::uint64_t fault_budget = 0;
    std::uint64_t instructions = 0;   // 粗检近似（10k 步进累计）
    std::uint64_t reload_success = 0;  // §3.2 换表成功
    std::uint64_t reload_failed = 0;   // §3.2 换表拒载（编译/冒烟/依赖）
    std::uint64_t attr_hook_calls = 0;             // §5.1 钩子调用
    std::uint64_t attr_hook_skipped_whitelist = 0; // §5.1 白名单外跳过
    std::uint64_t attr_hook_skipped_budget = 0;    // §5.2 预算耗尽跳过
    std::uint64_t attr_hook_faults = 0;            // §5.1 钩子错误（原值保持）
    std::uint64_t circuit_rejects = 0;             // §6.1 隔离期短路拒调
    std::uint64_t budget_warns = 0;                // §6.1 WARN 去抖后事件数
    std::uint64_t debug_evals = 0;                 // §7.1 attach eval 次数
    std::uint64_t cache_hits = 0;                  // §3.5 字节码缓存命中
    std::uint64_t cache_misses = 0;                // §3.5 缓存未命中（回退源码）
};

// §3.1 模块 = Lua 源 + 头注释声明的元信息（name/version/deps）。
struct ModuleSpec {
    std::string name;
    std::uint64_t version = 0;
    std::vector<std::string> deps;
};

// §7.2 Lua 状态面只读快照（检测原语在进程内；聚合呈现面归 observability
// 后续批）。owning 线程 tick 边界调用（同 §7.1 eval 窗口纪律）。
struct LuaGcState {
    std::size_t used_bytes = 0;   // 宿主 allocf 计数（上限执法真值）
    std::size_t limit_bytes = 0;  // 0 = 不限
    std::size_t lua_gc_kib = 0;   // GC 自视角（LUA_GCCOUNT）
    std::size_t gc_pause = 0;     // 增量模式参数（LUA_GCPARAM 只读查询）
    std::size_t gc_stepmul = 0;
    std::size_t gc_stepsize = 0;
    bool gc_is_running = false;
};

// 热替换审计记录（§7.2「线上各模块跑的哪个版本、怎么到的这个版本」；
// 拒载也是事件——失败同样入账）。
struct LuaReloadRecord {
    std::string name;
    std::uint64_t from_version = 0;  // 0 = 首次装载
    std::uint64_t to_version = 0;
    std::uint64_t tick = 0;          // 宿主喂入 tick（set_tick）
    bool success = false;
    std::string reason;              // 拒载原因（截断到 256 字符）；成功为空
};

// 模块 env 顶层值只读快照（深度 1，raw 遍历 + 类型化取值——不经元方法、
// 不就地转换数值；超长值截断）。
struct LuaEnvSnapshot {
    bool found = false;              // 模块名不在全局面 = false
    bool truncated = false;          // 条目数上限截断标志
    std::vector<std::pair<std::string, std::string>> entries;
};

class ScriptRuntime {
public:
    // §5.1 钩子负载：C++ 侧快照拷贝（int64 对），无锁——同属 owning thread。
    struct AttrChange {
        std::uint64_t entity;
        std::uint64_t attr_id;
        std::int64_t old_value;
        std::int64_t new_value;
    };

    explicit ScriptRuntime(ScriptConfig config = {});
    ~ScriptRuntime();

    ScriptRuntime(const ScriptRuntime&) = delete;
    ScriptRuntime& operator=(const ScriptRuntime&) = delete;

    // 故障事件冷路径单点（§6.1）：不设则只计数。
    void set_fault_sink(std::function<void(const ScriptFaultEvent&)> sink);

    // §4.1 启动期注入：具名模块表挂到全局（白名单制，非自动反射）。
    void register_module(const char* name, const luaL_Reg* funcs);

    // §3.2 热替换：模块源必须 return 一个表。编译 + 冒烟（smoke()）在影子栈
    // 完成，全部通过才原子换 _G[name]（Phase B）；任何一步失败旧表不动。
    bool swap_module(const ModuleSpec& spec, const std::string& source);
    // §3.2 Phase C：冷冻区换回活跃区（一次替换对应用一次回滚）。
    bool rollback_module(const std::string& name);

    bool has_module(const char* name) const;
    std::uint64_t module_version(const std::string& name) const;  // 无模块返回 0
    std::map<std::string, std::uint64_t> active_modules() const;  // §7.2 版本矩阵

    // ModuleReloaded 广播（脚本侧 onReload 钩子重注册订阅）。
    void set_reload_listener(std::function<void(const char*, std::uint64_t)> listener);

    // §7.2 状态面只读内省（owning 线程；不经 Lua 可调函数、不改写值）。
    [[nodiscard]] LuaGcState gc_state() const;
    // 热替换审计环（含拒载；上限 kReloadHistoryCap 条，超限丢最旧）。
    [[nodiscard]] const std::deque<LuaReloadRecord>& reload_history() const {
        return reload_history_;
    }
    // 模块 env 顶层值快照（max_entries = 条目上限，0 恒返回未找到）。
    [[nodiscard]] LuaEnvSnapshot env_snapshot(const char* module,
                                              std::size_t max_entries) const;

    // §5.1 属性钩子：注册全局函数 onAttrChanged(entity, attr, old, new) →
    // 整数（nil/非整数 = 不修饰）；全局名不存在返回 false，重挂换引用。
    bool set_attr_hook(const char* global_name);
    void clear_attr_hook();
    // §5.1/§5.2 钩子窗口：白名单外跳过计数；帧预算耗尽剩余跳过
    // （reset_attr_frame 下一帧恢复）；钩子错误保持原值记 fault，状态可继续。
    void run_attr_hook_window(std::vector<AttrChange>& changes,
                              const std::vector<std::uint64_t>& whitelist,
                              std::uint64_t frame_instruction_budget);
    void reset_attr_frame();

    // 编译 + 执行（pcall 隔离）。失败记 fault 并返回 false，状态可继续。
    bool call_string(const char* chunk, const std::string& source);
    // 调用已定义的全局函数（无参无返回）。
    bool call_global(const char* func);

    void set_global_string(const char* name, const std::string& value);
    std::string get_global_string(const char* name);  // 不存在/非串返回 ""
    bool get_global_bool(const char* name);           // 不存在/非布尔返回 false

    // 每帧预算复位（§6 帧语义；per-call 预算下为幂等）。
    void reset_budget();

    // §6.1 模块熔断：宿主每 tick 喂当前 tick（熔断窗与隔离期均以 tick 计）。
    void set_tick(std::uint64_t tick) { tick_ = tick; }
    // 模块 handler 调用（无参无返回，fault 归因到模块）：隔离期直接短路
    // 返回 false（不进 Lua）；半开期满放行一次，探针成功销账、再 FAULT
    // 隔离翻倍（封顶 max）；热更/回滚自动清零（新版本新账）。
    bool call_module(const std::string& module, const char* func);
    // §7.2 状态面子集：隔离名单与剩余 tick。
    [[nodiscard]] bool is_quarantined(const std::string& module) const;
    [[nodiscard]] std::uint64_t quarantine_remaining(const std::string& module) const;
    [[nodiscard]] std::map<std::string, std::uint64_t> quarantined_modules() const;

    // §6.1 熔断/去抖状态持久化（防坏版本崩溃循环：重启后坏模块仍隔离、
    // WARN 去抖窗口跨重启连续）。导出仅隔离中条目 + 去抖账；导入原子换面，
    // 格式不符（魔数/版本/截断/尾随字节）返回 false 且状态不动。
    [[nodiscard]] std::vector<std::uint8_t> export_breaker_state() const;
    bool import_breaker_state(const std::vector<std::uint8_t>& blob);

    // §3.4 契约表装载（contract.lua，apollo_gen 同批吐的服务端双形态）：
    // 编译 → 结构校验（routes 必备）→ 路由完整性闸（bin 有 handler 而 Lua 无
    // 即拒换）→ 原子换 _G["contract"] 与 C++ 数据面；任一步失败旧表不动。
    bool load_contract(const std::string& source);
    [[nodiscard]] std::uint64_t contract_version() const { return contract_version_; }
    [[nodiscard]] const std::map<std::uint32_t, std::string>& contract_routes() const {
        return contract_routes_;
    }
    // §3.4 写白名单数据化：直接喂 run_attr_hook_window（替代宿主自备白名单）。
    [[nodiscard]] const std::vector<std::uint64_t>& contract_write_whitelist() const {
        return contract_whitelist_;
    }
    // 契约路由分发：id → handler（全局函数或 module.func 点路径）。
    // 未注册 id / handler 不可达 / 执行错误均返回 false。
    bool call_route(std::uint32_t msg_id);

    // §7.1 attach eval：admin 单入口投递，owning 线程 tick 边界由宿主择机调用。
    // 同一沙盒面（白名单/预算/内存上限全管辖）；表达式优先、语句回退；
    // 结果串截断（debug_eval_result_max）、同 tick 次数限流（防打爆控制通道）。
    // 失败面 result = 错误原文（限流/语法/运行错——回执与审计消费）。
    bool eval_debug(const std::string& source, std::string& result);
    std::size_t memory_used() const;
    const ScriptStats& stats() const { return stats_; }

    // 后续批绑定面（属性钩子/契约表）用；本批测试不依赖。
    lua_State* raw() { return L_; }

    // alloc/hook 回调桥（勿直接调用）。
    void release_bytes(std::size_t n);
    void acquire_bytes(std::size_t n);
    bool exceeds_limit(std::size_t delta) const;
    std::uint64_t& fault_memory_counter();
    void tick_instructions(lua_Integer step);

private:
    bool run_chunk(const char* chunk, const std::string& source);
    // §3.5 缓存装载面：manifest 三元组（源 hash / bytecode hash / 格式签名）
    // 全过才走 "b" 分支；任一不过（含文件缺失/损坏）回退源码 "t" 编译。
    // 运行期只读——产物由构建管线（lua_precompile）独占写。
    int load_source(const char* chunk, const std::string& source, bool& used_cache);
    void fault(ScriptFaultEvent::Kind kind, const char* chunk, const std::string& msg);
    ScriptFaultEvent::Kind classify(int rc, const std::string& msg) const;
    void breaker_on_fault(const std::string& module, bool probing);
    void breaker_on_success(const std::string& module);
    // §6.1 热更/回滚清账（隔离中则先发 RECOVER 事件再抹状态）。
    void breaker_clear(const std::string& module, const char* reason);

    // 事件出口单点：填结构化字段（wall/tick/consumed/budget）后交 sink，
    // 不动 fault 计数（fault() 与 WARN/TRIP 共用）。
    void send(ScriptFaultEvent&& e);
    // §6.1 WARN：单次消耗 ≥ 预算 80% 且未超限；同 chunk 256 tick 去抖。
    void warn_check(const char* chunk, std::uint64_t consumed);
    // §3.4 handler 可达判定（全局名或一层 module.func 点路径）；找到则留
    // 函数于栈顶（调用方负责弹出），未找到返回 false 且栈复原。
    bool resolve_handler(const std::string& handler);

    // §6.1 持久化小端编码原语（仅本类内部使用）。
    static void put_u16(std::vector<std::uint8_t>& out, std::uint16_t v);
    static void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v);
    static void put_u64(std::vector<std::uint8_t>& out, std::uint64_t v);
    static void put_str(std::vector<std::uint8_t>& out, const std::string& s);
    // §7.2 热替换审计入账（环形上限内丢最旧）。
    void record_reload(const std::string& name, std::uint64_t from_version,
                       std::uint64_t to_version, bool success, std::string reason);

    ScriptConfig config_;
    lua_State* L_ = nullptr;
    std::function<void(const ScriptFaultEvent&)> sink_;
    ScriptStats stats_;
    // §3.5 字节码缓存面（env 门控；owning 线程独占——装载与统计同线程）
    std::string bytecode_cache_dir_;
    bool manifest_loaded_ = false;
    std::unique_ptr<std::vector<ManifestEntry>> manifest_;
    std::size_t allocated_ = 0;
    std::uint64_t used_instructions_ = 0;

    // §3.2 模块注册表：活跃表 + 冷冻区（旧活跃表，供 rollback；仅保留最近一份）。
    // registry ref 由本类独占（不交给宿主），析构随 lua_close 一并释放。
    std::map<std::string, int> module_refs_;
    struct FrozenModule {
        int ref;
        std::uint64_t version;
    };
    std::map<std::string, FrozenModule> module_frozen_;
    std::map<std::string, std::uint64_t> module_versions_;
    std::function<void(const char*, std::uint64_t)> reload_listener_;
    // §7.2 热替换审计环（swap 拒载/成功 + rollback；进程内即可查版本漂移）。
    std::deque<LuaReloadRecord> reload_history_;

    // §5.1 钩子 registry ref（随 lua_close 释放）+ §5.2 帧级已用预算。
    int hook_ref_ = LUA_NOREF;
    std::uint64_t frame_hook_used_ = 0;

    // §6.1 模块熔断状态：滑窗 FAULT 时间戳 + 隔离期（宿主喂 tick）。
    std::uint64_t tick_ = 0;
    struct ModuleBreaker {
        std::deque<std::uint64_t> fault_ticks;   // 窗内 FAULT tick（滑窗 W）
        std::uint64_t quarantined_until = 0;     // 0 = 未隔离
        std::uint64_t quarantine_span = 0;       // 当前隔离跨度（翻倍基数）
    };
    std::map<std::string, ModuleBreaker> breakers_;

    // §6.1 WARN 去抖账（chunk → 上次 WARN tick）。
    std::map<std::string, std::uint64_t> warn_ticks_;
    // §7.1 attach eval 同 tick 限流账。
    std::uint64_t debug_eval_tick_ = 0;
    std::uint32_t debug_eval_count_ = 0;
    // §3.4 契约数据面（装载成功后与 _G["contract"] 同拍生效）。
    std::map<std::uint32_t, std::string> contract_routes_;
    std::vector<std::uint64_t> contract_whitelist_;
    std::uint64_t contract_version_ = 0;
};

}  // namespace script
}  // namespace apollo
