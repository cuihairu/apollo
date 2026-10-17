
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q106: What is the hardest technical problem you have encountered? How did you solve it?

## Problem Analysis

This question examines problem-solving ability:
- Problem localization
- Analysis approach
- Solution
- Experience summary

---

## 1. Memory Leak Problem

### 1.1 Problem Description

```
Problem symptoms:
- Server memory keeps growing after 12 hours of running
- Grows from 2GB to 16GB, then crashes with OOM
- Only appears during specific events

Impact scope:
- Players disconnect
- Data loss
- Requires frequent restarts
```

### 1.2 Troubleshooting Process

```python
# Memory leak troubleshooting

import gc
import sys
import objgraph

class MemoryLeakDebugger:
    """内存泄漏调试器 (memory leak debugger)"""

    def __init__(self):
        self.snapshots = []

    def take_snapshot(self, label=""):
        """获取内存快照 (take a memory snapshot)"""
        import sys
        import gc

        gc.collect()

        snapshot = {
            'label': label,
            'objects': len(gc.get_objects()),
            'ref_counts': {}
        }

        # 统计各类对象数量 (count objects of each type)
        for obj in gc.get_objects():
            type_name = type(obj).__name__
            snapshot['ref_counts'][type_name] = \
                snapshot['ref_counts'].get(type_name, 0) + 1

        self.snapshots.append(snapshot)

        return snapshot

    def compare_snapshots(self, idx1, idx2):
        """比较两个快照 (compare two snapshots)"""
        s1 = self.snapshots[idx1]
        s2 = self.snapshots[idx2]

        print(f"Comparing {s1['label']} -> {s2['label']}")
        print(f"Total objects: {s1['objects']} -> {s2['objects']}")

        # 找出增长最多的类型 (find the types with the most growth)
        growth = {}
        for type_name, count in s2['ref_counts'].items():
            old_count = s1['ref_counts'].get(type_name, 0)
            growth[type_name] = count - old_count

        # 排序输出 (output sorted)
        for type_name, delta in sorted(growth.items(),
                                       key=lambda x: x[1],
                                       reverse=True)[:10]:
            if delta > 0:
                print(f"  {type_name}: +{delta}")

    def find_leaking_refs(self, obj_type):
        """查找泄漏对象的引用链 (find the reference chain of leaking objects)"""
        import gc

        for obj in gc.get_objects():
            if type(obj).__name__ == obj_type:
                # 获取引用链 (get the reference chain)
                referrers = gc.get_referrers(obj)
                print(f"Object {obj} referenced by:")
                for ref in referrers:
                    print(f"  {type(ref).__name__}: {ref}")

# KBEngine 内存泄漏案例 (KBEngine memory leak case)
"""
问题 (Problem): Player 实体被销毁后，事件监听器仍然持有引用
(After the Player entity is destroyed, the event listener still holds a reference)

根本原因 (Root cause):
1. EventManager 中注册的回调持有 Player 引用
   (Callbacks registered in EventManager hold a Player reference)
2. Player 销毁时没有取消注册 (Player is not unregistered on destruction)
3. EventManager 是全局单例，永不被回收
   (EventManager is a global singleton and is never collected)

解决方案 (Solution):
1. 实现 onDestroy 生命周期 (implement the onDestroy lifecycle)
2. 自动清理事件监听 (automatically clean up event listeners)
3. 使用弱引用 (weakref) (use weak references)
"""

class EventManager:
    """修复后的事件管理器 (fixed event manager)"""

    def __init__(self):
        self.listeners = {}

    def subscribe(self, event_type, callback, owner=None):
        """订阅事件 (subscribe to an event)"""
        if event_type not in self.listeners:
            self.listeners[event_type] = []

        # 使用弱引用 (use a weak reference)
        import weakref
        if owner:
            owner_ref = weakref.ref(owner)
            self.listeners[event_type].append({
                'callback': callback,
                'owner': owner_ref
            })
        else:
            self.listeners[event_type].append({
                'callback': callback,
                'owner': None
            })

    def publish(self, event_type, *args, **kwargs):
        """发布事件 (publish an event)"""
        if event_type not in self.listeners:
            return

        # 清理已销毁的监听器 (clean up destroyed listeners)
        self.listeners[event_type] = [
            listener for listener in self.listeners[event_type]
            if listener['owner'] is None or listener['owner']() is not None
        ]

        # 调用回调 (invoke callbacks)
        for listener in self.listeners[event_type]:
            if listener['owner']:
                owner = listener['owner']()
                if owner is not None:
                    listener['callback'](owner, *args, **kwargs)
            else:
                listener['callback'](*args, **kwargs)
```

---

## 2. Deadlock Problem

### 2.1 Problem Description

```
Problem symptoms:
- Server periodically freezes for about 30 seconds
- CPU usage is normal
- Player connections drop
- Logs show "Waiting for lock..."

Trigger conditions:
- Frequent during peak hours
- Related to specific operations
```

### 2.2 Solution

```cpp
// 死锁问题解决 (deadlock problem solution)

// 问题代码 (problem code)
class DatabaseManager {
    std::mutex player_mutex;
    std::mutex guild_mutex;

    void updatePlayerGuild(int playerId) {
        std::lock_guard<std::mutex> lock1(player_mutex);
        // ... 读取玩家数据 (read player data)

        std::lock_guard<std::mutex> lock2(guild_mutex);
        // ... 更新公会数据 (update guild data)
    }

    void updateGuildPlayers(int guildId) {
        std::lock_guard<std::mutex> lock1(guild_mutex);
        // ... 读取公会数据 (read guild data)

        std::lock_guard<std::mutex> lock2(player_mutex);
        // ... 更新玩家数据 (update player data)
    }
};

// 死锁原因 (deadlock cause):
// 1. 线程 A: updatePlayerGuild 获取 player_mutex，等待 guild_mutex
//    (Thread A: updatePlayerGuild acquires player_mutex, waits for guild_mutex)
// 2. 线程 B: updateGuildPlayers 获取 guild_mutex，等待 player_mutex
//    (Thread B: updateGuildPlayers acquires guild_mutex, waits for player_mutex)
// 3. 互相等待，形成死锁 (they wait for each other, forming a deadlock)

// 解决方案 1: 统一加锁顺序 (solution 1: unify the lock ordering)
class FixedDatabaseManager {
    std::mutex player_mutex;
    std::mutex guild_mutex;
    std::mutex order_mutex;  // 用于保证加锁顺序 (used to guarantee lock ordering)

    void updatePlayerGuild(int playerId) {
        // 按固定顺序加锁: 先 player 后 guild
        // (lock in a fixed order: player first, then guild)
        std::lock(player_mutex, guild_mutex);
        std::lock_guard<std::mutex> lock1(player_mutex, std::adopt_lock);
        std::lock_guard<std::mutex> lock2(guild_mutex, std::adopt_lock);

        // ... 业务逻辑 (business logic)
    }

    void updateGuildPlayers(int guildId) {
        // 同样的顺序 (the same order)
        std::lock(player_mutex, guild_mutex);
        std::lock_guard<std::mutex> lock1(player_mutex, std::adopt_lock);
        std::lock_guard<std::mutex> lock2(guild_mutex, std::adopt_lock);

        // ... 业务逻辑 (business logic)
    }
};

// 解决方案 2: 使用 std::scoped_lock (C++17)
// (solution 2: use std::scoped_lock (C++17))
class ModernDatabaseManager {
    std::mutex player_mutex;
    std::mutex guild_mutex;

    void updatePlayerGuild(int playerId) {
        // 同时锁定多个互斥量，避免死锁
        // (lock multiple mutexes at once to avoid deadlock)
        std::scoped_lock lock(player_mutex, guild_mutex);

        // ... 业务逻辑 (business logic)
    }

    void updateGuildPlayers(int guildId) {
        std::scoped_lock lock(player_mutex, guild_mutex);

        // ... 业务逻辑 (business logic)
    }
};

// 解决方案 3: 超时锁 (solution 3: timeout lock)
class TimeoutDatabaseManager {
    std::mutex player_mutex;
    std::mutex guild_mutex;

    bool updatePlayerGuild(int playerId) {
        // 尝试加锁，超时则返回失败
        // (try to lock; return failure on timeout)
        if (!try_lock_for(player_mutex, std::chrono::seconds(5))) {
            ERROR_MSG("Failed to acquire player_mutex");
            return false;
        }

        std::unique_lock<std::mutex> lock1(player_mutex, std::adopt_lock);

        if (!try_lock_for(guild_mutex, std::chrono::seconds(5))) {
            ERROR_MSG("Failed to acquire guild_mutex");
            return false;
        }

        std::unique_lock<std::mutex> lock2(guild_mutex, std::adopt_lock);

        // ... 业务逻辑 (business logic)
        return true
    }

private:
    template<typename Mutex, typename Duration>
    bool try_lock_for(Mutex& m, const Duration& duration) {
        std::unique_lock<Mutex> lock(m, std::try_to_lock);
        if (lock.owns_lock()) {
            lock.release();
            return true;
        }
        return false;
    }
};
```

---

## 3. Network Sync Problem

### 3.1 Problem Description

```
Problem symptoms:
- Players report position jumping
- Skill casts have no effect
- Client and server states are inconsistent

Reproduction steps:
- Appears under high-latency environments
- More obvious when moving fast
```

### 3.2 Solution

```python
# 网络同步修复 (network sync fix)

# 问题 (Problem): 客户端预测和服务端校验不一致 (client prediction and server validation are inconsistent)

class PositionSync:
    """位置同步 (修复版) (position sync, fixed version)"""

    # 时间同步补偿 (time sync compensation)
    SERVER_CLIENT_DELAY = 0.1  # 100ms

    def __init__(self, entity):
        self.entity = entity
        self.pending_moves = []
        self.last_confirmed_pos = (0, 0, 0)
        self.client_time_offset = 0

    def on_client_move(self, client_pos, client_time):
        """客户端移动 (client move)"""
        # 计算时间偏移 (compute the time offset)
        server_time = time.time()
        if not self.client_time_offset:
            self.client_time_offset = server_time - client_time

        # 校正后的客户端时间 (adjusted client time)
        adjusted_time = client_time + self.client_time_offset

        # 保存待验证移动 (save the pending move)
        self.pending_moves.append({
            'pos': client_pos,
            'time': adjusted_time,
            'sequence': len(self.pending_moves)
        })

        # 服务端预测 (server-side prediction)
        self.validate_and_correct()

    def validate_and_correct(self):
        """验证并校正 (validate and correct)"""
        if not self.pending_moves:
            return

        # 取最早的待验证移动 (take the earliest pending move)
        move = self.pending_moves[0]

        # 速度检测 (speed check)
        distance = self.calculate_distance(
            self.last_confirmed_pos,
            move['pos']
        )

        time_delta = move['time'] - time.time()

        # 速度限制 (假设最大速度 10m/s)
        # (speed limit, assuming max speed of 10m/s)
        max_speed = 10.0
        max_distance = max_speed * abs(time_delta)

        if distance > max_distance:
            # 超速，拒绝移动 (over speed limit, reject the move)
            WARNING_MSG(f"Player {self.entity.id} speed hack detected")

            # 发送校正位置 (send the corrected position)
            self.send_position_correction(
                self.last_confirmed_pos
            )

            self.pending_moves.clear()
            return

        # 验证通过，确认移动 (validation passed, confirm the move)
        self.last_confirmed_pos = move['pos']
        self.entity.position = move['pos']

        # 广播给附近玩家 (broadcast to nearby players)
        self.broadcast_position(move['pos'])

        # 移除已处理的移动 (remove the processed move)
        self.pending_moves.pop(0)

    def send_position_correction(self, correct_pos):
        """发送位置校正 (send position correction)"""
        if hasattr(self.entity, 'client'):
            self.entity.client.onPositionCorrection({
                'position': correct_pos,
                'server_time': time.time()
            })

    def broadcast_position(self, pos):
        """广播位置 (broadcast position)"""
        # 获取 AOI 内的玩家 (get players within AOI)
        nearby_players = self.entity.getAOIPlayers()

        for player in nearby_players:
            if hasattr(player, 'client') and player.client:
                player.client.onEntityPosition({
                    'entity_id': self.entity.id,
                    'position': pos
                })

    @staticmethod
    def calculate_distance(pos1, pos2):
        """计算距离 (calculate distance)"""
        import math
        dx = pos1[0] - pos2[0]
        dy = pos1[1] - pos2[1]
        dz = pos1[2] - pos2[2]
        return math.sqrt(dx*dx + dy*dy + dz*dz)
```

---

## 4. Summary

### 4.1 Problem-Solving Methodology

```mermaid
flowchart TD
    A[发现问题 (discover problem)] --> B[复现问题 (reproduce problem)]
    B --> C[定位根因 (locate root cause)]
    C --> D[设计方案 (design solution)]
    D --> E[实现修复 (implement fix)]
    E --> F[验证效果 (verify effect)]
    F -->|有效 (effective)| G[总结经验 (summarize experience)]
    F -->|无效 (ineffective)| C
    G --> H[文档记录 (document)]
```

| Stage | Method |
|------|------|
| Discovery | Monitoring alerts, log analysis |
| Reproduction | Minimized scenarios, stress testing |
| Localization | gdb/valgrind/pdb |
| Solution | Comparing multiple solutions |
| Verification | Unit tests, regression tests |
| Summary | Documentation, sharing |

---

## References

- [Debugging Memory Leaks](https://docs.python.org/3/library/tracemalloc.html)
- [Deadlock Detection](https://preshing.com/20120612/an-introduction-to-lock-free-programming/)
- [Network Compensation](https://www.gabrielgambetta.com/client-side-prediction-lockstep.html)
