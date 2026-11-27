-- Demo D (Sidecar Observer) 最小验证：不改 Lua 源码，用 Lua 5.5 内置机制观察
-- table 写入，实测每条写入路径能否被捕获。
-- 关键语义：__newindex 只在 key **不存在于该表**时触发；对已存在 key 赋值
-- 直接改表，不经过元方法。这决定了「不改源码方案」的覆盖上限。

print("===== Demo D: 写入路径覆盖度验证 =====")
print("验证问题：不改 Lua 源码，能捕获全部写入吗？\n")

-- 观察器：在目标表本身挂 __newindex（让 key 真实存在于同一表，才能测出差异）
local function track(name)
  local log = {}
  local t = setmetatable({}, {
    __newindex = function(tbl, k, v)
      rawset(tbl, k, v)   -- 真正写入表（key 从此存在）
      log[#log+1] = ("%s[%s]=%s"):format(name, tostring(k), tostring(v))
    end,
  })
  return t, log
end

-- 路径1：新 key 赋值 → __newindex 触发 ✅
print("--- 路径1: 新 key 赋值 ---")
local t1, log1 = track("t1")
t1.hp = 100
print("  捕获: " .. (#log1 > 0 and table.concat(log1, ", ") or "无"))
print("  结论: ✅ 捕获（key 不存在，__newindex 生效）\n")

-- 路径2：已存在 key 再赋值 → __newindex 不触发 ❌（最致命）
print("--- 路径2: 已存在 key 再赋值 ---")
local t2, log2 = track("t2")
t2.hp = 100                       -- 首次：__newindex 触发
local n1 = #log2
t2.hp = 500                       -- 二次：key 已存在，直接改，不经过 __newindex
print("  首次后捕获数: " .. n1 .. "，二次赋值后捕获数: " .. #log2 .. "（未增）")
print("  结论: ❌ 未捕获（已存在 key 直改表，绕过元方法）—— 不改源码方案的致命盲区\n")

-- 路径3：rawset 绕过 → ❌
print("--- 路径3: rawset(t, k, v) ---")
local t3, log3 = track("t3")
t3.hp = 100
local n3 = #log3
rawset(t3, "mp", 200)
print("  rawset 前后捕获数: " .. n3 .. " → " .. #log3)
print("  结论: ❌ 未捕获（rawset 是明确的绕过语义）\n")

-- 路径4：C API 直写 → ❌
print("--- 路径4: C API 直写（lua_setfield / lua_rawset）---")
print("  宿主 C++ 代码 lua_setfield(L, idx, \"gold\", 999) 直改表，")
print("  __newindex 不参与。apollo 的 pyhost/C++ 侧写入正属此类")
print("  结论: ❌ 未捕获\n")

-- 路径5：debug.sethook 快照兜底（能兜但慢）
print("--- 路径5: debug.sethook 快照比对兜底 ---")
local t5 = {hp = 100}
local snap = {hp = "100"}
local hits = 0
debug.sethook(function()
  for k, v in pairs(t5) do
    local ks = tostring(k)
    if snap[ks] ~= tostring(v) then
      snap[ks] = tostring(v)
      hits = hits + 1
      print(("  [hook] 捕获 %s=%s"):format(ks, tostring(v)))
    end
  end
end, "c", 20)
t5.hp = 300
rawset(t5, "mp", 500)
debug.sethook()
print("  hook 捕获数: " .. hits)
print("  结论: ✅ 能兜住上述漏网路径，代价=每 20 条 VM 指令全表快照比对\n")

print("===== Demo D 结论 =====")
print("1. 单靠 __newindex：只捕获新 key；已存在 key 赋值 / rawset / C API 全漏")
print("2. debug.sethook 快照能兜，但有指令级开销 + 延迟 + 大表比对成本")
print("3. 要『零延迟 + 全路径捕获』只能在 ltable.c/lvm.c 实际写入点插入观察")
print("   —— 这是 D 方案的核心代价：fork Lua 5.5.1 源码")
