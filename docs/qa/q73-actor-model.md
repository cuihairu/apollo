# Q73: Actor 模型是什么？有什么优势？

## 问题分析

本题考察对 Actor 并发模型的理解：
- Actor 模型的核心概念
- 消息传递机制
- 在游戏服务器中的应用
- 与传统多线程模型的对比

---

## 一、Actor 模型基础

### 1.1 核心概念

```
┌─────────────────────────────────────────────────────────────┐
│                    Actor 模型核心                               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Actor = 实体 + 邮箱 (Mailbox)                            │
│                                                             │
│  ┌─────────────────────────────────────────────────┐       │
│  │  Actor                                            │       │
│  │  │  ┌───────────────────────────────────┐  │       │
│  │  │  │  Mailbox (消息队列)              │  │       │
│  │  │  └───────────────────────────────────┘  │       │
│  │  │                                              │       │
│  │  │  ┌───────────────────────────────────┐  │       │
│  │  │  │  Behavior (状态+行为)           │  │       │
│  │  │  │  - processMessages()              │  │       │
│  │  │  │  - handleMessage()             │  │       │
│  │  │  └───────────────────────────────────┘  │       │
│  │  └─────────────────────────────────────────┘       │
│  │                                                     │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  特点：                                                    │
│  ├── 每个 Actor 串行处理自己的消息                        │
│  ├── Actor 之间通过消息通信                              │
│  ├── 无锁竞争（每个 Actor 独立队列）                     │
│  └── 易于并发和分布式                                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 Actor 消息

```
┌─────────────────────────────────────────────────────────────┐
│                    Actor 消息类型                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 普通消息                                                │
│     ├── 异步发送                                            │
│     ├── 可能乱序到达                                        │
│     └── 需要保证顺序时使用序列号                          │
│                                                             │
│  2. 请求/响应消息                                            │
│     ├── 期待回复                                            │
│     ├── 使用 Future/Promise 模式                              │
│     └── 支持超时                                            │
│                                                             │
│  3. 系统消息                                                │
│     ├── 系统控制                                            │
│     ├── 启动/停止                                            │
│     └── 监控                                                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、Actor 实现

### 2.1 Actor 基础类

```cpp
// Actor 基础实现

class Actor {
public:
    // Actor ID
    using ID = uint64_t;

    // 构造函数
    Actor(ID id, std::string name)
        : id_(id), name_(name), running_(true) {
        // 创建消息队列线程
        thread_ = std::thread(&Actor::messageLoop, this);
    }

    // 析构函数
    ~Actor() {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            running_ = false;
            queueCV_.notify_one();
        }
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    // 发送消息
    template<typename T>
    void send(ID targetId, const T& message) {
        // 查找目标 Actor
        Actor* target = ActorSystem::getActor(targetId);
        if (target) {
            target->receive(message);
        }
    }

    // 接收消息
    template<typename T>
    void receive(const T& message) {
        // 创建消息包装
        auto* msg = new Message::type(message);
        msg->sender = this->id_;
        msg->data = message;

        // 加入队列
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            messageQueue_.push(msg);
            queueCV_.notify_one();
        }
    }

protected:
    // 消息循环（在独立线程中运行）
    void messageLoop() {
        while (running_) {
            Message* msg = nullptr;

            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                queueCV_.wait(lock, [this] {
                    return !messageQueue_.empty() || !running_;
                });

                if (!running_) break;

                if (!messageQueue_.empty()) {
                    msg = messageQueue_.front();
                    messageQueue_.pop();
                }
            }

            if (msg) {
                handleMessage(msg);
                delete msg;
            }
        }
    }

    // 处理消息（子类实现）
    virtual void handleMessage(Message* msg) = 0;

private:
    struct Message {
        ID sender;
        std::any data;
        virtual void process() = 0;
    };

    std::thread thread_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;
    std::queue<Message*> messageQueue_;
    bool running_;
    std::string name_;
    ID id_;
};
```

### 2.2 Actor 系统

```cpp
// Actor 管理器

class ActorSystem {
public:
    static ActorSystem& instance() {
        static ActorSystem inst;
        return inst;
    }

    // 注册 Actor
    void register(Actor* actor) {
        std::lock_guard<std::mutex> lock(actorsMutex_);
        actors_[actor->id()] = actor;
    }

    // 获取 Actor
    Actor* getActor(Actor::ID id) {
        std::lock_guard<std::mutex> lock(actorsMutex_);
        auto it = actors_.find(id);
        return it != actors_.end() ? it->second : nullptr;
    }

    // 广播消息到所有 Actor
    template<typename T>
    void broadcast(const T& message) {
        std::lock_guard<std::mutex> lock(actorsMutex_);
        for (auto& [id, actor] : actors_) {
            actor->receive(message);
        }
    }

    // 创建 Actor
    template<typename T, typename... Args>
    T* create(Args&&... args) {
        auto* actor = new T(std::forward<Args>(args)...);
        register(actor);
        return actor;
    }

private:
    std::unordered_map<Actor::ID, Actor*> actors_;
    std::mutex actorsMutex_;
    Actor::ID nextId_ = 1;
};

// 全局访问函数
Actor::ID createActorID() {
    return ActorSystem::instance().nextId++;
}
```

---

## 三、Actor 模式 vs 多线程

### 3.1 对比分析

```
┌─────────────────────────────────────────────────────────────┐
│              Actor 模式 vs 传统多线程                         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  传统多线程：                                              │
│  ┌─────────────────────────────────────────────────┐       │
│  │  共享数据结构                                       │       │
│  │  ┌─────────────────────────────────────────┐   │       │
│  │  │  std::map<EntityID, Entity*>      │   │       │
│  │  │  ┌──┬──┬──┬──┬──┬┐        │   │       │
│  │  │  │E1 │E2 │E3 │E4 │... │       │       │
│  │  │  └──┴──┴──┴──┴──┘        │   │       │
│  │  └─────────────────────────────────────────┘   │       │
│  │  │        ▲                    │   │       │
│  │  │   ┌────┴────┴─────┐        │   │       │
│  │  │   │  锁竞争           │        │   │       │
│  │  │   └────┬────┴─────┘        │   │       │
│  │  └─────────────────────────────────────┘       │       │
│  │                                                     │       │
│  ┌─────────────────────────────────────────────────┐       │
│  │  线程1  │  线程2  │  线程3  │        │       │
│  │   │      │   │      │   │     │       │
│  │   └──────┴──────┴─────────────┘        │       │
│  │        │  │  │  │                         │       │
│  │        └──┴──────────────┘          │       │
│  │                                                     │       │
│  问题：                                                │       │
│  ├── 锁竞争                                             │       │
│  ├── 上下文切换                                         │       │
│  ├── 数据竞争                                           │       │
│  └── 难以调试                                           │       │
│                                                             │
└─────────────────────────────────────────────────────────────┘       │
│                                                             │
│  Actor 模式：                                              │
│  ┌─────────────────────────────────────────────────┐       │
│  │  Actor1  │  Actor2  │  Actor3  │  Actor4   │       │
│  │  │  ┌──┴──┐  │  ┌──┴──┐  │  │  ┌──┴┐     │       │
│  │  │  │Queue1 │  │  │Queue2 │  │  │Queue4   │       │
│  │  │  └──────┘  │  └──────┘  │  │ └──────┘   │       │
│  │  │         │  │        │  │  │           │       │
│  │  │  ▼       │  │        │  │  │           │       │
│  │  │ 串行处理  │  │  │        │  │  │           │       │
│  │  │  └────────┴─────────────────────┘  │ │           │       │
│  │  │            无锁竞争            │  │           │       │
│  │  │            高并发               │  │           │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘       │
```

### 3.2 性能对比

| 维度 | 传统多线程 | Actor 模型 |
|------|-----------|----------|
| **并发模型** | 共享内存 | 消息传递 |
| **锁竞争** | 严重 | 无锁 |
| **扩展性** | 受限于单机 | 易于分布式 |
| **调试难度** | 高 | 中 |
| **容错性** | 难 | 好 |

---

## 四、游戏服务器中的 Actor 应用

### 4.1 实体作为 Actor

```cpp
// 游戏实体作为 Actor

class EntityActor : public Actor {
public:
    EntityActor(EntityID id)
        : Actor(id, "Entity") {
    }

    // 处理移动消息
    void onMoveMessage(const MoveMessage& msg) {
        // 更新位置
        position_ = msg.newPosition;

        // 通知 AOI 系统
        notifyAOIUpdate(this);

        // 广播给附近的玩家
        broadcastToAOI(MoveBroadcastMessage{
            entityId = id_,
            oldPosition = oldPosition,
            newPosition = msg.newPosition
        });
    }

    // 处理攻击消息
    void onAttackMessage(const AttackMessage& msg) {
        EntityID targetId = msg.targetId;

        // 发送攻击请求到 CellApp
        auto* cellApp = getCellAppForEntity(id_);
        cellApp->sendAttackRequest(id, targetId);

        // 等待结果
        pendingAttacks_[msg.sequence] = msg;
    }

    // 处理伤害结果
    void onDamageResult(const DamageResult& result) {
        // 更新血量
        hp_ -= result.damage;

        if (hp_ <= 0) {
            onDeath();
        }

        // 通知客户端
        sendToClient(DamageNotification{
            entityId = id_,
            damage = result.damage,
            newHp = hp_
        });
    }

private:
    Position position_;
    float hp_;
    std::unordered_map<uint32_t, AttackMessage> pendingAttacks_;
};
```

### 4.2 系统作为 Actor

```cpp
// 系统服务 Actor

class CombatSystemActor : public Actor {
public:
    CombatSystemActor(ActorID id)
        : Actor(id, "CombatSystem") {
    }

    // 处理战斗匹配
    void onMatchRequest(const MatchRequest& request) {
        // 查找合适的战斗服务器
        CellApp* target = findBestCellApp();

        if (target) {
            // 创建战斗
            BattleInstance battle = createBattle(request.players);

            // 通知所有玩家
            for (auto player : request.players) {
                send(player.actorId, BattleReadyMessage{
                    battleId = battle.id,
                    serverAddr = target->getAddress()
                });
            }
        }
    }

    // 战斗循环
    void onBattleLoop(const BattleTick& tick) {
        // 更新战斗状态
        Battle* battle = getBattle(tick.battleId);

        // 处理逻辑
        battle->update(tick.deltaTime);

        // 检查战斗是否结束
        if (battle->isFinished()) {
            endBattle(battle);
        }
    }
};
```

---

## 五、分布式 Actor

### 5.1 跨节点 Actor

```
┌─────────────────────────────────────────────────────────────┐
│               分布式 Actor 系统                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   Server 1                   Server 2                   │
│  ┌───────────┐  ┌─────────────┐                    │
│  │ Player A  │  │  │ NPC X     │                    │
│  │  Actor   │  │  │ Actor     │                    │
│  │         │  └─────────────┘                    │
│  └───────────┘  └─────────────┘                    │
│       │                │                                │
│       │                │  NNG (REQ/REP)             │
│       │                │                                │
│       ▼                ▼                                │
│   ┌─────────────────────────────────────┐                     │
│   │       Actor Registry (Redis)          │                     │
│   │  - Actor 注册                   │                     │
│   │  - 服务发现                   │                     │
│   │  - 负载均衡                   │                     │
│   └─────────────────────────────────────┘                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 Actor 注册

```cpp
// Actor 注册表 (Redis)

class ActorRegistry {
public:
    // 注册 Actor
    bool register(const std::string& name,
                     const std::string& address,
                     uint16_t port) {
        std::string key = "actor:" + name;

        // 使用 Redis Hash 存储
        redis_->hset(key, "name", name);
        redis_->hset(key, "address", address);
        redis_->hset(key, "port", port);

        // 设置过期时间
        redis_->expire(key, 60);  // 60秒

        return true;
    }

    // 发现 Actor
    std::string find(const std::string& name) {
        std::string key = "actor:" + name;
        auto addr = redis_->hget(key, "address");

        if (!addr.empty()) {
            std::string port = redis_->hget(key, "port");
            return addr + ":" + port;
        }

        return "";
    }

    // 心跳保持
    void heartbeat(const std::string& name) {
        std::string key = "actor:" + name;
        redis_->expire(key, 60);  // 刷新过期时间
    }
};
```

---

## 六、最佳实践

### 6.1 Actor 设计原则

```
┌─────────────────────────────────────────────────────────────┐
│                  Actor 设计原则                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 单一职责                                              │
│     ├── 每个 Actor 只做一件事                               │
│     ├── 避免上帝对象                                       │
│     └── 保持简单，专注                                       │
│                                                             │
│  2. 消息不可变                                              │
│     ├── 消息创建后不可修改                                 │
│     ├── 避免共享状态                                       │
│     └── 保持幂等                                           │
│                                                             │
│  3. 错误处理                                                │
│     ├── 监督策略 (Let It Crash)                             │
│     ├── 隔离机制 (Bulkhead)                                │
│     └── 重试机制                                           │
│                                                             │
│  4. 有限状态机                                              │
│     ├── 状态转换明确                                       │
│     ├── 避免过多状态                                       │
│     └── 状态转换可观测                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘       │
```

### 6.2 常见陷阱

```
┌─────────────────────────────────────────────────────────────┐
│                  Actor 常见陷阱                               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ❌ 避免的做法：                                             │
│     ├── Actor 之间直接调用方法                                 │
│     │   └─ 不通过消息，破坏模型                       │
│     ├── 共享可变状态                                       │
│     │   └─ 导致数据竞争                               │
│     ├── 阻塞消息处理                                         │
│     │   └─ 阻塞整个 Actor                           │
│     └── 包含大量数据                                         │
│         └─ └─ 导致消息传递慢                           │
│                                                             │
│  ✅ 推荐的做法：                                           │
│     ├── 所有交互通过消息                                   │
│     ├── 消息使用不可变类型                                 │
│     ├── 快速处理消息                                         │
│     ├── 使用异步消息                                         │
│     └── 数据存储在外部                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘       │
```

---

## 七、总结

### Actor 模型总结

| 优势 | 说明 | 适用场景 |
|------|------|----------|
| **无锁竞争** | 每个 Actor 独立队列 | 高并发 |
| **易扩展** | Actor 可分布式 | 分布式系统 |
| **容错好** | Actor 隔离故障 | 高可用 |
| **测试简单** | 每个 Actor 独立测试 | 单元测试 |
| **延迟高** | 消息传递开销 | 实时性要求低的场景 |

### 何时使用 Actor

```
✅ 推荐使用 Actor 的场景：
- 大量独立实体（玩家、NPC、怪物）
- 需要高并发处理
- 分布式部署
- 容错性要求高

❌ 不推荐使用 Actor 的场景：
- 计算密集型任务（单线程更快）
- 需要大量共享状态
- 简单的 CRUD 操作
- 实时性要求极高的场景
```

---

## 参考资料

- [Actor Model 论文](https://www.semanticscholar.org/paper/7210807/)
- [Erlang/OTP 实现参考](https://www.erlang.org/doc/getting_started/concurrency.html)
- [Akka.NET Actor 模型](https://getakka.net/)
