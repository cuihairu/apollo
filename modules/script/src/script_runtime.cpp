// ScriptRuntime 沙盒宿主实现（scripting-lua.md §2/§4/§6 第一批）。

#include "apollo/script/script_runtime.hpp"

#include <algorithm>
#include <cstdlib>

namespace apollo {
namespace script {

namespace {

// 指令 hook 粗检步进（§6：粗检每 10k 指令）。
constexpr lua_Integer kHookStep = 10000;

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
    lua_setglobal(L, name);
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
    lua_setglobal(L_, "load");
    // this 入 registry（hook 回调取用）。
    lua_pushlightuserdata(L_, const_cast<char*>(&kSelfKey));
    lua_pushlightuserdata(L_, static_cast<void*>(this));
    lua_settable(L_, LUA_REGISTRYINDEX);
    lua_sethook(L_, script_hook, LUA_MASKCOUNT, static_cast<int>(kHookStep));
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
    lua_setglobal(L_, name);
}

// ---- §3.2 热替换：编译 + 冒烟在影子栈完成，通过才原子换 _G[name] ----

bool ScriptRuntime::swap_module(const ModuleSpec& spec, const std::string& source) {
    // 依赖校验（依赖图拓扑序由装载器保证，本层只查已装载）。
    for (const auto& dep : spec.deps) {
        if (module_refs_.find(dep) == module_refs_.end()) {
            ++stats_.reload_failed;
            return false;
        }
    }
    // 编译（§3.5 红线：运行期字符串按源码处理，禁 bytecode）。
    if (luaL_loadbufferx(L_, source.data(), source.size(), spec.name.c_str(), "t") != LUA_OK) {
        const std::string msg = lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "";
        lua_pop(L_, 1);
        ++stats_.reload_failed;
        fault(ScriptFaultEvent::Kind::kSyntax, spec.name.c_str(), msg);
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
        return false;
    }
    if (!lua_istable(L_, -1)) {
        lua_pop(L_, 1);
        ++stats_.reload_failed;
        fault(ScriptFaultEvent::Kind::kRuntime, spec.name.c_str(),
              "module did not return a table");
        return false;
    }
    // 冒烟闸：新表带 smoke() 则须返回 true（§3.2 Phase A 自检用例集）。
    lua_getfield(L_, -1, "smoke");
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
            return false;
        }
    } else {
        lua_pop(L_, 1);
    }
    // 走到此处栈顶 = 新表。registry ref（luaL_ref 自弹栈顶）+ 原子换表。
    lua_pushvalue(L_, -1);
    const int new_ref = luaL_ref(L_, LUA_REGISTRYINDEX);  // [新表]
    lua_setglobal(L_, spec.name.c_str());                 // []
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
    ++stats_.reload_success;
    if (reload_listener_) {
        reload_listener_(spec.name.c_str(), spec.version);
    }
    // onReload 钩子（§3.2 Phase B 事件钩子：脚本侧重挂订阅）。失败不回滚
    // （换表已原子完成，观察期由 Phase C 处理），记 fault。
    lua_rawgeti(L_, LUA_REGISTRYINDEX, new_ref);
    lua_getfield(L_, -1, "onReload");
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
    lua_setglobal(L_, name.c_str());
    module_versions_[name] = restored_version;
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

// ---- §5.1/§5.2 属性钩子窗口：重算后广播前，同步修订值 ----

bool ScriptRuntime::set_attr_hook(const char* global_name) {
    lua_getglobal(L_, global_name);
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

bool ScriptRuntime::call_string(const char* chunk, const std::string& source) {
    ++stats_.calls;
    used_instructions_ = 0;
    return run_chunk(chunk, source);
}

bool ScriptRuntime::call_global(const char* func) {
    ++stats_.calls;
    used_instructions_ = 0;
    lua_getglobal(L_, func);
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
    return true;
}

bool ScriptRuntime::run_chunk(const char* chunk, const std::string& source) {
    if (luaL_loadbufferx(L_, source.data(), source.size(), chunk, "t") != LUA_OK) {
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
    return true;
}

void ScriptRuntime::set_global_string(const char* name, const std::string& value) {
    lua_pushlstring(L_, value.data(), value.size());
    lua_setglobal(L_, name);
}

std::string ScriptRuntime::get_global_string(const char* name) {
    lua_getglobal(L_, name);
    size_t len = 0;
    const char* s = lua_tolstring(L_, -1, &len);  // 布尔转换失败返回空
    std::string out(s ? s : "", s ? len : 0u);
    lua_pop(L_, 1);
    return out;
}

bool ScriptRuntime::get_global_bool(const char* name) {
    lua_getglobal(L_, name);
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
    }
    if (sink_) {
        sink_(ScriptFaultEvent{kind, chunk, msg});
    }
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
