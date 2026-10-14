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

// §7.3 超标事件环上限（同 reload 审计环——防热更循环无界增长）。
constexpr std::size_t kOverBudgetCap = 64;

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

// §7.3 归因墙钟（steady——单调，不受系统时间回拨影响；微秒粒度）。
std::uint64_t steady_now_us() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// §8.3 超时清点墙钟（steady-ms）。
std::uint64_t steady_now_ms() {
    return steady_now_us() / 1000;
}

// §8.2 异步交接物取值（只读 API，不触元方法；数值/串分型，无就地转换——
// 栈上必是字符串才走 tolstring）。
LuaValue read_async_arg(lua_State* L, int idx) {
    switch (lua_type(L, idx)) {
        case LUA_TBOOLEAN:
            return LuaValue::of_bool(lua_toboolean(L, idx) != 0);
        case LUA_TNUMBER:
            if (lua_isinteger(L, idx)) {
                return LuaValue::of_int(
                    static_cast<std::int64_t>(lua_tointeger(L, idx)));
            }
            return LuaValue::of_double(lua_tonumber(L, idx));
        case LUA_TSTRING: {
            std::size_t len = 0;
            const char* s = lua_tolstring(L, idx, &len);
            return LuaValue::of_string(std::string(s ? s : "", len));
        }
        default:
            return LuaValue::nil();  // nil/table/function 等 → nil（单值语义）
    }
}

// §8.2 完成注入（typed push）。
void push_async_value(lua_State* L, const LuaValue& v) {
    switch (v.type) {
        case LuaValue::Type::kNil: lua_pushnil(L); break;
        case LuaValue::Type::kBool: lua_pushboolean(L, v.b ? 1 : 0); break;
        case LuaValue::Type::kInt:
            lua_pushinteger(L, static_cast<lua_Integer>(v.i));
            break;
        case LuaValue::Type::kDouble: lua_pushnumber(L, v.d); break;
        case LuaValue::Type::kString:
            lua_pushlstring(L, v.s.data(), v.s.size());
            break;
    }
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
    self->tick_instructions(L, kHookStep);
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

    // §8 异步底座面：apollo.async 模块 + coroutine.create/wrap 登记（协程
    // 登记表 = §7.2 协程清单数据源）。
    install_async_surface_();
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

// ---- §7.3 归因分桶：调用边界入账（hook 指令计数已付成本上分桶） ----

void ScriptRuntime::profile_record(const char* chunk, std::uint64_t instructions,
                                   std::uint64_t wall_us) {
    auto& b = profile_[chunk];
    b.chunk = chunk;
    ++b.calls;
    b.instructions += instructions;
    b.wall_us += wall_us;
}

std::vector<ProfileBucket> ScriptRuntime::profile_snapshot() const {
    std::vector<ProfileBucket> out;
    out.reserve(profile_.size());
    for (const auto& kv : profile_) {
        out.push_back(kv.second);
    }
    return out;
}

void ScriptRuntime::profile_reset() {
    profile_.clear();
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
    hook_name_ = global_name;                    // §7.3 归因键
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
        const auto t0 = steady_now_us();
        const int rc = lua_pcall(L_, 4, 1, 0);
        frame_hook_used_ += used_instructions_;
        profile_record(hook_name_.c_str(), used_instructions_, steady_now_us() - t0);
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
            const auto t0 = steady_now_us();
            const int rc = lua_pcall(L_, 0, 0, 0);
            stats_.instructions += used_instructions_;
            profile_record(module.c_str(), used_instructions_, steady_now_us() - t0);
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
    const auto t0 = steady_now_us();
    const int rc = lua_pcall(L_, 0, 0, 0);
    stats_.instructions += used_instructions_;
    profile_record(it->second.c_str(), used_instructions_, steady_now_us() - t0);
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
    const auto t0 = steady_now_us();
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
    profile_record("debug_eval", used_instructions_, steady_now_us() - t0);
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
    const auto t0 = steady_now_us();
    const int rc = lua_pcall(L_, 0, 0, 0);
    stats_.instructions += used_instructions_;
    profile_record(func, used_instructions_, steady_now_us() - t0);
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
    const auto t0 = steady_now_us();
    const int rc = lua_pcall(L_, 0, 0, 0);
    stats_.instructions += used_instructions_;
    profile_record(chunk, used_instructions_, steady_now_us() - t0);
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
    if (kind == ScriptFaultEvent::Kind::kInstructionBudget) {
        // §7.3 超标事件进状态面（哪个 chunk 触发、当次消耗；事件面同拍仍发）。
        if (over_budget_history_.size() == kOverBudgetCap) {
            over_budget_history_.pop_front();
        }
        over_budget_history_.push_back(
            OverBudgetRecord{chunk, tick_, used_instructions_,
                             config_.instruction_budget});
    }
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

// ---- §8 异步底座（L1 交接 + L2 协程面） ----

void ScriptRuntime::install_async_surface_() {
    // coroutine.create/wrap/resume 替换：stdlib 原件不登记——脚本自建协程
    // 必须进登记表（§7.2 清单 + 指令 hook），且首跑须打 started（终态懒清扫
    // 判据），否则清单有盲区/永挂 ready。status/yield 沿用 stdlib（同一
    // thread 对象，宿主只加登记）。stdlib 表无元表，raw set 亦安全。
    raw_getglobal(L_, "coroutine");                  // [coro]
    lua_pushstring(L_, "create");                    // [coro, "create"]
    lua_pushcfunction(L_, coroutine_create_impl_);   // [coro, "create", fn]
    lua_rawset(L_, -3);                              // coro.create = fn → [coro]
    lua_pushstring(L_, "wrap");
    lua_pushcfunction(L_, coroutine_wrap_impl_);
    lua_rawset(L_, -3);                              // [coro]
    lua_pushstring(L_, "resume");
    lua_pushcfunction(L_, coroutine_resume_impl_);
    lua_rawset(L_, -3);                              // [coro]
    lua_pop(L_, 1);                                  // []

    // apollo.async = { call = ... }（嵌套全局表；apollo 缺则先建——ctor 期
    // 无脚本运行，raw 建/取即安全）。
    lua_newtable(L_);                                // [async]
    lua_pushcfunction(L_, async_call_impl_);
    lua_setfield(L_, -2, "call");                    // [async]
    raw_getglobal(L_, "apollo");                     // [async, apollo?]
    if (!lua_istable(L_, -1)) {
        lua_pop(L_, 1);                              // [async]
        lua_newtable(L_);                            // [async, t]
        raw_setglobal(L_, "apollo");                 // [async]
        raw_getglobal(L_, "apollo");                 // [async, apollo]
    }
    lua_pushstring(L_, "async");                     // [async, apollo, "async"]
    lua_pushvalue(L_, -3);                           // [.., "async", async]
    lua_rawset(L_, -3);                              // apollo.async = async → [async, apollo]
    lua_pop(L_, 2);
}

void ScriptRuntime::register_async_service(const char* name, AsyncHandler handler,
                                           std::uint64_t timeout_ms) {
    async_services_[name] = AsyncService{std::move(handler), timeout_ms};
}

void ScriptRuntime::set_async_validator(
    std::function<bool(const LuaCoroutineInfo&)> validator) {
    async_validator_ = std::move(validator);
}

std::uint64_t ScriptRuntime::register_coroutine_(lua_State* T, std::string origin,
                                                 std::string module) {
    // T 须在 L_ 栈顶：registry anchor（挂起协程不被 GC 回收），随后弹栈。
    const int ref = luaL_ref(L_, LUA_REGISTRYINDEX);
    const std::uint64_t handle = next_coroutine_handle_++;
    CoroutineEntry e;
    e.thread = T;
    e.thread_ref = ref;
    e.origin = std::move(origin);
    e.module = std::move(module);
    e.born_tick = tick_;
    coroutines_.emplace(handle, std::move(e));
    coroutine_by_thread_[T] = handle;
    return handle;
}

std::map<std::uint64_t, ScriptRuntime::CoroutineEntry>::iterator
ScriptRuntime::drop_coroutine_(CoroutineMap::iterator it) {
    luaL_unref(L_, LUA_REGISTRYINDEX, it->second.thread_ref);
    coroutine_by_thread_.erase(it->second.thread);
    return coroutines_.erase(it);
}

bool ScriptRuntime::call_coroutine(const std::string& module, const char* func) {
    auto bit = breakers_.find(module);
    if (bit != breakers_.end() && bit->second.quarantined_until != 0 &&
        tick_ < bit->second.quarantined_until) {
        ++stats_.circuit_rejects;  // 隔离期短路，不进 Lua
        return false;
    }
    ++stats_.calls;
    used_instructions_ = 0;
    raw_getglobal(L_, module.c_str());
    if (!lua_istable(L_, -1)) {
        lua_pop(L_, 1);
        fault(ScriptFaultEvent::Kind::kRuntime, module.c_str(), "module not a table");
        return false;
    }
    raw_getfield(L_, -1, func);
    if (!lua_isfunction(L_, -1)) {
        lua_pop(L_, 2);
        fault(ScriptFaultEvent::Kind::kRuntime, module.c_str(),
              "handler not a function");
        return false;
    }
    // [module, func] → 新协程线程承载（§8.2：跨 yield 业务以协程形态运行）
    lua_newthread(L_);                            // [module, func, T]
    lua_sethook(lua_tothread(L_, -1), script_hook, LUA_MASKCOUNT,
                static_cast<int>(kHookStep));
    lua_insert(L_, -2);                           // [module, T, func]
    lua_State* T = lua_tothread(L_, -2);
    lua_xmove(L_, T, 1);                          // func → T；L_: [module, T]
    const std::uint64_t handle =
        register_coroutine_(T, module + ":" + func, module);  // 弹 T → [module]
    lua_pop(L_, 1);
    auto& e = coroutines_[handle];
    e.started = true;
    const auto t0 = steady_now_us();
    int nres = 0;
    const int rc = lua_resume(T, L_, 0, &nres);
    stats_.instructions += used_instructions_;
    profile_record(e.origin.c_str(), used_instructions_, steady_now_us() - t0);
    return finalize_resume_(handle, rc, nres);
}

bool ScriptRuntime::finalize_resume_(std::uint64_t handle, int rc, int nres,
                                      bool injected_error) {
    auto eit = coroutines_.find(handle);
    if (eit == coroutines_.end()) {
        return rc == LUA_OK;
    }
    CoroutineEntry& e = eit->second;
    if (rc == LUA_OK) {
        lua_settop(e.thread, 0);  // 结果已交付（wrap 由脚本取），栈面清零
        if (!e.module.empty()) {
            breaker_on_success(e.module);
        }
        warn_check(e.origin.c_str(), used_instructions_);
        drop_coroutine_(eit);
        return true;
    }
    if (rc == LUA_YIELD) {
        if (!e.awaiting) {
            e.reason = "yield";  // 裸 yield（脚本自管挂起）；await 已在 call impl 记因
        }
        return true;
    }
    // 执行错误：协程终止（§8.3 纪律 2——脚本侧 pcall 可捕获，此处走 §6 审计）。
    const std::string msg = lua_tostring(e.thread, -1) ? lua_tostring(e.thread, -1) : "";
    lua_settop(e.thread, 0);
    fault(classify(rc, msg), e.origin.c_str(), msg);
    // 注入错误（超时/取消/失效）是宿主发起的运营终态，不计模块熔断——
    // 否则停机取消会把全部业务模块打进隔离；仅真实执行错误归因熔断。
    if (!injected_error && !e.module.empty()) {
        breaker_on_fault(e.module, false);
    }
    drop_coroutine_(eit);
    return false;
}

void ScriptRuntime::resume_with_completion_(std::uint64_t handle,
                                            AsyncCompletion&& comp) {
    auto eit = coroutines_.find(handle);
    if (eit == coroutines_.end()) {
        return;
    }
    CoroutineEntry& e = eit->second;
    e.awaiting = false;
    e.request_id = 0;
    e.reason.clear();
    e.timeout_ms = 0;
    e.deadline_ms = 0;
    const bool injected = comp.cause != AsyncCause::kDone;
    const std::uint64_t rid = comp.request_id;
    async_delivered_[rid] = std::move(comp);
    ++stats_.calls;
    used_instructions_ = 0;
    const auto t0 = steady_now_us();
    int nres = 0;
    const int rc = lua_resume(e.thread, L_, 0, &nres);
    stats_.instructions += used_instructions_;
    profile_record(e.origin.c_str(), used_instructions_, steady_now_us() - t0);
    async_delivered_.erase(rid);  // cont 已消费；防御性清残留
    finalize_resume_(handle, rc, nres, injected);
}

int ScriptRuntime::async_call_impl_(lua_State* L) {
    lua_pushlightuserdata(L, const_cast<char*>(&kSelfKey));
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto* self = static_cast<ScriptRuntime*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!self) {
        return luaL_error(L, "async: runtime missing");
    }
    const char* svc = luaL_checkstring(L, 1);
    if (L == self->L_) {
        return luaL_error(L, "apollo.async.call outside coroutine");
    }
    auto sit = self->async_services_.find(svc);
    if (sit == self->async_services_.end()) {
        return luaL_error(L, "async service not registered: %s", svc);
    }
    auto tit = self->coroutine_by_thread_.find(L);
    if (tit == self->coroutine_by_thread_.end()) {
        return luaL_error(L, "async: untracked coroutine");
    }
    auto eit = self->coroutines_.find(tit->second);
    if (eit == self->coroutines_.end()) {
        return luaL_error(L, "async: untracked coroutine");
    }
    std::vector<LuaValue> args;
    const int n = lua_gettop(L);
    args.reserve(static_cast<std::size_t>(n > 1 ? n - 1 : 0));
    for (int i = 2; i <= n; ++i) {
        args.push_back(read_async_arg(L, i));
    }
    const std::uint64_t rid = self->next_request_id_++;
    CoroutineEntry& e = eit->second;
    const std::uint64_t timeout =
        sit->second.timeout_ms != 0 ? sit->second.timeout_ms
                                    : self->config_.async_default_timeout_ms;
    e.awaiting = true;
    e.request_id = rid;
    e.reason = std::string("await ") + svc;
    e.timeout_ms = timeout;
    e.deadline_ms = timeout != 0 ? steady_now_ms() + timeout : 0;
    self->async_pending_[rid] = tit->second;
    ++self->stats_.async_requests;
    {
        // 受理即返回（L0 纪律：不阻塞 owning 线程）。实参 vector 作用域收在
        // yield 之前——lua_yieldk 跳栈不走 C++ 析构，RAII 局部不得横跨挂起点。
        std::vector<LuaValue> handler_args = std::move(args);
        sit->second.handler(rid, handler_args);
    }
    return lua_yieldk(L, 0, static_cast<lua_KContext>(rid), async_cont_);
}

int ScriptRuntime::async_cont_(lua_State* L, int /*status*/, lua_KContext ctx) {
    lua_pushlightuserdata(L, const_cast<char*>(&kSelfKey));
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto* self = static_cast<ScriptRuntime*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!self) {
        return luaL_error(L, "async: runtime missing");
    }
    const auto rid = static_cast<std::uint64_t>(ctx);
    auto it = self->async_delivered_.find(rid);
    if (it == self->async_delivered_.end()) {
        lua_pushliteral(L, "async internal: completion missing");
        return lua_error(L);
    }
    AsyncCompletion comp = std::move(it->second);
    self->async_delivered_.erase(it);
    if (comp.cause != AsyncCause::kDone) {
        // 超时/取消/失败/失效 → 协程以错误终止（§8.3 纪律 2/3；脚本侧
        // pcall 捕获，走 §6 错误审计）。错误串先复制进 Lua，再以 swap 释放
        // 宿主侧副本——lua_error 跳栈不走 C++ 析构，且 string move-assign
        // 对 SSO 右值不释放左值堆缓冲，必须显式换出。
        lua_pushlstring(L, comp.error.data(), comp.error.size());
        std::string().swap(comp.error);
        return lua_error(L);
    }
    push_async_value(L, comp.value);
    return 1;
}

int ScriptRuntime::coroutine_create_impl_(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushlightuserdata(L, const_cast<char*>(&kSelfKey));
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto* self = static_cast<ScriptRuntime*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!self) {
        return luaL_error(L, "async: runtime missing");
    }
    lua_newthread(L);                              // [f, T]
    lua_sethook(lua_tothread(L, -1), script_hook, LUA_MASKCOUNT,
                static_cast<int>(kHookStep));
    lua_insert(L, -2);                             // [T, f]
    lua_State* T = lua_tothread(L, -2);
    lua_xmove(L, T, 1);                            // f → T；L: [T]
    const std::uint64_t handle = self->register_coroutine_(T, "coroutine.create", "");
    lua_rawgeti(L, LUA_REGISTRYINDEX, self->coroutines_[handle].thread_ref);
    return 1;
}

int ScriptRuntime::coroutine_wrap_impl_(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushlightuserdata(L, const_cast<char*>(&kSelfKey));
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto* self = static_cast<ScriptRuntime*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!self) {
        return luaL_error(L, "async: runtime missing");
    }
    lua_newthread(L);                              // [f, T]
    lua_sethook(lua_tothread(L, -1), script_hook, LUA_MASKCOUNT,
                static_cast<int>(kHookStep));
    lua_insert(L, -2);                             // [T, f]
    lua_State* T = lua_tothread(L, -2);
    lua_xmove(L, T, 1);                            // f → T；L: [T]
    const std::uint64_t handle =
        self->register_coroutine_(T, "coroutine.wrap", "");  // 弹 T
    lua_rawgeti(L, LUA_REGISTRYINDEX,
                self->coroutines_[handle].thread_ref);   // 重压 T
    lua_pushcclosure(L, coroutine_wrap_resume_, 1);  // T 作 upvalue
    return 1;
}

int ScriptRuntime::coroutine_wrap_resume_(lua_State* L) {
    lua_pushlightuserdata(L, const_cast<char*>(&kSelfKey));
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto* self = static_cast<ScriptRuntime*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!self) {
        return luaL_error(L, "async: runtime missing");
    }
    lua_State* T = lua_tothread(L, lua_upvalueindex(1));
    auto tit = self->coroutine_by_thread_.find(T);
    const std::uint64_t handle =
        tit != self->coroutine_by_thread_.end() ? tit->second : 0;
    // 指针直取（map 内串，函数期有效）：lua_error 跳栈不走 C++ 析构，
    // 错误路径不得横跨堆串局部。
    const char* origin = "coroutine.wrap";
    if (handle != 0) {
        auto eit = self->coroutines_.find(handle);
        if (eit != self->coroutines_.end()) {
            eit->second.started = true;  // 首跑打点（懒清扫判据）
            origin = eit->second.origin.c_str();
        }
    }
    const int n = lua_gettop(L);
    lua_xmove(L, T, n);
    ++self->stats_.calls;
    self->used_instructions_ = 0;
    const auto t0 = steady_now_us();
    int nres = 0;
    const int rc = lua_resume(T, L, n, &nres);
    self->stats_.instructions += self->used_instructions_;
    self->profile_record(origin, self->used_instructions_,
                         steady_now_us() - t0);
    if (rc == LUA_OK || rc == LUA_YIELD) {
        if (rc == LUA_YIELD && handle != 0) {
            auto eit = self->coroutines_.find(handle);
            if (eit != self->coroutines_.end() && !eit->second.awaiting) {
                eit->second.reason = "yield";  // 裸 yield 记因
            }
        }
        lua_xmove(T, L, nres);
        return nres;
    }
    // wrap 语义：错误抛给调用者（pcall 捕获面）；终态懒清扫归 snapshot。
    lua_xmove(T, L, 1);
    return lua_error(L);
}

int ScriptRuntime::coroutine_resume_impl_(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTHREAD);
    lua_pushlightuserdata(L, const_cast<char*>(&kSelfKey));
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto* self = static_cast<ScriptRuntime*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!self) {
        return luaL_error(L, "async: runtime missing");
    }
    lua_State* T = lua_tothread(L, 1);
    auto tit = self->coroutine_by_thread_.find(T);
    const std::uint64_t handle =
        tit != self->coroutine_by_thread_.end() ? tit->second : 0;
    if (handle != 0) {
        auto eit = self->coroutines_.find(handle);
        if (eit != self->coroutines_.end()) {
            eit->second.started = true;  // 首跑打点（懒清扫判据）
        }
    }
    const int n = lua_gettop(L) - 1;               // 除 co 外全是实参
    for (int i = 2; i <= n + 1; ++i) {             // [co, a1..an]
        lua_pushvalue(L, i);
        lua_xmove(L, T, 1);
    }
    ++self->stats_.calls;
    self->used_instructions_ = 0;
    const auto t0 = steady_now_us();
    int nres = 0;
    const int rc = lua_resume(T, L, n, &nres);
    self->stats_.instructions += self->used_instructions_;
    if (handle != 0) {
        auto eit = self->coroutines_.find(handle);
        if (eit != self->coroutines_.end()) {
            self->profile_record(eit->second.origin.c_str(),
                                 self->used_instructions_, steady_now_us() - t0);
            if (rc == LUA_YIELD && !eit->second.awaiting) {
                eit->second.reason = "yield";
            }
        }
    }
    if (rc == LUA_OK || rc == LUA_YIELD) {
        lua_pushboolean(L, 1);                     // stdlib 语义：true + 结果
        lua_xmove(T, L, nres);
        return nres + 1;
    }
    lua_pushboolean(L, 0);                         // false + 错误对象
    lua_xmove(T, L, 1);
    return 2;
}

void ScriptRuntime::complete_async(std::uint64_t request_id, const LuaValue& value) {
    std::lock_guard<std::mutex> guard(async_mtx_);
    completions_.push_back(AsyncCompletion{request_id, AsyncCause::kDone, "", value});
}

void ScriptRuntime::fail_async(std::uint64_t request_id, std::string error) {
    std::lock_guard<std::mutex> guard(async_mtx_);
    completions_.push_back(
        AsyncCompletion{request_id, AsyncCause::kError, std::move(error), {}});
}

void ScriptRuntime::cancel_pending(const char* reason) {
    std::lock_guard<std::mutex> guard(async_mtx_);
    for (auto& kv : coroutines_) {
        if (kv.second.awaiting) {
            completions_.push_back(AsyncCompletion{
                kv.second.request_id, AsyncCause::kCancelled,
                std::string("cancelled: ") + (reason ? reason : ""), {}});
        }
    }
}

void ScriptRuntime::drain_async() {
    // 1) 超时清点（§8.3 纪律 3：超时先到 → 超时错误 resume，迟到结果丢弃+计数）。
    const std::uint64_t now = steady_now_ms();
    {
        std::lock_guard<std::mutex> guard(async_mtx_);
        for (auto& kv : coroutines_) {
            CoroutineEntry& e = kv.second;
            if (e.awaiting && e.deadline_ms != 0 && now >= e.deadline_ms) {
                completions_.push_back(AsyncCompletion{
                    e.request_id, AsyncCause::kTimeout, "timeout", {}});
            }
        }
    }
    // 2) 取走完成队列（含 cancel 合成的；complete_async 期间新到 → 下拍）。
    std::deque<AsyncCompletion> queue;
    {
        std::lock_guard<std::mutex> guard(async_mtx_);
        queue.swap(completions_);
    }
    // 3) 逐条投递：无主丢弃计数 → validator → resume。
    while (!queue.empty()) {
        AsyncCompletion comp = std::move(queue.front());
        queue.pop_front();
        auto pit = async_pending_.find(comp.request_id);
        if (pit == async_pending_.end()) {
            ++stats_.async_late;  // 迟到/重复/无主完成：丢弃 + 计数
            continue;
        }
        const std::uint64_t handle = pit->second;
        async_pending_.erase(pit);
        auto eit = coroutines_.find(handle);
        if (eit == coroutines_.end()) {
            ++stats_.async_late;
            continue;
        }
        CoroutineEntry& e = eit->second;
        if (comp.cause == AsyncCause::kDone && async_validator_) {
            LuaCoroutineInfo info;
            info.handle = handle;
            info.origin = e.origin;
            info.suspend_reason = e.reason;
            info.pending_request = e.request_id;
            if (!async_validator_(info)) {
                comp.cause = AsyncCause::kInvalid;
                comp.error = "context invalid (re-validate failed)";
            }
        }
        switch (comp.cause) {
            case AsyncCause::kDone: ++stats_.async_completed; break;
            case AsyncCause::kError: ++stats_.async_failed; break;
            case AsyncCause::kTimeout: ++stats_.async_timeouts; break;
            case AsyncCause::kCancelled: ++stats_.async_cancelled; break;
            case AsyncCause::kInvalid: ++stats_.async_invalid; break;
        }
        resume_with_completion_(handle, std::move(comp));
    }
}

std::vector<LuaCoroutineInfo> ScriptRuntime::coroutine_snapshot() {
    std::vector<LuaCoroutineInfo> out;
    for (auto it = coroutines_.begin(); it != coroutines_.end();) {
        CoroutineEntry& e = it->second;
        const int st = lua_status(e.thread);
        // 懒清扫：已启动且非挂起且已结束（LUA_OK = 跑完；错误码 = 脚本侧
        // resume 收尾失败）——清单只呈现活体。
        if (e.started && !e.awaiting &&
            (st == LUA_OK || (st != LUA_OK && st != LUA_YIELD))) {
            it = drop_coroutine_(it);
            continue;
        }
        LuaCoroutineInfo info;
        info.handle = it->first;
        info.origin = e.origin;
        info.born_tick = e.born_tick;
        info.age_ticks = tick_ - e.born_tick;
        if (e.awaiting) {
            info.status = "suspended";
            info.suspend_reason = e.reason;
            info.pending_request = e.request_id;
            info.timeout_ms = e.timeout_ms;
        } else if (!e.started) {
            info.status = "ready";  // 已登记未首跑（脚本 create 后未 resume）
        } else {
            info.status = "suspended";
            info.suspend_reason = e.reason.empty() ? "yield" : e.reason;
        }
        info.leaked = config_.async_leak_threshold_ticks != 0 &&
                      info.age_ticks >= config_.async_leak_threshold_ticks;
        out.push_back(std::move(info));
        ++it;
    }
    return out;  // map 迭代 = handle 升序
}

// ---- alloc/hook 回调桥（friend 面最小化：单字段访问器） ----

void ScriptRuntime::release_bytes(std::size_t n) { allocated_ -= n; }

void ScriptRuntime::acquire_bytes(std::size_t n) { allocated_ += n; }

bool ScriptRuntime::exceeds_limit(std::size_t delta) const {
    return config_.memory_limit_bytes != 0 &&
           allocated_ + delta > config_.memory_limit_bytes;
}

std::uint64_t& ScriptRuntime::fault_memory_counter() { return stats_.fault_memory; }

void ScriptRuntime::tick_instructions(lua_State* L, lua_Integer step) {
    // L = 触发 hook 的运行态（主 state 或协程线程）：超预算错误必须在
    // 触发态上抛（resume 的保护上下文挂在协程线程；固定抛 L_ 会在协程
    // 执行中命中无保护 longjmp → panic abort）。
    used_instructions_ += static_cast<std::uint64_t>(step);
    if (config_.instruction_budget != 0 &&
        used_instructions_ >= config_.instruction_budget) {
        luaL_error(L, "instruction budget exceeded");
    }
}

}  // namespace script
}  // namespace apollo
