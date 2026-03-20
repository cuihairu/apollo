# Q80: 协程在游戏服务器中的应用场景？

## 问题分析

本题考察对协程在游戏服务器中应用的理解：
- 协程基础概念
- 与多线程对比
- 异步操作简化
- 游戏逻辑应用

---

## 一、协程基础

### 1.1 什么是协程

```
┌─────────────────────────────────────────────────────────────┐
│                    协程 vs 线程                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  线程 (Thread):                                             │
│  ├── 操作系统级调度                                          │
│  ├── 抢占式调度                                              │
│  ├── 创建开销大 (MB 级栈)                                   │
│  ├── 上下文切换慢 (~微秒)                                    │
│  └── 需要处理同步问题                                        │
│                                                             │
│  协程 (Coroutine):                                          │
│  ├── 用户态调度                                              │
│  ├── 协作式调度 (主动让出)                                   │
│  ├── 创建开销小 (KB 级栈)                                    │
│  ├── 上下文切换快 (~纳秒)                                    │
│  └── 无需锁 (单线程执行)                                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 协程类型

| 类型 | 特点 | 实现示例 |
|------|------|----------|
| **栈式协程** | 有独立栈，可暂停嵌套调用 | Python yield, ucontext |
| **无栈协程** | 状态机实现，轻量 | C++20 coroutine, JavaScript async |
| **对称协程** | 协程间可互相调度 | Lua coroutine |
| **非对称协程** | 调用者与被调用者关系 | Python async/await |

---

## 二、协程实现

### 2.1 Python 协程

```python
# Python asyncio 协程

import asyncio

class Player:
    def __init__(self, player_id):
        self.player_id = player_id
        self.hp = 100

    async def attack(self, target_id):
        """异步攻击"""
        print(f"Player {self.player_id} attacking {target_id}")

        # 模拟网络延迟 (不阻塞其他协程)
        await asyncio.sleep(0.5)  # 等待服务器响应

        damage = 10
        print(f"Player {self.player_id} dealt {damage} damage")

        # 异步等待伤害确认
        await self.confirm_damage(target_id, damage)

    async def confirm_damage(self, target_id, damage):
        """异步确认伤害"""
        # 发送 RPC 请求
        response = await rpc_call("applyDamage", {
            "target": target_id,
            "damage": damage
        })

        if response["success"]:
            print(f"Damage confirmed: {damage}")

    async def move_to(self, x, z):
        """异步移动"""
        print(f"Moving to ({x}, {z})")

        # 分段移动，每段等待
        await asyncio.sleep(0.1)
        print("Step 1")
        await asyncio.sleep(0.1)
        print("Step 2")
        await asyncio.sleep(0.1)
        print("Arrived")

# 协程调度器
async def game_loop():
    players = [Player(i) for i in range(10)]

    # 并发执行多个协程
    tasks = []
    for player in players:
        tasks.append(player.attack(player.player_id + 1))
        tasks.append(player.move_to(100, 200))

    # 等待所有任务完成
    await asyncio.gather(*tasks)

# 运行
asyncio.run(game_loop())
```

### 2.2 C++20 协程

```cpp
// C++20 协程示例

#include <coroutine>
#include <iostream>

template<typename T>
class Task {
public:
    struct promise_type {
        T value_;
        std::exception_ptr exception_;

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_value(T value) { value_ = std::move(value); }
        void unhandled_exception() { exception_ = std::current_exception(); }
    };

    explicit Task(std::coroutine_handle<promise_type> handle)
        : handle_(handle) {}

    ~Task() {
        if (handle_) handle_.destroy();
    }

    // 简化的 awaiter
    bool await_ready() { return handle_.done(); }
    void await_suspend(std::coroutine_handle<> h) {}
    T await_resume() {
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
        return std::move(handle_.promise().value_);
    }

private:
    std::coroutine_handle<promise_type> handle_;
};

// 异步延迟
class Delay {
public:
    Delay(int milliseconds) : ms_(milliseconds) {}

    bool await_ready() const { return ms_ <= 0; }

    void await_suspend(std::coroutine_handle<> handle) {
        // 在实际实现中，这里会注册到定时器
        // 简化: 直接恢复
        handle.resume();
    }

    void await_resume() const {}

private:
    int ms_;
};

// 游戏逻辑协程
Task<void> playerAttack(Player* player, Entity* target) {
    co_await Delay(100);  // 延迟 100ms
    player->dealDamage(target, 10);

    co_await Delay(50);  // 等待确认
    if (target->isAlive()) {
        player->sendHitEffect();
    }
}

Task<void> gameLoop() {
    Player* player = getPlayer();
    Entity* target = getTarget();

    co_await playerAttack(player, target);
}
```

### 2.3 Lua 协程

```lua
-- Lua 协程在游戏中的典型应用

-- 创建协程
local function attackCoroutine(attacker, target)
    print("Starting attack")

    -- 第一步: 检查距离
    local distance = getDistance(attacker, target)
    if distance > 10 then
        -- 移动到攻击范围
        for i = 1, 5 do
            moveTo(attacker, target)
            coroutine.yield()  -- 让出执行权
        end
    end

    -- 第二步: 执行攻击
    playAnimation(attacker, "attack")
    coroutine.yield()  -- 等待动画

    -- 第三步: 计算伤害
    applyDamage(target, 10)

    print("Attack finished")
end

-- 协程管理器
local CoroutineManager = {
    coroutines = {}
}

function CoroutineManager.start(fn, ...)
    local co = coroutine.create(fn)
    table.insert(self.coroutines, co)
    coroutine.resume(co, ...)
    return co
end

function CoroutineManager.update()
    for i = #self.coroutines, 1, -1 do
        local co = self.coroutines[i]

        if coroutine.status(co) == "suspended" then
            coroutine.resume(co)
        else
            -- 协程结束，移除
            table.remove(self.coroutines, i)
        end
    end
end

-- 使用示例
CoroutineManager.start(attackCoroutine, player, enemy)

-- 每帧更新所有协程
function onFrame()
    CoroutineManager.update()
end
```

---

## 三、游戏服务器应用

### 3.1 技能系统

```python
# 协程实现技能系统

import asyncio

class SkillSystem:
    """技能系统"""

    async def cast_skill(self, player, skill_id, target_id):
        """释放技能"""
        skill = self.get_skill(skill_id)

        # 1. 检查冷却
        if not await self.check_cooldown(player, skill_id):
            return False

        # 2. 检查距离
        target = get_entity(target_id)
        if distance(player.position, target.position) > skill.range:
            return False

        # 3. 播放前摇动画
        await self.play_animation(player, skill.cast_animation)
        await asyncio.sleep(skill.cast_time)

        # 4. 检查目标是否还在范围内
        if distance(player.position, target.position) > skill.range:
            return False

        # 5. 应用效果
        await self.apply_skill_effect(player, target, skill)

        # 6. 播放后摇动画
        await self.play_animation(player, skill.after_animation)
        await asyncio.sleep(skill.after_time)

        # 7. 进入冷却
        await self.start_cooldown(player, skill_id)

        return True

    async def apply_skill_effect(self, player, target, skill):
        """应用技能效果"""
        for effect in skill.effects:
            if effect.type == "damage":
                await self.deal_damage(target, effect.value)
            elif effect.type == "heal":
                await self.heal(target, effect.value)
            elif effect.type == "buff":
                await self.apply_buff(target, effect.buff_id)

    async def deal_damage(self, target, damage):
        """造成伤害"""
        # 异步 RPC 调用
        response = await rpc_call("dealDamage", {
            "target": target.id,
            "damage": damage
        })
        return response
```

### 3.2 任务系统

```python
# 协程实现任务系统

class QuestSystem:
    """任务系统"""

    async def accept_quest(self, player, quest_id):
        """接受任务"""
        quest = self.get_quest(quest_id)

        # 检查前置条件
        if not await self.check_prerequisites(player, quest):
            return False

        # 添加到玩家任务列表
        player.quests[quest_id] = {
            "status": "in_progress",
            "progress": {},
            "start_time": time.time()
        }

        # 播放接取任务特效
        await self.play_quest_effect(player, "accept", quest_id)

        return True

    async def update_quest_progress(self, player, quest_id, objective_type, target_id, count=1):
        """更新任务进度"""
        quest_data = player.quests.get(quest_id)
        if not quest_data or quest_data["status"] != "in_progress":
            return

        quest = self.get_quest(quest_id)

        # 找到对应的目标
        for objective in quest.objectives:
            if objective.type == objective_type and objective.target == target_id:
                current = quest_data["progress"].get(objective.id, 0)
                new_count = min(current + count, objective.required)

                quest_data["progress"][objective.id] = new_count

                # 检查是否完成
                if new_count >= objective.required:
                    await self.on_objective_complete(player, quest_id, objective.id)

                break

        # 检查任务是否完成
        await self.check_quest_complete(player, quest_id)

    async def check_quest_complete(self, player, quest_id):
        """检查任务是否完成"""
        quest_data = player.quests.get(quest_id)
        quest = self.get_quest(quest_id)

        # 检查所有目标是否完成
        all_complete = True
        for objective in quest.objectives:
            current = quest_data["progress"].get(objective.id, 0)
            if current < objective.required:
                all_complete = False
                break

        if all_complete:
            await self.complete_quest(player, quest_id)

    async def complete_quest(self, player, quest_id):
        """完成任务"""
        quest = self.get_quest(quest_id)
        quest_data = player.quests[quest_id]

        # 发放奖励
        for reward in quest.rewards:
            if reward.type == "item":
                await self.give_item(player, reward.item_id, reward.count)
            elif reward.type == "exp":
                await self.give_exp(player, reward.exp)

        # 更新状态
        quest_data["status"] = "completed"

        # 播放完成特效
        await self.play_quest_effect(player, "complete", quest_id)

        # 检查是否有后续任务
        if quest.next_quest:
            await self.offer_next_quest(player, quest.next_quest)
```

### 3.3 剧情系统

```python
# 协程实现剧情对话

class DialogSystem:
    """对话系统"""

    async def play_dialog(self, player, dialog_id):
        """播放对话"""
        dialog = self.get_dialog(dialog_id)

        for line in dialog.lines:
            # 显示对话
            await self.show_dialog_line(player, line.speaker, line.text)

            # 等待玩家点击继续
            await self.wait_for_continue(player)

            # 可选: 显示选项
            if line.options:
                choice = await self.show_dialog_options(player, line.options)
                await self.handle_dialog_choice(player, dialog_id, choice)

        # 对话结束
        await self.on_dialog_complete(player, dialog_id)

    async def show_dialog_line(self, player, speaker, text):
        """显示对话行"""
        # 发送消息到客户端
        player.client.showDialogLine({
            "speaker": speaker,
            "text": text
        })

        # 打字机效果
        for i in range(len(text)):
            player.client.updateDialogText(text[:i+1])
            await asyncio.sleep(0.05)  # 每个字符延迟

    async def wait_for_continue(self, player):
        """等待玩家点击继续"""
        future = asyncio.Future()

        # 注册回调
        player.on_dialog_continue = lambda: future.set_result(True)

        # 等待
        await future

        # 清理
        player.on_dialog_continue = None

    async def show_dialog_options(self, player, options):
        """显示对话选项"""
        player.client.showDialogOptions(options)

        future = asyncio.Future()

        # 等待玩家选择
        player.on_dialog_choice = lambda choice: future.set_result(choice)

        choice = await future
        player.on_dialog_choice = None

        return choice
```

---

## 四、协程 vs 回调

### 4.1 代码对比

```python
# 回调地狱
def on_player_attack_callback(player, target):
    def check_distance_callback(in_range):
        if not in_range:
            return

        def play_animation_callback(success):
            def apply_damage_callback(damage):
                def on_confirm_callback(confirmed):
                    print(f"Damage confirmed: {damage}")
                confirm_damage(target, damage, on_confirm_callback)

            calculate_damage(player, target, apply_damage_callback)

        play_animation(player, "attack", play_animation_callback)

    check_distance(player, target, check_distance_callback)

# 协程版本
async def on_player_attack_coroutine(player, target):
    in_range = await check_distance_async(player, target)
    if not in_range:
        return

    await play_animation_async(player, "attack")
    damage = await calculate_damage_async(player, target)
    confirmed = await confirm_damage_async(target, damage)
    print(f"Damage confirmed: {confirmed}")
```

---

## 五、协程调度器

### 5.1 游戏协程调度器

```cpp
// C++ 游戏协程调度器

class CoroutineScheduler {
public:
    struct Coroutine {
        std::coroutine_handle<> handle;
        uint32_t priority;
        bool active;
    };

    void schedule(std::coroutine_handle<> coro, uint32_t priority = 0) {
        coroutines_.push_back({coro, priority, true});
    }

    void update(uint32 deltaTime) {
        // 按优先级排序
        std::sort(coroutines_.begin(), coroutines_.end(),
            [](const auto& a, const auto& b) {
                return a.priority > b.priority;
            });

        // 执行所有活跃协程
        for (auto& coro : coroutines_) {
            if (coro.active) {
                coro.handle.resume();

                if (coro.handle.done()) {
                    coro.active = false;
                    coro.handle.destroy();
                }
            }
        }

        // 清理完成的协程
        coroutines_.erase(
            std::remove_if(coroutines_.begin(), coroutines_.end(),
                [](const auto& c) { return !c.active; }),
            coroutines_.end()
        );
    }

private:
    std::vector<Coroutine> coroutines_;
};

// 使用示例
Task<void> playerMove(Player* player, Vector3 target) {
    while (distance(player->position, target) > 0.1f) {
        Vector3 dir = normalize(target - player->position);
        player->position += dir * 0.1f;

        co_await std::suspend_always{};  // 暂停，下一帧继续
    }
}
```

---

## 六、最佳实践

### 6.1 协程使用建议

| 场景 | 适用 | 不适用 |
|------|------|--------|
| **异步 IO** | ✅ | - |
| **技能系统** | ✅ | - |
| **剧情对话** | ✅ | - |
| **任务流程** | ✅ | - |
| **AI 行为** | ✅ | - |
| **密集计算** | - | ✅ (用线程池) |

### 6.2 注意事项

```
协程注意事项:

1. 避免在协程中使用阻塞操作
2. 协程是单线程执行，注意 CPU 占用
3. 合理使用超时机制
4. 注意协程生命周期管理
5. 异常处理要完善
```

---

## 七、总结

### 协程在游戏服务器中的应用

```
协程应用 = 异步操作简化 + 逻辑流程清晰 + 易于维护
- 技能系统
- 任务系统
- 对话系统
- AI 行为

优势: 代码像同步一样写，实际是异步执行
```

---

## 参考资料

- [Python asyncio](https://docs.python.org/3/library/asyncio.html)
- [C++20 Coroutines](https://en.cppreference.com/w/cpp/language/coroutines)
- [Lua Coroutines](https://www.lua.org/manual/5.4/manual.html#2.6)
