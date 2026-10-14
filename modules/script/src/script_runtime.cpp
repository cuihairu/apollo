// ScriptRuntime 沙盒宿主实现（scripting-lua.md §2/§4/§6 第一批）。

#include "apollo/script/script_runtime.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>

namespace apollo {
namespace script {

namespace {

// 指令 hook 粗检步进（§6：粗检每 10k 指令）。
constexpr lua_Integer kHookStep = 10000;

// §6.1 WARN 去抖窗（同 chunk 每 256 tick 至多一条）。
constexpr std::uint64_t kWarnDebounceTicks = 256;

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
    breaker_clear(spec.name, "hot swap cleared quarantine");  // §6.1 热更新账
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
    breaker_clear(name, "rollback cleared quarantine");  // §6.1 回滚同热更：新账
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
    lua_getglobal(L_, module.c_str());
    if (!lua_istable(L_, -1)) {
        lua_pop(L_, 1);
        fault(ScriptFaultEvent::Kind::kRuntime, module.c_str(), "module not a table");
    } else {
        lua_getfield(L_, -1, func);
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

// ---- §3.4 契约表装载：编译 → 结构校验 → 路由完整性闸 → 原子提交 ----

bool ScriptRuntime::resolve_handler(const std::string& handler) {
    const std::size_t dot = handler.find('.');
    if (dot == std::string::npos) {
        lua_getglobal(L_, handler.c_str());
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
    lua_getglobal(L_, mod.c_str());
    if (!lua_istable(L_, -1)) {
        lua_pop(L_, 1);
        return false;
    }
    lua_getfield(L_, -1, fn.c_str());
    if (lua_isfunction(L_, -1)) {
        lua_remove(L_, -2);  // [func]
        return true;
    }
    lua_pop(L_, 2);
    return false;
}

bool ScriptRuntime::load_contract(const std::string& source) {
    // 编译（mode "t"）+ 求值；任一步失败旧表/旧数据面不动（§3.4 同 §3.2 协议）。
    if (luaL_loadbufferx(L_, source.data(), source.size(), "contract", "t") != LUA_OK) {
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
    lua_getfield(L_, -1, "routes");  // [table, routes]
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
    lua_getfield(L_, -1, "write_whitelist");  // [table, wl]
    if (!lua_isnil(L_, -1)) {
        if (!lua_istable(L_, -1)) {
            lua_pop(L_, 2);
            fault(ScriptFaultEvent::Kind::kRuntime, "contract",
                  "write_whitelist not a table");
            return false;
        }
        const lua_Integer n = static_cast<lua_Integer>(lua_rawlen(L_, -1));
        for (lua_Integer i = 1; i <= n; ++i) {
            lua_geti(L_, -1, i);
            if (lua_isinteger(L_, -1)) {
                whitelist.push_back(static_cast<std::uint64_t>(lua_tointeger(L_, -1)));
            }
            lua_pop(L_, 1);
        }
    }
    lua_pop(L_, 1);  // wl → [table]
    // version 可选。
    std::uint64_t version = 0;
    lua_getfield(L_, -1, "version");
    if (lua_isinteger(L_, -1)) {
        version = static_cast<std::uint64_t>(lua_tointeger(L_, -1));
    }
    lua_pop(L_, 1);
    // 全部通过 → 原子提交：表与 C++ 数据面同拍生效。
    lua_setglobal(L_, "contract");  // []
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
    warn_check(func, used_instructions_);
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
    warn_check(chunk, used_instructions_);  // §6.1 WARN：≥80% 预算完成
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
    if (config_.instruction_budget == 0 ||
        consumed * 5 < config_.instruction_budget * 4) {
        return;  // < 80% 或不限预算：静默
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
