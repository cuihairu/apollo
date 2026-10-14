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
// 留后续批：契约表装载（§3.4）、字节码缓存（§3.5）、
// 故障熔断状态机（§6.1）、attach 调试（§7）。

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
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
};

struct ScriptFaultEvent {
    enum class Kind {
        kSyntax,           // 编译错误（含二进制 chunk 拒绝）
        kRuntime,          // 运行时 error
        kMemoryLimit,      // 内存超限
        kInstructionBudget, // 指令预算超限
    };
    Kind kind;
    std::string chunk;    // 来源名（chunkname）
    std::string message;  // 错误串
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
};

// §3.1 模块 = Lua 源 + 头注释声明的元信息（name/version/deps）。
struct ModuleSpec {
    std::string name;
    std::uint64_t version = 0;
    std::vector<std::string> deps;
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
    void fault(ScriptFaultEvent::Kind kind, const char* chunk, const std::string& msg);
    ScriptFaultEvent::Kind classify(int rc, const std::string& msg) const;

    ScriptConfig config_;
    lua_State* L_ = nullptr;
    std::function<void(const ScriptFaultEvent&)> sink_;
    ScriptStats stats_;
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

    // §5.1 钩子 registry ref（随 lua_close 释放）+ §5.2 帧级已用预算。
    int hook_ref_ = LUA_NOREF;
    std::uint64_t frame_hook_used_ = 0;
};

}  // namespace script
}  // namespace apollo
