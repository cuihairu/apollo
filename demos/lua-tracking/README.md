# Lua 属性追踪方案最小 Demo（C / D）

对应 `docs/zh/design/lua-property-sync-options.md` 里的方案 C 与 D。
**不依赖 apollo 主构建**，独立编译运行——目的是先验证机制可行性，再决定是否接入。

## 运行

```bash
# Demo D：写入路径覆盖度（Lua 5.5.1）
# 需要 lua 5.5 的 liblua.a + 头文件（vcpkg: /home/cui/vcpkg/installed/x64-linux）
./run.sh d

# Demo C：PropertyOwner 冒泡（C++20）
./run.sh c

# 两个都跑
./run.sh all
```

`run.sh` 会自动编译 `runner.c`（Lua 宿主）与 `demo-c-owner.cpp`。

## Demo D 验证什么

评审 D 方案的核心断言是「**必须改 Lua 5.5.1 的 C 源码（ltable.c/lvm.c）才能捕获全部写入**」。
本 demo 用真实 Lua 5.5.1 实测四条写入路径，钉死这个断言的边界：

| 写入路径 | `__newindex` 能捕获？ | 说明 |
|---|---|---|
| 新 key 赋值 `t.x = 1` | ✅ | key 不存在时元方法生效 |
| **已存在 key 再赋值 `t.x = 2`** | ❌ | **最致命盲区**：key 已存在，直改表，不走元方法 |
| `rawset(t, k, v)` | ❌ | 明确绕过语义 |
| C API `lua_setfield` / `lua_rawset` | ❌ | apollo pyhost/C++ 侧写入正属此类 |

补充路径 `debug.sethook` 快照比对：**能兜住**上述漏网，但代价是每 N 条 VM 指令全表比对 + 延迟，
大表不可行。

**结论**：零延迟 + 全路径捕获只能在 Lua 的 `ltable.c`（快慢写入路径）、`lvm.c`（VM 指令）、
`lapi.c`（C API）、`lgc.c`（GC）实际写入点插桩——这就是 D 方案的核心代价（fork 引擎）。

## Demo C 验证什么

方案 C 的核心是 BigWorld 的 `PropertyOwnerBase::onOwnedPropertyChanged` + `PropertyChange::path_`
（叶→根 index 序列）。本 demo 用最小 C++ 模型复现：

- 容器 `Container` 持有 N 个 `Vector3`，`init` 时绑定 `parent` + `index`
- 改 `slot(3).z = 4` 后，由引擎钩子调 `bubble()`，index 沿父链压入 path
- 根 `Entity::report` 把 path 反转翻译成 `entity.slot[3]` —— **只发叶子，不整表重发**

输出：
```
--- 玩法脚本等价: state.slot(3).z = 4.0 ---
  [Entity] 深度变更: entity.slot[3]
```

**结论**：path 冒泡模型成立，深度变更可精确增量下发。代价是 apollo 现在没有 owner 链
（属性是 int64 + slice），要照搬这套 attach 架构。

## 边界

- 这两个是**机制验证原型**，不是生产代码，没有接入 `modules/game/attrsync`
- Demo D 只验证「不改源码不够」，**没有**实现改源码的观察器（那需要完整 Lua 源码树 + 重新编译）
- Demo C 用直接调 `bubble()` 模拟引擎钩子；真实接入需要 `DataType::attach` 与脚本写入拦截
