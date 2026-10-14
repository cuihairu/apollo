// ScriptRuntime 沙盒宿主实现（scripting-lua.md §2/§4/§6 第一批）。

#include "apollo/script/script_runtime.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <utility>

namespace apollo {
namespace script {

namespace {

// 指令 hook 粗检步进（§6：粗检每 10k 指令）。
constexpr lua_Integer kHookStep = 10000;

// §6.1 WARN 去抖窗（同 chunk 每 256 tick 至多一条）。
constexpr std::uint64_t kWarnDebounceTicks = 256;

// §7.2 热替换审计环上限（超限丢最旧——防热更循环无界增长）。
constexpr std::size_t kReloadHistoryCap = 64;

// §7.2 env 快照单值字节上限（超长截断——顶层字符串可能是大块内容）。
constexpr std::size_t kEnvValueCap = 512;

// 装载路径结构探测 raw 读：模块/契约自带字段不经元表继承，裸 lua_getfield
// 会在 __index error 时把错误抛出保护区（崩进程，违 §2 纪律）——同栈效果
// 换 raw 读取，__index/__pairs/__tostring 恶意表全程不触元方法。
void raw_getfield(lua_State* L, int idx, const char* k) {
    const int t = lua_absindex(L, idx);
    lua_pushstring(L, k);
    lua_rawget(L, t);
}

// global raw 读/写：_ENV 可被脚本运行期挂 __index/__newindex（setmetatable
// 在任何 call_string 里都能跑），宿主侧 global 访问一律 raw——恶意元表不再
// 把错误抛出保护区（栈效果同 lua_getglobal/lua_setglobal，无元表时行为一致）。
void raw_getglobal(lua_State* L, const char* name) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);  // [globals]
    lua_pushstring(L, name);
    lua_rawget(L, -2);                                    // [globals, value]
    lua_remove(L, -2);                                    // [value]
}

void raw_setglobal(lua_State* L, const char* name) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);  // [value, globals]
    lua_pushstring(L, name);                              // [value, globals, key]
    lua_pushvalue(L, -3);                                 // [.., key, value]
    lua_rawset(L, -3);                                    // [value, globals]
    lua_pop(L, 2);                                        // [] —— 同 lua_setglobal 弹出值
}

// §7.2 只读取值：字符串拷贝、数值自格式化（不走 lua_tolstring——它会把
// 栈上数值就地转字符串，破坏只读纪律）、其余只出类型名。不调用任何
// Lua 函数、不触任何元方法。
std::string read_plain_value(lua_State* L, int idx) {
    auto clipped = [](std::string v) -> std::string {
        if (v.size() > kEnvValueCap) {
            v.resize(kEnvValueCap);
            v += "...(truncated)";
        }
        return v;
    };
    switch (lua_type(L, idx)) {
        case LUA_TNIL:
            return "nil";
        case LUA_TBOOLEAN:
            return lua_toboolean(L, idx) ? "true" : "false";
        case LUA_TNUMBER: {
            char buf[64];
            if (lua_isinteger(L, idx)) {
                std::snprintf(buf, sizeof(buf), "%lld",
                              static_cast<long long>(lua_tointeger(L, idx)));
            } else {
                std::snprintf(buf, sizeof(buf), "%.14g", lua_tonumber(L, idx));
            }
            return std::string(buf);
        }
        case LUA_TSTRING: {
            std::size_t len = 0;
            const char* s = lua_tolstring(L, idx, &len);  // 已是字符串，无转换
            return clipped(std::string(s ? s : "", len));
        }
        default:
            return std::string("<") + lua_typename(L, lua_type(L, idx)) + ">";
    }
}

// §7.2 审计 reason 截断（编译错误原文可能很长，账面保可读性）。
std::string clip_reason(std::string reason) {
    if (reason.size() > 256) {
        reason.resize(256);
        reason += "...";
    }
    return reason;
}

std::uint64_t wall_now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

// registry 存 this 的固定 key（静态地址唯一）。
const char kSelfKey = 0;

// 内存上限分配器（§6：超限 → 拒绝新分配请求）。
void* script_alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    auto* self = static_cast<ScriptRuntime*>(ud);
    if (nsize == 0) {
        if (ptr) {
            self->release_bytes(osize);
            std::free(ptr);
        }
        return nullptr;
    }
    const std::size_t held = ptr ? osize : 0;
    if (nsize < held) {
        // 缩容：直接 realloc，计数减少（无符号差安全）。
        void* p = std::realloc(ptr, nsize);
        if (p) {
            self->release_bytes(held - nsize);
        }
        return p;
    }
    if (self->exceeds_limit(nsize - held)) {
        ++self->fault_memory_counter();
        return nullptr;
    }
    void* p = std::realloc(ptr, nsize);
    if (!p) {
        return nullptr;
    }
    self->acquire_bytes(nsize - held);
    return p;
}

// 指令预算 hook（§6：超预算 → 中止当前脚本调用）。
void script_hook(lua_State* L, lua_Debug* /*ar*/) {
    lua_pushlightuserdata(L, const_cast<char*>(&kSelfKey));
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto* self = static_cast<ScriptRuntime*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!self) {
        return;
    }
    self->tick_instructions(kHookStep);
}

// base 库 load 替换（§2：运行期一切字节串按源码处理；mode "t" 拒二进制）。
int sandbox_load(lua_State* L) {
    size_t len = 0;
    const char* s = luaL_checklstring(L, 1, &len);
    const char* name = luaL_optstring(L, 2, "chunk");
    if (luaL_loadbufferx(L, s, len, name, "t") != LUA_OK) {
        return lua_error(L);
    }
    return 1;
}

void disable_global(lua_State* L, const char* name) {
    lua_pushnil(L);
    raw_setglobal(L, name);  // raw：_ENV 元表纪律全局统一
}

// §3.5 缓存产物读取（只读；运行期不写——产物归构建管线）。
bool read_bytes_file(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string& s = buffer.str();
    out.assign(s.begin(), s.end());
    return true;
}

// 标准库白名单（§2：base/string/table/math/bit32 → 5.5 线落点：
// bit32 已并入语言内建，代之 utf8；coroutine 供 §8 协程异步模型）。
struct LibEntry {
    const char* name;
    lua_CFunction open;
};

constexpr LibEntry kWhitelist[] = {
    {"_G", luaopen_base},
    {"string", luaopen_string},
    {"table", luaopen_table},
    {"math", luaopen_math},
    {"utf8", luaopen_utf8},
    {"coroutine", luaopen_coroutine},
};

}  // namespace

ScriptRuntime::ScriptRuntime(ScriptConfig config) : config_(config) {
    L_ = lua_newstate(script_alloc, this, 0);  // 5.5 签名：第三参 seed（默认语义 0）
    for (const auto& lib : kWhitelist) {
        luaL_requiref(L_, lib.name, lib.open, 1);
        lua_pop(L_, 1);
    }
    // base 危险面清除（§4.2 评审红线：脚本不得触文件系统/GC 控制）。
    disable_global(L_, "dofile");
    disable_global(L_, "loadfile");
    disable_global(L_, "collectgarbage");
    lua_pushcfunction(L_, sandbox_load);
    raw_setglobal(L_, "load");
    // this 入 registry（hook 回调取用）。
    lua_pushlightuserdata(L_, const_cast<char*>(&kSelfKey));
    lua_pushlightuserdata(L_, static_cast<void*>(this));
    lua_settable(L_, LUA_REGISTRYINDEX);
    lua_sethook(L_, script_hook, LUA_MASKCOUNT, static_cast<int>(kHookStep));

    // §3.5 字节码缓存（env 门控；空/未设 = 关，装载面恒走源码编译）
    if (const char* dir = std::getenv(kBytecodeCacheEnv)) {
        if (*dir != '\0') {
            bytecode_cache_dir_ = dir;
        }
    }
}

ScriptRuntime::~ScriptRuntime() {
    if (L_) {
        lua_close(L_);
    }
}

void ScriptRuntime::set_fault_sink(std::function<void(const ScriptFaultEvent&)> sink) {
    sink_ = std::move(sink);
}

void ScriptRuntime::register_module(const char* name, const luaL_Reg* funcs) {
    luaL_newlib(L_, funcs);
    raw_setglobal(L_, name);
}

// ---- §3.2 热替换：编译 + 冒烟在影子栈完成，通过才原子换 _G[name] ----

bool ScriptRuntime::swap_module(const ModuleSpec& spec, const std::string& source) {
    const std::uint64_t prev_version = module_version(spec.name);  // 0 = 首次
    // 依赖校验（依赖图拓扑序由装载器保证，本层只查已装载）。
    for (const auto& dep : spec.deps) {
        if (module_refs_.find(dep) == module_refs_.end()) {
            ++stats_.reload_failed;
            record_reload(spec.name, prev_version, spec.version, false,
                          "missing dependency: " + dep);
            return false;
        }
    }
    // 编译（§3.5 缓存面：manifest 校验过的自家产物走 "b"，其余回退源码 "t"；
    // 红线不变——沙盒 `load` 对运行期字符串仍拒 bytecode）。
    bool swap_from_cache = false;
    if (load_source(spec.name.c_str(), source, swap_from_cache) != LUA_OK) {
        const std::string msg = lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "";
        lua_pop(L_, 1);
        ++stats_.reload_failed;
        fault(ScriptFaultEvent::Kind::kSyntax, spec.name.c_str(), msg);
        record_reload(spec.name, prev_version, spec.version, false,
                      "compile error: " + msg);
        return false;
    }
    // 求值（失败 → 旧表不动）。
    const int rc = lua_pcall(L_, 0, 1, 0);
    stats_.instructions += used_instructions_;
    used_instructions_ = 0;
    if (rc != LUA_OK) {
        const std::string msg = lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "";
        lua_pop(L_, 1);
        ++stats_.reload_failed;
        fault(classify(rc, msg), spec.name.c_str(), msg);
        record_reload(spec.name, prev_version, spec.version, false,
                      "eval error: " + msg);
        return false;
    }
    if (!lua_istable(L_, -1)) {
        lua_pop(L_, 1);
        ++stats_.reload_failed;
        fault(ScriptFaultEvent::Kind::kRuntime, spec.name.c_str(),
              "module did not return a table");
        record_reload(spec.name, prev_version, spec.version, false,
                      "module did not return a table");
        return false;
    }
    // 冒烟闸：新表带 smoke() 则须返回 true（§3.2 Phase A 自检用例集）。
    raw_getfield(L_, -1, "smoke");
    if (lua_isfunction(L_, -1)) {
        const int src = lua_pcall(L_, 0, 1, 0);
        const bool passed = src == LUA_OK && lua_toboolean(L_, -1) != 0;
        const std::string smsg =
            src == LUA_OK ? "module smoke check failed"
                          : (lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "smoke error");
        lua_pop(L_, 1);  // 冒烟结果
        if (!passed) {
            lua_pop(L_, 1);  // 新表（未过闸，不进活跃引用）
            ++stats_.reload_failed;
            fault(ScriptFaultEvent::Kind::kRuntime, spec.name.c_str(), smsg);
            record_reload(spec.name, prev_version, spec.version, false, smsg);
            return false;
        }
    } else {
        lua_pop(L_, 1);
    }
    // 走到此处栈顶 = 新表。registry ref（luaL_ref 自弹栈顶）+ 原子换表。
    lua_pushvalue(L_, -1);
    const int new_ref = luaL_ref(L_, LUA_REGISTRYINDEX);  // [新表]
    raw_setglobal(L_, spec.name.c_str());                 // []
    // 旧活跃表 → 冷冻区（§3.2 冷冻区语义：rollback 依赖；只留最近一份）。
    const auto it = module_refs_.find(spec.name);
    if (it != module_refs_.end()) {
        const auto fit = module_frozen_.find(spec.name);
        if (fit != module_frozen_.end()) {
            luaL_unref(L_, LUA_REGISTRYINDEX, fit->second.ref);
        }
        module_frozen_[spec.name] = FrozenModule{it->second, module_versions_[spec.name]};
    }
    module_refs_[spec.name] = new_ref;
    module_versions_[spec.name] = spec.version;
    breaker_clear(spec.name, "hot swap cleared quarantine");  // §6.1 热更新账
    ++stats_.reload_success;
    record_reload(spec.name, prev_version, spec.version, true, "");
    if (reload_listener_) {
        reload_listener_(spec.name.c_str(), spec.version);
    }
    // onReload 钩子（§3.2 Phase B 事件钩子：脚本侧重挂订阅）。失败不回滚
    // （换表已原子完成，观察期由 Phase C 处理），记 fault。
    lua_rawgeti(L_, LUA_REGISTRYINDEX, new_ref);
    raw_getfield(L_, -1, "onReload");
    if (lua_isfunction(L_, -1)) {
        const auto fit = module_frozen_.find(spec.name);
        if (fit != module_frozen_.end()) {
            lua_rawgeti(L_, LUA_REGISTRYINDEX, fit->second.ref);
        } else {
            lua_pushnil(L_);
        }
        const int hrc = lua_pcall(L_, 1, 0, 0);
        if (hrc != LUA_OK) {
            const std::string msg = lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "";
            lua_pop(L_, 1);
            fault(ScriptFaultEvent::Kind::kRuntime, spec.name.c_str(), msg);
        }
    } else {
        lua_pop(L_, 1);
    }
    lua_pop(L_, 1);  // 新表
    return true;
}

bool ScriptRuntime::rollback_module(const std::string& name) {
    const auto fit = module_frozen_.find(name);
    if (fit == module_frozen_.end()) {
        return false;
    }
    lua_rawgeti(L_, LUA_REGISTRYINDEX, fit->second.ref);
    if (lua_isnil(L_, -1)) {
        lua_pop(L_, 1);
        return false;
    }
    // 回滚 = 撤销最近一次替换：坏版本表丢弃（unref 交 GC），冷冻区清空。
    const int restored_ref = fit->second.ref;
    const std::uint64_t restored_version = fit->second.version;
    const auto it = module_refs_.find(name);
    if (it != module_refs_.end()) {
        luaL_unref(L_, LUA_REGISTRYINDEX, it->second);
    }
    module_frozen_.erase(name);
    module_refs_[name] = restored_ref;
    raw_setglobal(L_, name.c_str());
    const std::uint64_t from_version = module_version(name);
    module_versions_[name] = restored_version;
    breaker_clear(name, "rollback cleared quarantine");  // §6.1 回滚同热更：新账
    record_reload(name, from_version, restored_version, true, "rollback");
    if (reload_listener_) {
        reload_listener_(name.c_str(), restored_version);  // §3.2 reload 伪事件
    }
    return true;
}

bool ScriptRuntime::has_module(const char* name) const {
    return module_refs_.count(name) != 0;
}

std::uint64_t ScriptRuntime::module_version(const std::string& name) const {
    const auto it = module_versions_.find(name);
    return it != module_versions_.end() ? it->second : 0;
}

std::map<std::string, std::uint64_t> ScriptRuntime::active_modules() const {
    return module_versions_;
}

void ScriptRuntime::set_reload_listener(
    std::function<void(const char*, std::uint64_t)> listener) {
    reload_listener_ = std::move(listener);
}

// ---- §7.2 Lua 状态面只读内省（owning 线程；不经 Lua 可调函数） ----

void ScriptRuntime::record_reload(const std::string& name, std::uint64_t from_version,
                                  std::uint64_t to_version, bool success,
                                  std::string reason) {
    if (reload_history_.size() == kReloadHistoryCap) {
        reload_history_.pop_front();  // 环形上限：丢最旧
    }
    reload_history_.push_back(LuaReloadRecord{name, from_version, to_version, tick_,
                                              success, clip_reason(std::move(reason))});
}

LuaGcState ScriptRuntime::gc_state() const {
    LuaGcState s;
    s.used_bytes = allocated_;                       // allocf 执法真值
    s.limit_bytes = config_.memory_limit_bytes;
    s.lua_gc_kib = static_cast<std::size_t>(lua_gc(L_, LUA_GCCOUNT));
    // LUA_GCPARAM：value 传 -1 = 只读返回当前参数值（百分位标度）。
    s.gc_pause =
        static_cast<std::size_t>(lua_gc(L_, LUA_GCPARAM, LUA_GCPPAUSE, -1));
    s.gc_stepmul =
        static_cast<std::size_t>(lua_gc(L_, LUA_GCPARAM, LUA_GCPSTEPMUL, -1));
    s.gc_stepsize =
        static_cast<std::size_t>(lua_gc(L_, LUA_GCPARAM, LUA_GCPSTEPSIZE, -1));
    s.gc_is_running = lua_gc(L_, LUA_GCISRUNNING) != 0;
    return s;
}

LuaEnvSnapshot ScriptRuntime::env_snapshot(const char* module,
                                           std::size_t max_entries) const {
    LuaEnvSnapshot snap;
    if (module == nullptr || *module == '\0' || max_entries == 0) {
        return snap;  // found = false
    }
    // 只读纪律：lua_next 裸遍历（不走 __pairs/__index），值经
    // read_plain_value 类型化取值（不走 lua_tolstring 就地转换、不调函数）。
    raw_getglobal(L_, module);
    if (!lua_istable(L_, -1)) {
        lua_pop(L_, 1);
        return snap;  // 模块名不在全局面 = 未找到
    }
    snap.found = true;
    lua_pushnil(L_);
    while (lua_next(L_, -2) != 0) {  // [module, key, value]
        if (snap.entries.size() >= max_entries) {
            snap.truncated = true;
            lua_pop(L_, 2);  // key, value
            break;
        }
        std::string k = read_plain_value(L_, -2);
        std::string v = read_plain_value(L_, -1);
        snap.entries.emplace_back(std::move(k), std::move(v));
        lua_pop(L_, 1);  // value（key 留作 lua_next 续行）
    }
    lua_pop(L_, 1);  // module 表
    return snap;
}

// ---- §5.1/§5.2 属性钩子窗口：重算后广播前，同步修订值 ----

bool ScriptRuntime::set_attr_hook(const char* global_name) {
    raw_getglobal(L_, global_name);
    if (!lua_isfunction(L_, -1)) {
        lua_pop(L_, 1);
        return false;
    }
    if (hook_ref_ != LUA_NOREF) {
        luaL_unref(L_, LUA_REGISTRYINDEX, hook_ref_);  // 重挂换引用
    }
    hook_ref_ = luaL_ref(L_, LUA_REGISTRYINDEX);  // 自弹栈顶
    return true;
}

void ScriptRuntime::clear_attr_hook() {
    if (hook_ref_ != LUA_NOREF) {
        luaL_unref(L_, LUA_REGISTRYINDEX, hook_ref_);
        hook_ref_ = LUA_NOREF;
    }
}

void ScriptRuntime::run_attr_hook_window(std::vector<AttrChange>& changes,
                                         const std::vector<std::uint64_t>& whitelist,
                                         std::uint64_t frame_instruction_budget) {
    if (hook_ref_ == LUA_NOREF) {
        return;  // §5.2 空载零调用（无钩子窗口零开销）
    }
    for (auto& ch : changes) {
        if (std::find(whitelist.begin(), whitelist.end(), ch.attr_id) ==
            whitelist.end()) {
            ++stats_.attr_hook_skipped_whitelist;
            continue;
        }
        if (frame_hook_used_ >= frame_instruction_budget) {
            ++stats_.attr_hook_skipped_budget;  // §5.2 帧级预算：剩余跳过
            continue;
        }
        lua_rawgeti(L_, LUA_REGISTRYINDEX, hook_ref_);
        lua_pushinteger(L_, static_cast<lua_Integer>(ch.entity));
        lua_pushinteger(L_, static_cast<lua_Integer>(ch.attr_id));
        lua_pushinteger(L_, static_cast<lua_Integer>(ch.old_value));
        lua_pushinteger(L_, static_cast<lua_Integer>(ch.new_value));
        used_instructions_ = 0;
        const int rc = lua_pcall(L_, 4, 1, 0);
        frame_hook_used_ += used_instructions_;
        used_instructions_ = 0;
        if (rc != LUA_OK) {
            const std::string msg = lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "";
            lua_pop(L_, 1);
            ++stats_.attr_hook_faults;
            fault(classify(rc, msg), "onAttrChanged", msg);  // 原值保持
            continue;
        }
        ++stats_.attr_hook_calls;
        if (lua_isinteger(L_, -1)) {
            ch.new_value = static_cast<std::int64_t>(lua_tointeger(L_, -1));
        }
        lua_pop(L_, 1);  // nil = 不修饰，丢弃
    }
}

void ScriptRuntime::reset_attr_frame() { frame_hook_used_ = 0; }

// ---- §6.1 模块熔断：滑窗 W 计 FAULT → 隔离 Q，半开翻倍，热更新账 ----

bool ScriptRuntime::call_module(const std::string& module, const char* func) {
    auto bit = breakers_.find(module);
    if (bit != breakers_.end() && bit->second.quarantined_until != 0 &&
        tick_ < bit->second.quarantined_until) {
        ++stats_.circuit_rejects;  // 隔离期短路，不进 Lua（实体回退默认行为）
        return false;
    }
    // 走到这里 = 正常调用，或隔离期满的半开探针（成败定销账/翻倍）。
    const bool probing = bit != breakers_.end() && bit->second.quarantined_until != 0;
    ++stats_.calls;
    used_instructions_ = 0;
    bool ok = false;
    raw_getglobal(L_, module.c_str());
    if (!lua_istable(L_, -1)) {
        lua_pop(L_, 1);
        fault(ScriptFaultEvent::Kind::kRuntime, module.c_str(), "module not a table");
    } else {
        raw_getfield(L_, -1, func);
        if (!lua_isfunction(L_, -1)) {
            lua_pop(L_, 2);
            fault(ScriptFaultEvent::Kind::kRuntime, module.c_str(),
                  "handler not a function");
        } else {
            lua_remove(L_, -2);  // 栈顶 handler（模块函数无 self）
            const int rc = lua_pcall(L_, 0, 0, 0);
            stats_.instructions += used_instructions_;
            if (rc == LUA_OK) {
                ok = true;
            } else {
                const std::string msg =
                    lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "";
                lua_pop(L_, 1);
                fault(classify(rc, msg), module.c_str(), msg);
            }
        }
    }
    if (ok) {
        warn_check(module.c_str(), used_instructions_);
        breaker_on_success(module);
    } else {
        breaker_on_fault(module, probing);
    }
    return ok;
}

void ScriptRuntime::breaker_on_fault(const std::string& module, bool probing) {
    auto& br = breakers_[module];
    if (probing) {
        // §6.1 半开探针再 FAULT → 隔离跨度翻倍（封顶 max）。
        br.quarantine_span = std::min(br.quarantine_span * 2,
                                      config_.breaker_max_quarantine_ticks);
        br.quarantined_until = tick_ + br.quarantine_span;
        br.fault_ticks.clear();
        send(ScriptFaultEvent{ScriptFaultEvent::Kind::kCircuitTripped, module,
                              "module re-quarantined (half-open fault) for " +
                                  std::to_string(br.quarantine_span) + " ticks"});
        return;
    }
    // 滑窗 W：窗外 FAULT 时间戳到期弹出。
    while (!br.fault_ticks.empty() &&
           br.fault_ticks.front() + config_.breaker_window_ticks <= tick_) {
        br.fault_ticks.pop_front();
    }
    br.fault_ticks.push_back(tick_);
    if (br.fault_ticks.size() >= config_.breaker_fault_limit) {
        br.quarantine_span = config_.breaker_quarantine_ticks;
        br.quarantined_until = tick_ + br.quarantine_span;
        br.fault_ticks.clear();
        send(ScriptFaultEvent{ScriptFaultEvent::Kind::kCircuitTripped, module,
                              "module quarantined for " +
                                  std::to_string(br.quarantine_span) + " ticks"});
    }
}

void ScriptRuntime::breaker_on_success(const std::string& module) {
    auto bit = breakers_.find(module);
    if (bit != breakers_.end() && bit->second.quarantined_until != 0) {
        breakers_.erase(bit);  // §6.1 半开探针成功 → 销账解除
        send(ScriptFaultEvent{ScriptFaultEvent::Kind::kCircuitRecovered, module,
                              "half-open probe succeeded"});
    }
}

void ScriptRuntime::breaker_clear(const std::string& module, const char* reason) {
    auto bit = breakers_.find(module);
    if (bit == breakers_.end()) {
        return;
    }
    if (bit->second.quarantined_until != 0) {
        send(ScriptFaultEvent{ScriptFaultEvent::Kind::kCircuitRecovered, module,
                              std::string(reason)});
    }
    breakers_.erase(bit);
}

bool ScriptRuntime::is_quarantined(const std::string& module) const {
    auto bit = breakers_.find(module);
    return bit != breakers_.end() && bit->second.quarantined_until != 0 &&
           tick_ < bit->second.quarantined_until;
}

std::uint64_t ScriptRuntime::quarantine_remaining(const std::string& module) const {
    auto bit = breakers_.find(module);
    if (bit == breakers_.end() || bit->second.quarantined_until == 0) {
        return 0;
    }
    return tick_ < bit->second.quarantined_until
               ? bit->second.quarantined_until - tick_
               : 0;
}

std::map<std::string, std::uint64_t> ScriptRuntime::quarantined_modules() const {
    std::map<std::string, std::uint64_t> out;
    for (const auto& [name, br] : breakers_) {
        if (br.quarantined_until != 0 && tick_ < br.quarantined_until) {
            out.emplace(name, br.quarantined_until - tick_);
        }
    }
    return out;
}

// §6.1 熔断/去抖状态持久化：小端 TLV（魔数 APLB + 版本 + 隔离条目 + 去抖账）。
// 只持久化隔离中条目（未隔离的滑窗计数器不跨重启延续）。
namespace {
constexpr std::uint8_t kStateMagic[4] = {'A', 'P', 'L', 'B'};
constexpr std::uint8_t kStateVersion = 1;

struct BlobReader {
    const std::uint8_t* data;
    std::size_t size;
    std::size_t off = 0;

    bool bytes(std::size_t k, const std::uint8_t** out) {
        if (size < off + k) {
            return false;  // 截断
        }
        *out = data + off;
        off += k;
        return true;
    }
    bool u8(std::uint8_t& v) {
        const std::uint8_t* p = nullptr;
        if (!bytes(1, &p)) {
            return false;
        }
        v = p[0];
        return true;
    }
    bool u16(std::uint16_t& v) {
        const std::uint8_t* p = nullptr;
        if (!bytes(2, &p)) {
            return false;
        }
        v = static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
        return true;
    }
    bool u32(std::uint32_t& v) {
        const std::uint8_t* p = nullptr;
        if (!bytes(4, &p)) {
            return false;
        }
        v = static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
            (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
        return true;
    }
    bool u64(std::uint64_t& v) {
        const std::uint8_t* p = nullptr;
        if (!bytes(8, &p)) {
            return false;
        }
        std::uint64_t acc = 0;
        for (int i = 0; i < 8; ++i) {
            acc |= static_cast<std::uint64_t>(p[i]) << (8u * static_cast<std::size_t>(i));
        }
        v = acc;
        return true;
    }
    bool str(std::string& s) {
        std::uint16_t len = 0;
        if (!u16(len)) {
            return false;
        }
        const std::uint8_t* p = nullptr;
        if (!bytes(len, &p)) {
            return false;
        }
        s.assign(reinterpret_cast<const char*>(p), len);
        return true;
    }
};
}  // namespace

void ScriptRuntime::put_u16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}

void ScriptRuntime::put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

void ScriptRuntime::put_u64(std::vector<std::uint8_t>& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>((v >> (8u * static_cast<std::size_t>(i))) & 0xFF));
    }
}

void ScriptRuntime::put_str(std::vector<std::uint8_t>& out, const std::string& s) {
    if (s.size() > 0xFFFF) {
        return;  // 编码位宽受限；模块名/路径名远小于此上限
    }
    put_u16(out, static_cast<std::uint16_t>(s.size()));
    const auto* p = reinterpret_cast<const std::uint8_t*>(s.data());
    out.insert(out.end(), p, p + s.size());
}

std::vector<std::uint8_t> ScriptRuntime::export_breaker_state() const {
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i < 4; ++i) {
        out.push_back(kStateMagic[i]);
    }
    out.push_back(kStateVersion);

    std::uint32_t quarantined = 0;
    for (const auto& [name, br] : breakers_) {
        if (br.quarantined_until != 0) {
            ++quarantined;
        }
    }
    put_u32(out, quarantined);
    for (const auto& [name, br] : breakers_) {
        if (br.quarantined_until == 0) {
            continue;
        }
        put_str(out, name);
        put_u64(out, br.quarantined_until);
        put_u64(out, br.quarantine_span);
    }

    put_u32(out, static_cast<std::uint32_t>(warn_ticks_.size()));
    for (const auto& [chunk, stamp] : warn_ticks_) {
        put_str(out, chunk);
        put_u64(out, stamp);
    }
    return out;
}

bool ScriptRuntime::import_breaker_state(const std::vector<std::uint8_t>& blob) {
    BlobReader r{blob.data(), blob.size()};
    const std::uint8_t* magic = nullptr;
    std::uint8_t version = 0;
    if (!r.bytes(4, &magic) || magic[0] != kStateMagic[0] || magic[1] != kStateMagic[1] ||
        magic[2] != kStateMagic[2] || magic[3] != kStateMagic[3] || !r.u8(version) ||
        version != kStateVersion) {
        return false;
    }

    std::map<std::string, ModuleBreaker> breakers;
    std::uint32_t count = 0;
    if (!r.u32(count)) {
        return false;
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        std::string name;
        std::uint64_t until = 0;
        std::uint64_t span = 0;
        if (!r.str(name) || !r.u64(until) || !r.u64(span)) {
            return false;
        }
        breakers.emplace(std::move(name),
                         ModuleBreaker{{}, until, span});
    }

    std::map<std::string, std::uint64_t> warns;
    if (!r.u32(count)) {
        return false;
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        std::string chunk;
        std::uint64_t stamp = 0;
        if (!r.str(chunk) || !r.u64(stamp)) {
            return false;
        }
        warns.emplace(std::move(chunk), stamp);
    }

    if (r.off != r.size) {
        return false;  // 尾随字节 = 格式不符（拒，防截断/拼接）
    }

    breakers_ = std::move(breakers);  // 原子换面：解析全过才生效
    warn_ticks_ = std::move(warns);
    return true;
}

// ---- §3.4 契约表装载：编译 → 结构校验 → 路由完整性闸 → 原子提交 ----

bool ScriptRuntime::resolve_handler(const std::string& handler) {
    const std::size_t dot = handler.find('.');
    if (dot == std::string::npos) {
        raw_getglobal(L_, handler.c_str());
        if (lua_isfunction(L_, -1)) {
            return true;  // [func]
        }
        lua_pop(L_, 1);
        return false;
    }
    const std::string mod = handler.substr(0, dot);
    const std::string fn = handler.substr(dot + 1);
    if (fn.empty() || fn.find('.') != std::string::npos) {
        return false;  // 仅支持一层 module.func 点路径
    }
    raw_getglobal(L_, mod.c_str());
    if (!lua_istable(L_, -1)) {
        lua_pop(L_, 1);
        return false;
    }
    raw_getfield(L_, -1, fn.c_str());
    if (lua_isfunction(L_, -1)) {
        lua_remove(L_, -2);  // [func]
        return true;
    }
    lua_pop(L_, 2);
    return false;
}

bool ScriptRuntime::load_contract(const std::string& source) {
    // 编译（§3.5 缓存面同 swap_module）+ 求值；任一步失败旧表/旧数据面不动
    // （§3.4 同 §3.2 协议）。
    bool contract_from_cache = false;
    if (load_source("contract", source, contract_from_cache) != LUA_OK) {
        const std::string msg = lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "";
        lua_pop(L_, 1);
        fault(ScriptFaultEvent::Kind::kSyntax, "contract", msg);
        return false;
    }
    if (lua_pcall(L_, 0, 1, 0) != LUA_OK) {
        const std::string msg = lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "";
        lua_pop(L_, 1);
        fault(ScriptFaultEvent::Kind::kRuntime, "contract", msg);
        return false;
    }
    if (!lua_istable(L_, -1)) {
        lua_pop(L_, 1);
        fault(ScriptFaultEvent::Kind::kRuntime, "contract",
              "contract did not return a table");
        return false;
    }
    // routes 必备：msg_id → handler 名。
    raw_getfield(L_, -1, "routes");  // [table, routes]
    if (!lua_istable(L_, -1)) {
        lua_pop(L_, 2);
        fault(ScriptFaultEvent::Kind::kRuntime, "contract", "missing routes table");
        return false;
    }
    std::map<std::uint32_t, std::string> routes;
    lua_pushnil(L_);  // [table, routes, nil]
    while (lua_next(L_, -2) != 0) {  // [table, routes, key, value]
        const bool keyed = lua_isinteger(L_, -2) && lua_isstring(L_, -1);
        std::string handler = keyed ? lua_tostring(L_, -1) : "";
        bool ok = keyed && resolve_handler(handler);  // 完整性闸：Lua 无 handler 即拒换
        if (ok) {
            lua_pop(L_, 1);  // func
            routes.emplace(static_cast<std::uint32_t>(lua_tointeger(L_, -2)),
                           std::move(handler));
        } else {
            fault(ScriptFaultEvent::Kind::kRuntime, "contract",
                  "route handler missing: " +
                      (handler.empty() ? std::string("(malformed entry)") : handler));
        }
        lua_pop(L_, 1);  // value
        if (!ok) {
            lua_pop(L_, 3);  // key, routes, table
            return false;
        }
    }
    lua_pop(L_, 1);  // routes → [table]
    // write_whitelist 可选：attr id 数组（§3.4 写白名单数据化）。
    std::vector<std::uint64_t> whitelist;
    raw_getfield(L_, -1, "write_whitelist");  // [table, wl]
    if (!lua_isnil(L_, -1)) {
        if (!lua_istable(L_, -1)) {
            lua_pop(L_, 2);
            fault(ScriptFaultEvent::Kind::kRuntime, "contract",
                  "write_whitelist not a table");
            return false;
        }
        const lua_Integer n = static_cast<lua_Integer>(lua_rawlen(L_, -1));
        for (lua_Integer i = 1; i <= n; ++i) {
            lua_rawgeti(L_, -1, i);  // raw 数组读（lua_geti 亦走 __index）
            if (lua_isinteger(L_, -1)) {
                whitelist.push_back(static_cast<std::uint64_t>(lua_tointeger(L_, -1)));
            }
            lua_pop(L_, 1);
        }
    }
    lua_pop(L_, 1);  // wl → [table]
    // version 可选。
    std::uint64_t version = 0;
    raw_getfield(L_, -1, "version");
    if (lua_isinteger(L_, -1)) {
        version = static_cast<std::uint64_t>(lua_tointeger(L_, -1));
    }
    lua_pop(L_, 1);
    // 全部通过 → 原子提交：表与 C++ 数据面同拍生效。
    raw_setglobal(L_, "contract");  // []
    contract_routes_ = std::move(routes);
    contract_whitelist_ = std::move(whitelist);
    contract_version_ = version;
    return true;
}

bool ScriptRuntime::call_route(std::uint32_t msg_id) {
    const auto it = contract_routes_.find(msg_id);
    if (it == contract_routes_.end()) {
        return false;
    }
    if (!resolve_handler(it->second)) {
        // 契约装载后 handler 随热替换丢失（新版本未带）→ 拒调记 fault。
        fault(ScriptFaultEvent::Kind::kRuntime, "contract",
              "route handler missing: " + it->second);
        return false;
    }
    ++stats_.calls;
    used_instructions_ = 0;
    const int rc = lua_pcall(L_, 0, 0, 0);
    stats_.instructions += used_instructions_;
    if (rc != LUA_OK) {
        const std::string msg = lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "";
        lua_pop(L_, 1);
        fault(classify(rc, msg), it->second.c_str(), msg);
        return false;
    }
    warn_check(it->second.c_str(), used_instructions_);
    return true;
}

// ---- §7.1 attach eval：tick 边界沙盒执行，admin 通道面归宿主 ----

bool ScriptRuntime::eval_debug(const std::string& source, std::string& result) {
    // 同 tick 限流（防误操作打爆控制通道；0 = 不限）。
    if (tick_ != debug_eval_tick_) {
        debug_eval_tick_ = tick_;
        debug_eval_count_ = 0;
    }
    if (config_.debug_eval_max_per_tick != 0 &&
        debug_eval_count_ >= config_.debug_eval_max_per_tick) {
        result = "rate limited";  // 失败面 result = 错误原文（回执/审计消费）
        fault(ScriptFaultEvent::Kind::kRuntime, "debug_eval", "rate limited");
        return false;
    }
    ++debug_eval_count_;
    ++stats_.debug_evals;
    // 表达式优先（REPL 惯例），编译失败回退语句执行。
    ++stats_.calls;
    used_instructions_ = 0;
    int rc = LUA_ERRSYNTAX;
    const std::string expr = "return " + source;
    if (luaL_loadbufferx(L_, expr.data(), expr.size(), "debug_eval", "t") == LUA_OK) {
        rc = lua_pcall(L_, 0, 1, 0);
    } else {
        lua_pop(L_, 1);  // 表达式编译错误 → 回退
        if (luaL_loadbufferx(L_, source.data(), source.size(), "debug_eval", "t") !=
            LUA_OK) {
            const std::string msg =
                lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "";
            lua_pop(L_, 1);
            result = msg;  // 失败面 result = 错误原文
            fault(ScriptFaultEvent::Kind::kSyntax, "debug_eval", msg);
            return false;
        }
        rc = lua_pcall(L_, 0, 0, 0);
    }
    stats_.instructions += used_instructions_;
    if (rc != LUA_OK) {
        const std::string msg = lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "";
        lua_pop(L_, 1);
        result = msg;  // 失败面 result = 错误原文
        fault(classify(rc, msg), "debug_eval", msg);
        return false;
    }
    // 结果序列化：表达式路径才有返回值（nil/无值 = 空串；不可转换给类型名）。
    if (lua_gettop(L_) > 0 && !lua_isnil(L_, -1)) {
        std::size_t len = 0;
        const char* s = lua_tolstring(L_, -1, &len);
        if (!s) {
            s = lua_typename(L_, lua_type(L_, -1));
            len = std::strlen(s);
        }
        result.assign(s, std::min(len, config_.debug_eval_result_max));
        lua_pop(L_, 1);
    } else {
        result.clear();
        if (lua_gettop(L_) > 0) {
            lua_pop(L_, 1);  // 表达式路径的 nil
        }
    }
    warn_check("debug_eval", used_instructions_);
    return true;
}

bool ScriptRuntime::call_string(const char* chunk, const std::string& source) {
    ++stats_.calls;
    used_instructions_ = 0;
    return run_chunk(chunk, source);
}

bool ScriptRuntime::call_global(const char* func) {
    ++stats_.calls;
    used_instructions_ = 0;
    raw_getglobal(L_, func);
    if (!lua_isfunction(L_, -1)) {
        lua_pop(L_, 1);
        fault(ScriptFaultEvent::Kind::kRuntime, func, "not a function");
        return false;
    }
    const int rc = lua_pcall(L_, 0, 0, 0);
    stats_.instructions += used_instructions_;
    if (rc != LUA_OK) {
        const std::string msg = lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "";
        lua_pop(L_, 1);
        fault(classify(rc, msg), func, msg);
        return false;
    }
    warn_check(func, used_instructions_);
    return true;
}

// §3.5 缓存装载：manifest 三元组三重校验全过才走 "b" 分支。失效规则只有三条
// （Lua 版本变 / 源变 / 装载器逻辑变）在构建期重跑管线；运行期任一校验
// 不过（含产物缺失/损坏、清单未命中）= 回退源码编译，装载永不因缓存失败。
int ScriptRuntime::load_source(const char* chunk, const std::string& source,
                               bool& used_cache) {
    // 未命中面统一回退：计 miss 后源码 "t" 编译（缓存关闭不计数——统计只
    // 反映缓存面本身）。
    auto fallback = [&]() -> int {
        ++stats_.cache_misses;
        return luaL_loadbufferx(L_, source.data(), source.size(), chunk, "t");
    };
    used_cache = false;
    if (bytecode_cache_dir_.empty()) {
        return luaL_loadbufferx(L_, source.data(), source.size(), chunk, "t");
    }
    if (!manifest_loaded_) {
        manifest_ = std::make_unique<std::vector<ManifestEntry>>(
            load_manifest(bytecode_cache_dir_ + "/manifest"));
        manifest_loaded_ = true;
    }
    for (const auto& e : *manifest_) {
        if (e.name != chunk) {
            continue;
        }
        // 规则①源变：manifest 源 hash 与当前源不符 = 回退（管线应重跑）。
        if (e.source_hash !=
            fnv1a64(reinterpret_cast<const std::uint8_t*>(source.data()),
                    source.size())) {
            return fallback();
        }
        // 规则③版本变：格式签名（版本 + 尺寸集）不符 = 回退源码编译。
        if (e.format_sig != bytecode_format_signature()) {
            return fallback();
        }
        std::vector<std::uint8_t> bytes;
        if (!read_bytes_file(bytecode_cache_dir_ + "/" + std::string(chunk) + ".luac",
                             bytes)) {
            return fallback();
        }
        // 产物损坏（整块 hash 不符）= 回退，不加载可疑字节码。
        if (e.bytecode_hash != fnv1a64(bytes.data(), bytes.size())) {
            return fallback();
        }
        const int rc = luaL_loadbufferx(L_, reinterpret_cast<const char*>(bytes.data()),
                                        bytes.size(), chunk, "b");
        if (rc != LUA_OK) {
            lua_pop(L_, 1);
            return fallback();
        }
        used_cache = true;
        ++stats_.cache_hits;
        return rc;
    }
    return fallback();  // 清单无该 chunk 条目（未预编译）
}

bool ScriptRuntime::run_chunk(const char* chunk, const std::string& source) {
    bool chunk_from_cache = false;
    if (load_source(chunk, source, chunk_from_cache) != LUA_OK) {
        const std::string msg = lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "";
        lua_pop(L_, 1);
        fault(ScriptFaultEvent::Kind::kSyntax, chunk, msg);
        return false;
    }
    const int rc = lua_pcall(L_, 0, 0, 0);
    stats_.instructions += used_instructions_;
    if (rc != LUA_OK) {
        const std::string msg = lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "";
        lua_pop(L_, 1);
        fault(classify(rc, msg), chunk, msg);
        return false;
    }
    warn_check(chunk, used_instructions_);  // §6.1 WARN：≥80% 预算完成
    return true;
}

void ScriptRuntime::set_global_string(const char* name, const std::string& value) {
    lua_pushlstring(L_, value.data(), value.size());
    raw_setglobal(L_, name);
}

std::string ScriptRuntime::get_global_string(const char* name) {
    raw_getglobal(L_, name);
    size_t len = 0;
    const char* s = lua_tolstring(L_, -1, &len);  // 布尔转换失败返回空
    std::string out(s ? s : "", s ? len : 0u);
    lua_pop(L_, 1);
    return out;
}

bool ScriptRuntime::get_global_bool(const char* name) {
    raw_getglobal(L_, name);
    const bool out = lua_isboolean(L_, -1) && lua_toboolean(L_, -1) != 0;
    lua_pop(L_, 1);
    return out;
}

void ScriptRuntime::reset_budget() { used_instructions_ = 0; }

std::size_t ScriptRuntime::memory_used() const { return allocated_; }

ScriptFaultEvent::Kind ScriptRuntime::classify(int rc, const std::string& msg) const {
    if (rc == LUA_ERRMEM) {
        return ScriptFaultEvent::Kind::kMemoryLimit;
    }
    if (msg.find("instruction budget") != std::string::npos) {
        return ScriptFaultEvent::Kind::kInstructionBudget;
    }
    return ScriptFaultEvent::Kind::kRuntime;
}

void ScriptRuntime::fault(ScriptFaultEvent::Kind kind, const char* chunk,
                          const std::string& msg) {
    ++stats_.faults;
    switch (kind) {
        case ScriptFaultEvent::Kind::kSyntax:
            ++stats_.fault_syntax;
            break;
        case ScriptFaultEvent::Kind::kRuntime:
            ++stats_.fault_runtime;
            break;
        case ScriptFaultEvent::Kind::kMemoryLimit:
            ++stats_.fault_memory;
            break;
        case ScriptFaultEvent::Kind::kInstructionBudget:
            ++stats_.fault_budget;
            break;
        case ScriptFaultEvent::Kind::kWarnBudget:
        case ScriptFaultEvent::Kind::kCircuitTripped:
        case ScriptFaultEvent::Kind::kCircuitRecovered:
            break;  // 非 fault 级（走 send 直出，不经本函数计数）
    }
    send(ScriptFaultEvent{kind, chunk, msg});
}

void ScriptRuntime::send(ScriptFaultEvent&& e) {
    e.ts_wall_ms = wall_now_ms();
    e.tick = tick_;
    e.consumed = used_instructions_;
    e.budget = config_.instruction_budget;
    if (sink_) {
        sink_(e);
    }
}

void ScriptRuntime::warn_check(const char* chunk, std::uint64_t consumed) {
    const std::uint32_t pct = config_.warn_threshold_percent;
    if (pct == 0 || config_.instruction_budget == 0 ||
        consumed * 100 < config_.instruction_budget * pct) {
        return;  // 阈值下/关闭/不限预算：静默
    }
    const auto it = warn_ticks_.find(chunk);
    // 存 tick+1（0 = 从未 WARN），tick 0 首警不被误判为已警。
    const std::uint64_t stamp = it != warn_ticks_.end() ? it->second : 0;
    if (stamp != 0 && tick_ - (stamp - 1) < kWarnDebounceTicks) {
        return;  // §6.1 去抖：同 chunk 256 tick 内至多一条
    }
    warn_ticks_[chunk] = tick_ + 1;
    ++stats_.budget_warns;
    send(ScriptFaultEvent{ScriptFaultEvent::Kind::kWarnBudget, chunk,
                          "instruction budget near limit"});
}

// ---- alloc/hook 回调桥（friend 面最小化：单字段访问器） ----

void ScriptRuntime::release_bytes(std::size_t n) { allocated_ -= n; }

void ScriptRuntime::acquire_bytes(std::size_t n) { allocated_ += n; }

bool ScriptRuntime::exceeds_limit(std::size_t delta) const {
    return config_.memory_limit_bytes != 0 &&
           allocated_ + delta > config_.memory_limit_bytes;
}

std::uint64_t& ScriptRuntime::fault_memory_counter() { return stats_.fault_memory; }

void ScriptRuntime::tick_instructions(lua_Integer step) {
    used_instructions_ += static_cast<std::uint64_t>(step);
    if (config_.instruction_budget != 0 &&
        used_instructions_ >= config_.instruction_budget) {
        luaL_error(L_, "instruction budget exceeded");
    }
}

}  // namespace script
}  // namespace apollo
